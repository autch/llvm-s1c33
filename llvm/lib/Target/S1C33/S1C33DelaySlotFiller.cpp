//===-- S1C33DelaySlotFiller.cpp - S1C33 Delay Slot Filler ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass fills delay slots for delayed branch/call/return instructions
// on the S1C33 architecture.
//
// S1C33 delay slot rules (CPU manual §4.x):
//   - The instruction immediately following a delayed branch executes before
//     the branch takes effect.
//   - Eligible delayed instructions: ret.d, call.d %rb, jp.d sign8.
//   - FORBIDDEN: jp.d %rb — hardware DMA bug (DESIGN_SPEC §4.3 / errata §1).
//   - Delay slot constraints:
//       • Must be a 1-cycle instruction.
//       • Must NOT be a memory access (load/store).
//       • Must NOT use an ext prefix (size > 2 bytes).
//       • Must NOT be a branch or call.
//
// Strategy:
//   For each RET / CALL_r / JP_i instruction in the function:
//     1. Search backward for a legal delay-slot candidate.
//     2. If found, convert to the delayed variant and move the candidate into
//        the slot.
//     3. Otherwise, keep the non-delayed instruction.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/S1C33MCTargetDesc.h"
#include "S1C33InstrInfo.h"
#include "S1C33Subtarget.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "s1c33-delay-slot-filler"

namespace {

class S1C33DelaySlotFiller : public MachineFunctionPass {
public:
  static char ID;
  S1C33DelaySlotFiller() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "S1C33 Delay Slot Filler";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  MachineBasicBlock::iterator
  findDelaySlotCandidate(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                         const S1C33InstrInfo &TII) const;
  void fillDelaySlot(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                     MachineBasicBlock::iterator Candidate) const;
  bool isDelaySlotCandidate(const MachineInstr &MI) const;
  bool hasRegisterHazard(const MachineInstr &Candidate,
                         const MachineInstr &Other,
                         const TargetRegisterInfo &TRI) const;
  bool hasBranchHazard(const MachineInstr &Candidate, const MachineInstr &Branch,
                       const TargetRegisterInfo &TRI) const;
};

char S1C33DelaySlotFiller::ID = 0;

} // end anonymous namespace

static int getPushPopRangeLastRegIdx(const MachineInstr &MI);
static MCPhysReg getGPRByIdx(unsigned Idx);
static bool pushPopRangeTouchesRegister(const MachineInstr &MI, Register Reg,
                                        const TargetRegisterInfo &TRI);
static bool definesRegisterSemantically(const MachineInstr &MI, Register Reg,
                                        const TargetRegisterInfo &TRI);
static bool readsRegisterSemantically(const MachineInstr &MI, Register Reg,
                                      const TargetRegisterInfo &TRI);

// Returns true if MI can legally fill a delay slot.
// Constraints from the CPU manual §4.x and DESIGN_SPEC.md §4.3.
bool S1C33DelaySlotFiller::isDelaySlotCandidate(
    const MachineInstr &MI) const {
  const MachineFunction *MF = MI.getParent() ? MI.getParent()->getParent() : nullptr;
  const TargetRegisterInfo &TRI =
      *MF->getSubtarget<S1C33Subtarget>().getRegisterInfo();

  // Reject pseudo instructions.
  if (MI.isPseudo())
    return false;
  // Reject EXT prefix instructions — they modify the NEXT instruction and
  // must remain immediately before their target.
  if (MI.getOpcode() == S1C33::EXT)
    return false;
  // Reject terminators (branches, calls, returns).
  if (MI.isTerminator() || MI.isBranch() || MI.isCall() || MI.isReturn())
    return false;
  // Reject memory access instructions (load or store).
  if (MI.mayLoad() || MI.mayStore())
    return false;
  // Reject instructions with side effects (e.g. hasSideEffects).
  if (MI.hasUnmodeledSideEffects())
    return false;
  // Reject multi-cycle multiply instructions (D:- in the CPU manual).
  // mlt.w and mltu.w take 5 cycles; mac takes 2n+4 cycles.
  // Placing them in a delay slot would produce incorrect results.
  {
    unsigned Opc = MI.getOpcode();
    if (Opc == S1C33::MLT_W || Opc == S1C33::MLTU_W || Opc == S1C33::MAC)
      return false;
  }
  // Reject instructions that define SP.  ret.d reads [SP] to obtain the
  // return address, and call.d writes [SP-4] before branching.  If an
  // SP-modifying instruction (add %sp / sub %sp) is placed in the delay
  // slot, the branch instruction sees the OLD SP value, not the adjusted
  // one.  For ret.d this means reading from the wrong stack location
  // (e.g. outgoing arg area instead of return address → jump to 0).
  if (definesRegisterSemantically(MI, S1C33::SP, TRI))
    return false;
  // Reject instructions larger than 2 bytes (ext-prefixed instructions).
  // All standard 16-bit S1C33 instructions have Size==0 (default); only
  // pseudo ext sequences have Size > 0.  We treat any non-2-byte instruction
  // as ineligible.  At this late stage, pseudos have been expanded, so Size==0
  // for a real instruction means the 2-byte base form.
  // Note: MCInstrDesc::getSize() returns 0 for most real instructions.
  if (MI.getDesc().getSize() > 2)
    return false;
  return true;
}

static bool isMetaInstr(const MachineInstr &MI) {
  return MI.isDebugInstr() || MI.isCFIInstruction();
}

static int getPushPopRangeLastRegIdx(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case S1C33::PUSHN:
  case S1C33::POPN:
    break;
  default:
    return -1;
  }

  if (MI.getNumOperands() == 0 || !MI.getOperand(0).isReg())
    return -1;

  switch (MI.getOperand(0).getReg()) {
  case S1C33::R0:  return 0;
  case S1C33::R1:  return 1;
  case S1C33::R2:  return 2;
  case S1C33::R3:  return 3;
  case S1C33::R4:  return 4;
  case S1C33::R5:  return 5;
  case S1C33::R6:  return 6;
  case S1C33::R7:  return 7;
  case S1C33::R8:  return 8;
  case S1C33::R9:  return 9;
  case S1C33::R10: return 10;
  case S1C33::R11: return 11;
  case S1C33::R12: return 12;
  case S1C33::R13: return 13;
  case S1C33::R14: return 14;
  case S1C33::R15: return 15;
  default:
    return -1;
  }
}

static MCPhysReg getGPRByIdx(unsigned Idx) {
  static constexpr MCPhysReg GPRs[] = {
      S1C33::R0,  S1C33::R1,  S1C33::R2,  S1C33::R3,
      S1C33::R4,  S1C33::R5,  S1C33::R6,  S1C33::R7,
      S1C33::R8,  S1C33::R9,  S1C33::R10, S1C33::R11,
      S1C33::R12, S1C33::R13, S1C33::R14, S1C33::R15,
  };
  assert(Idx < std::size(GPRs) && "invalid GPR index");
  return GPRs[Idx];
}

static bool pushPopRangeTouchesRegister(const MachineInstr &MI, Register Reg,
                                        const TargetRegisterInfo &TRI) {
  int LastIdx = getPushPopRangeLastRegIdx(MI);
  if (LastIdx < 0)
    return false;

  for (int Idx = 0; Idx <= LastIdx; ++Idx) {
    if (TRI.regsOverlap(Reg, getGPRByIdx(Idx)))
      return true;
  }
  return false;
}

static bool definesRegisterSemantically(const MachineInstr &MI, Register Reg,
                                        const TargetRegisterInfo &TRI) {
  if (MI.definesRegister(Reg, &TRI))
    return true;

  switch (MI.getOpcode()) {
  case S1C33::POPN:
    return TRI.regsOverlap(Reg, S1C33::SP) ||
           pushPopRangeTouchesRegister(MI, Reg, TRI);
  case S1C33::PUSHN:
    return TRI.regsOverlap(Reg, S1C33::SP);
  default:
    return false;
  }
}

static bool readsRegisterSemantically(const MachineInstr &MI, Register Reg,
                                      const TargetRegisterInfo &TRI) {
  if (MI.readsRegister(Reg, &TRI))
    return true;

  switch (MI.getOpcode()) {
  case S1C33::PUSHN:
    return TRI.regsOverlap(Reg, S1C33::SP) ||
           pushPopRangeTouchesRegister(MI, Reg, TRI);
  case S1C33::POPN:
    return TRI.regsOverlap(Reg, S1C33::SP);
  default:
    return false;
  }
}

static MachineBasicBlock::iterator
prevNonMetaInstr(MachineBasicBlock &MBB, MachineBasicBlock::iterator I) {
  while (I != MBB.begin()) {
    --I;
    if (!isMetaInstr(*I))
      return I;
  }
  return MBB.end();
}

bool S1C33DelaySlotFiller::hasRegisterHazard(const MachineInstr &Candidate,
                                             const MachineInstr &Other,
                                             const TargetRegisterInfo &TRI) const {
  SmallVector<Register, 8> Defs;
  SmallVector<Register, 8> Uses;

  for (const MachineOperand &Op : Candidate.operands()) {
    if (!Op.isReg())
      continue;
    Register Reg = Op.getReg();
    if (!Reg)
      continue;
    if (Op.isDef())
      Defs.push_back(Reg);
    if (Op.readsReg())
      Uses.push_back(Reg);
  }

  for (Register Reg : Defs) {
    if (readsRegisterSemantically(Other, Reg, TRI) ||
        definesRegisterSemantically(Other, Reg, TRI))
      return true;
  }
  for (Register Reg : Uses) {
    if (definesRegisterSemantically(Other, Reg, TRI))
      return true;
  }
  return false;
}

bool S1C33DelaySlotFiller::hasBranchHazard(const MachineInstr &Candidate,
                                           const MachineInstr &Branch,
                                           const TargetRegisterInfo &TRI) const {
  switch (Branch.getOpcode()) {
  case S1C33::CALL_r:
  case S1C33::CALL_r_D: {
    Register TargetReg = Branch.getOperand(0).getReg();
    return Candidate.definesRegister(TargetReg, &TRI);
  }
  default:
    return false;
  }
}

MachineBasicBlock::iterator
S1C33DelaySlotFiller::findDelaySlotCandidate(MachineBasicBlock &MBB,
                                             MachineBasicBlock::iterator I,
                                             const S1C33InstrInfo &TII) const {
  const TargetRegisterInfo &TRI = TII.getRegisterInfo();

  SmallVector<MachineInstr *, 8> Intervening;
  for (auto Scan = prevNonMetaInstr(MBB, I); Scan != MBB.end();
       Scan = prevNonMetaInstr(MBB, Scan)) {
    auto Ext = prevNonMetaInstr(MBB, Scan);
    bool NeedsExt = Ext != MBB.end() && Ext->getOpcode() == S1C33::EXT;

    bool Safe =
        !NeedsExt && isDelaySlotCandidate(*Scan) && !hasBranchHazard(*Scan, *I, TRI);
    if (Safe) {
      for (MachineInstr *Mid : Intervening) {
        if (hasRegisterHazard(*Scan, *Mid, TRI)) {
          Safe = false;
          break;
        }
      }
    }

    if (Safe) {
      return Scan;
    }

    if (!isMetaInstr(*Scan))
      Intervening.push_back(&*Scan);
  }
  return MBB.end();
}

void S1C33DelaySlotFiller::fillDelaySlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
    MachineBasicBlock::iterator Candidate) const {
  MachineBasicBlock::iterator SlotPos = std::next(I);
  MBB.splice(SlotPos, &MBB, Candidate);
  LLVM_DEBUG(dbgs() << "  Filled delay slot with: " << *std::prev(SlotPos));
}

// A delayed branch (the bundle header) semantically executes AFTER its delay
// slot instruction.  When the delay slot defines a register the branch reads —
// e.g. RET_D reads $r10 (the return value) and the slot is the MOV that
// produces it — that value is produced *within* the bundle.  Mark such uses on
// the branch as internal reads: MachineOperand::readsReg() then returns false
// for them, so the machine verifier no longer flags them as reading an
// undefined physical register.  (Without this, `int f(int v){return v;}` fails
// `llc -verify-machineinstrs`, since the MOV that sets $r10 lands in the RET_D
// delay slot.)
static void markDelayedBranchInternalReads(MachineInstr &Branch,
                                           MachineInstr &Slot,
                                           const TargetRegisterInfo &TRI) {
  for (MachineOperand &MO : Branch.operands()) {
    if (!MO.isReg() || !MO.isUse() || !MO.getReg())
      continue;
    if (definesRegisterSemantically(Slot, MO.getReg(), TRI))
      MO.setIsInternalRead();
  }
}

bool S1C33DelaySlotFiller::runOnMachineFunction(MachineFunction &MF) {
  const S1C33Subtarget &STI = MF.getSubtarget<S1C33Subtarget>();
  const S1C33InstrInfo &TII =
      *static_cast<const S1C33InstrInfo *>(STI.getInstrInfo());
  const TargetRegisterInfo &TRI = *STI.getRegisterInfo();

  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end();
         I != E; ++I) {
      unsigned Opc = I->getOpcode();

      if (Opc == S1C33::JP_i) {
        auto Candidate = findDelaySlotCandidate(MBB, I, TII);
        if (Candidate == MBB.end())
          continue;
        I->setDesc(TII.get(S1C33::JP_D_i));
        LLVM_DEBUG(dbgs() << "Converting JP_i to JP_D_i in " << MF.getName() << "\n");
        fillDelaySlot(MBB, I, Candidate);
        markDelayedBranchInternalReads(*I, *std::next(I), TRI);
        Changed = true;
        // Bundle the delayed branch with its delay slot instruction so that
        // MachineBasicBlock::back() returns JP_D_i (a barrier), not the moved
        // slot instruction. Without bundling, canFallThrough() sees the slot
        // last instruction and incorrectly suppresses the successor BB label,
        // causing "Undefined temporary symbol" errors in the assembler.
        MIBundleBuilder(MBB, I, std::next(I, 2));
        // MachineInstrBundleIterator skips InsideBundle instructions, so
        // ++I will advance past the whole bundle to the next real instruction.
        ++I;
        continue;
      }

      if (Opc == S1C33::RET) {
        auto Candidate = findDelaySlotCandidate(MBB, I, TII);
        if (Candidate == MBB.end())
          continue;
        I->setDesc(TII.get(S1C33::RET_D));
        LLVM_DEBUG(dbgs() << "Converting RET to RET_D in " << MF.getName() << "\n");
        fillDelaySlot(MBB, I, Candidate);
        markDelayedBranchInternalReads(*I, *std::next(I), TRI);
        Changed = true;
        MIBundleBuilder(MBB, I, std::next(I, 2));
        ++I;
        continue;
      }

      if (Opc == S1C33::CALL_r) {
        auto Candidate = findDelaySlotCandidate(MBB, I, TII);
        if (Candidate == MBB.end())
          continue;
        I->setDesc(TII.get(S1C33::CALL_r_D));
        LLVM_DEBUG(dbgs() << "Converting CALL_r to CALL_r_D in " << MF.getName() << "\n");
        fillDelaySlot(MBB, I, Candidate);
        markDelayedBranchInternalReads(*I, *std::next(I), TRI);
        Changed = true;
        MIBundleBuilder(MBB, I, std::next(I, 2));
        ++I;
        continue;
      }
    }
  }
  return Changed;
}

namespace llvm {

FunctionPass *createS1C33DelaySlotFillerPass() {
  return new S1C33DelaySlotFiller();
}

} // namespace llvm
