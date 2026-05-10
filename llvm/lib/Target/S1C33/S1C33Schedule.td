//===-- S1C33Schedule.td - S1C33 Scheduling Model ----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// S1C33000 scheduling model.
//
// The S1C33000 is an in-order single-issue processor.  Most instructions
// execute in 1 cycle.  Loads have a 2-cycle latency (load-use penalty),
// 32x32 multiplies take 5 cycles, and memory bit operations take 3 cycles.
//
// This model drives both the pre-RA MachineScheduler and the post-RA list
// scheduler.  The default SelectionDAG list scheduler ("list-burr") is
// register-pressure driven and aggressively sinks computations across calls,
// causing unnecessary spills (see fpkplay/decode.c memset case);
// MachineScheduler with a real SchedModel respects call barriers and
// latencies more conservatively.
//
// CPU Manual section 3 pipeline overview:
//   IF -> ID -> EX -> (MA) -> WB
// A load result is available at the end of MA, so an immediately following
// instruction that consumes the loaded register stalls for 1 cycle.
//
// Scope:
//   - Load-use penalty (latency 2)
//   - 32x32 multiply latency (5 cycles, see docs/s1c33000_quick_reference.md)
//   - 16x16 multiply latency (1 cycle)
//   - Memory bit-operation latency (3 cycles for btst/bclr/bset/bnot)
//   - Branch / call / return classified separately to keep room for tuning
//   - pushn/popn modeled with worst-case latency 16 to discourage MISched
//     from reordering long sequences across them
// Out of scope:
//   - FuncUnit definitions (single-issue, no need)
//   - Memory wait states / bus contention
//   - Branch taken/not-taken cost (modeled as 1 cycle; mispredict penalty is
//     handled at the MISched layer via call-barrier semantics)
//   - mac variable latency (modeled as a 32x32 multiply, conservative)
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// SchedWrite / SchedRead definitions
//===----------------------------------------------------------------------===//

def WriteALU      : SchedWrite;  // Generic ALU (1 cycle)
def WriteLd       : SchedWrite;  // Memory load [%rb] / [%sp+imm6] (2 cycles)
def WriteLdPI     : SchedWrite;  // Post-increment load [%rb]+ (2 cycles)
def WriteMltW     : SchedWrite;  // mlt.w / mltu.w 32x32->64 (5 cycles)
def WriteMltH     : SchedWrite;  // mlt.h / mltu.h 16x16->32 (1 cycle)
def WriteBitOp    : SchedWrite;  // btst/bclr/bset/bnot memory bit ops (3 cycles)
def WriteJmp      : SchedWrite;  // Unconditional jump: jp / jp.d / jp %rb
def WriteJmpCond  : SchedWrite;  // Conditional branch: jr** and jr**.d
def WriteCall     : SchedWrite;  // Subroutine call: call / call.d / call %rb
def WriteRet      : SchedWrite;  // Return: ret / ret.d / reti / retd
def WriteSys      : SchedWrite;  // System: nop / slp / halt / brk / int
def WritePushPop  : SchedWrite;  // pushn / popn (range op, worst-case 16 cyc)
def WriteDivStep  : SchedWrite;  // div0s / div0u / div1 / div2s / div3s
def WriteSpecMove : SchedWrite;  // GPR <-> SP / PSR / ALR / AHR transfer

def ReadAfterLd : SchedRead;  // Read consuming a load result

//===----------------------------------------------------------------------===//
// S1C33 Machine Model
//===----------------------------------------------------------------------===//

def S1C33Model : SchedMachineModel {
  let MicroOpBufferSize = 0;   // In-order processor, no reorder buffer
  // CompleteModel = 1: TableGen errors on any instruction without a SchedRW.
  // All real instructions and surviving pseudos carry an explicit SchedRW;
  // the InstRW block below catches the few remaining pre-RA-eliminated
  // pseudos that TableGen still wants to see classified.
  let CompleteModel = 1;
  let LoadLatency = 2;         // Default load latency (load-use penalty)
}

let SchedModel = S1C33Model in {
  def : WriteRes<WriteALU,      []> { let Latency = 1; }
  def : WriteRes<WriteLd,       []> { let Latency = 2; }
  def : WriteRes<WriteLdPI,     []> { let Latency = 2; }
  // 32x32 multiply: docs/s1c33000_quick_reference.md (mlt.w / mltu.w take
  // 5 cycles due to the multiplier's pipeline interlock).
  def : WriteRes<WriteMltW,     []> { let Latency = 5; }
  def : WriteRes<WriteMltH,     []> { let Latency = 1; }
  def : WriteRes<WriteBitOp,    []> { let Latency = 3; }
  def : WriteRes<WriteJmp,      []> { let Latency = 1; }
  def : WriteRes<WriteJmpCond,  []> { let Latency = 1; }
  def : WriteRes<WriteCall,     []> { let Latency = 1; }
  def : WriteRes<WriteRet,      []> { let Latency = 1; }
  def : WriteRes<WriteSys,      []> { let Latency = 1; }
  // pushn %r15 performs 16 consecutive memory writes; latency 16 keeps
  // MISched from sinking many independent instructions across it.
  def : WriteRes<WritePushPop,  []> { let Latency = 16; }
  def : WriteRes<WriteDivStep,  []> { let Latency = 1; }
  def : WriteRes<WriteSpecMove, []> { let Latency = 1; }

  def : ReadAdvance<ReadAfterLd, 0>;

  // Target-independent pseudos that survive into the scheduler but are
  // expanded or removed before code emission.  RISCVSchedRocket.td uses the
  // same idiom for COPY.
  def : InstRW<[WriteALU], (instrs COPY)>;
}
