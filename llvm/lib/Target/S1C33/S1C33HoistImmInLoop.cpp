//===-- S1C33HoistImmInLoop.cpp - Hoist repeated immediates out of loops --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass detects ALU pseudo instructions of the form *_rri (ADD_rri,
// SUB_rri, AND_rri, OR_rri, XOR_rri) inside a natural loop where the SAME
// immediate value appears two or more times. For each such group it
// materializes the constant once in the loop preheader via MOV_ri32, and
// rewrites the in-loop uses to the corresponding 2-address reg-reg form
// (ADD_rr, SUB_rr, AND_rr, OR_rr, XOR_rr).
//
// MachineLICM cannot perform this transformation because the immediate is
// fused into the *_rri instruction; the whole instruction looks loop-variant
// to LICM (the register operand changes per iteration). Hoisting requires
// splitting the immediate from the operation.
//
// The pass runs pre-RA so the rewritten reg-reg form goes through normal
// register allocation. The post-RA ExpandExtPseudos pass continues to handle
// the *_ri32 pseudos used elsewhere.
//
//===----------------------------------------------------------------------===//

#include "S1C33.h"
#include "S1C33InstrInfo.h"
#include "S1C33Subtarget.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "s1c33-hoist-imm-in-loop"

namespace {

class S1C33HoistImmInLoop : public MachineFunctionPass {
public:
  static char ID;
  S1C33HoistImmInLoop() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "S1C33 Hoist Loop-Invariant Immediates";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.addPreserved<MachineLoopInfoWrapperPass>();
    AU.addPreserved<MachineDominatorTreeWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setIsSSA();
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  bool processLoop(MachineLoop *L, MachineRegisterInfo &MRI,
                   const S1C33InstrInfo &TII);
};

} // namespace

char S1C33HoistImmInLoop::ID = 0;

// Map a *_rri pseudo opcode to its 2-address reg-reg counterpart, or 0 if
// this is not a hoistable opcode.
static unsigned getRegRegOpcode(unsigned PseudoOpc) {
  switch (PseudoOpc) {
  case S1C33::ADD_rri: return S1C33::ADD_rr;
  case S1C33::SUB_rri: return S1C33::SUB_rr;
  case S1C33::AND_rri: return S1C33::AND_rr;
  case S1C33::OR_rri:  return S1C33::OR_rr;
  case S1C33::XOR_rri: return S1C33::XOR_rr;
  default:             return 0;
  }
}

bool S1C33HoistImmInLoop::processLoop(MachineLoop *L, MachineRegisterInfo &MRI,
                                       const S1C33InstrInfo &TII) {
  // Process inner loops first so a constant used in both an inner and outer
  // loop gets hoisted to the inner preheader; standard MachineLICM (which
  // already ran) can pull the resulting MOV_ri32 further out if the inner
  // preheader is itself loop-invariant w.r.t. the outer loop.
  bool Changed = false;
  for (MachineLoop *Sub : L->getSubLoops())
    Changed |= processLoop(Sub, MRI, TII);

  MachineBasicBlock *Preheader = L->getLoopPreheader();
  if (!Preheader)
    return Changed;

  // Group candidate instructions by (rr-opcode, immediate value).
  // Using SmallVector keeps deterministic ordering for testing.
  using KeyT = std::pair<unsigned, int64_t>;
  SmallVector<std::pair<KeyT, SmallVector<MachineInstr *, 4>>, 8> Groups;

  auto findGroup = [&](KeyT K) -> SmallVector<MachineInstr *, 4> & {
    for (auto &G : Groups)
      if (G.first == K)
        return G.second;
    Groups.push_back({K, {}});
    return Groups.back().second;
  };

  for (MachineBasicBlock *MBB : L->blocks()) {
    for (MachineInstr &MI : *MBB) {
      unsigned RrOpc = getRegRegOpcode(MI.getOpcode());
      if (!RrOpc)
        continue;
      if (MI.getNumOperands() < 3 || !MI.getOperand(2).isImm())
        continue;
      int64_t Imm = MI.getOperand(2).getImm();
      findGroup({RrOpc, Imm}).push_back(&MI);
    }
  }

  for (auto &G : Groups) {
    auto &Uses = G.second;
    if (Uses.size() < 2)
      continue;

    unsigned RrOpc = G.first.first;
    int64_t Imm = G.first.second;

    // Materialize once in the preheader.
    Register TmpReg = MRI.createVirtualRegister(&S1C33::GR32RegClass);
    MachineBasicBlock::iterator InsertPt = Preheader->getFirstTerminator();
    DebugLoc DL = Uses.front()->getDebugLoc();
    BuildMI(*Preheader, InsertPt, DL, TII.get(S1C33::MOV_ri32), TmpReg)
        .addImm(Imm);

    // Rewrite each use: %rd = AND_rr %rs, %TmpReg (and similarly for other
    // opcodes). The 2-address tie ($rd = $rs1) is satisfied by the
    // TwoAddressInstructionPass before register allocation.
    for (MachineInstr *MI : Uses) {
      Register Rd = MI->getOperand(0).getReg();
      Register Rs = MI->getOperand(1).getReg();
      MachineBasicBlock *MBB = MI->getParent();
      BuildMI(*MBB, *MI, MI->getDebugLoc(), TII.get(RrOpc), Rd)
          .addReg(Rs)
          .addReg(TmpReg);
      MI->eraseFromParent();
    }
    Changed = true;
    LLVM_DEBUG(dbgs() << "S1C33HoistImmInLoop: hoisted imm " << Imm
                      << " (" << Uses.size() << " uses) from "
                      << printMBBReference(*Preheader) << "\n");
  }

  return Changed;
}

bool S1C33HoistImmInLoop::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  const S1C33InstrInfo &TII =
      *MF.getSubtarget<S1C33Subtarget>().getInstrInfo();
  MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  bool Changed = false;
  for (MachineLoop *L : MLI)
    Changed |= processLoop(L, MRI, TII);
  return Changed;
}

FunctionPass *llvm::createS1C33HoistImmInLoopPass() {
  return new S1C33HoistImmInLoop();
}
