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
//   For each RET and CALL_r instruction in the function:
//     1. Replace with the delayed variant (RET_D, CALL_r_D).
//     2. Look at the instruction immediately preceding the branch.
//     3. If it satisfies the delay slot constraints and has no hazard with the
//        branch, move it to the delay slot (insert it after the delayed branch).
//     4. Otherwise, insert a NOP (0x0000) in the delay slot.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/S1C33MCTargetDesc.h"
#include "S1C33InstrInfo.h"
#include "S1C33Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
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
  bool fillDelaySlot(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                     const S1C33InstrInfo &TII);
  bool isDelaySlotCandidate(const MachineInstr &MI) const;
};

char S1C33DelaySlotFiller::ID = 0;

} // end anonymous namespace

// Returns true if MI can legally fill a delay slot.
// Constraints from the CPU manual §4.x and DESIGN_SPEC.md §4.3.
bool S1C33DelaySlotFiller::isDelaySlotCandidate(
    const MachineInstr &MI) const {
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

// Try to fill the delay slot of the delayed branch/return/call at iterator I.
// Returns true if the function was modified.
bool S1C33DelaySlotFiller::fillDelaySlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator I,
                                          const S1C33InstrInfo &TII) {
  DebugLoc DL = I->getDebugLoc();

  // Look for a candidate in the instruction immediately before I.
  if (I != MBB.begin()) {
    MachineBasicBlock::iterator Prev = std::prev(I);
    // Reject if the candidate requires an EXT prefix: the EXT would have been
    // emitted as a separate MachineInstr immediately before Prev.  Moving Prev
    // to the delay slot without its EXT would corrupt the encoding.
    bool PrevNeedsExt = (Prev != MBB.begin()) &&
                        (std::prev(Prev)->getOpcode() == S1C33::EXT);
    if (!PrevNeedsExt && isDelaySlotCandidate(*Prev)) {
      // Move the candidate into the delay slot (splice it to after I).
      MachineBasicBlock::iterator SlotPos = std::next(I);
      MBB.splice(SlotPos, &MBB, Prev);
      LLVM_DEBUG(dbgs() << "  Filled delay slot with: " << *std::prev(SlotPos));
      return true;
    }
  }

  // No valid candidate found — insert a NOP in the delay slot.
  // S1C33 NOP = 0x0000 (all-zero 16-bit instruction).
  BuildMI(MBB, std::next(I), DL, TII.get(S1C33::NOP));
  LLVM_DEBUG(dbgs() << "  Inserted NOP in delay slot\n");
  return true;
}

bool S1C33DelaySlotFiller::runOnMachineFunction(MachineFunction &MF) {
  const S1C33Subtarget &STI = MF.getSubtarget<S1C33Subtarget>();
  const S1C33InstrInfo &TII =
      *static_cast<const S1C33InstrInfo *>(STI.getInstrInfo());

  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end();
         I != E; ++I) {
      unsigned Opc = I->getOpcode();

      // Convert JP_i → JP_D_i.
      if (Opc == S1C33::JP_i) {
        I->setDesc(TII.get(S1C33::JP_D_i));
        LLVM_DEBUG(dbgs() << "Converting JP_i to JP_D_i in " << MF.getName() << "\n");
        Changed |= fillDelaySlot(MBB, I, TII);
        // Bundle the delayed branch with its delay slot instruction so that
        // MachineBasicBlock::back() returns JP_D_i (a barrier), not the NOP.
        // Without bundling, canFallThrough() sees NOP (non-barrier) as the
        // last instruction and incorrectly suppresses the successor BB label,
        // causing "Undefined temporary symbol" errors in the assembler.
        MIBundleBuilder(MBB, I, std::next(I, 2));
        // MachineInstrBundleIterator skips InsideBundle instructions, so
        // ++I will advance past the whole bundle to the next real instruction.
        ++I;
        continue;
      }

      // Convert RET → RET_D.
      if (Opc == S1C33::RET) {
        I->setDesc(TII.get(S1C33::RET_D));
        LLVM_DEBUG(dbgs() << "Converting RET to RET_D in " << MF.getName() << "\n");
        Changed |= fillDelaySlot(MBB, I, TII);
        MIBundleBuilder(MBB, I, std::next(I, 2));
        ++I;
        continue;
      }

      // Convert CALL_r → CALL_r_D.
      if (Opc == S1C33::CALL_r) {
        I->setDesc(TII.get(S1C33::CALL_r_D));
        LLVM_DEBUG(dbgs() << "Converting CALL_r to CALL_r_D in " << MF.getName() << "\n");
        Changed |= fillDelaySlot(MBB, I, TII);
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
