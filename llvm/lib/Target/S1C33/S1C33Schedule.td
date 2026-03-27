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
// 32-bit multiplies take 5 cycles, and memory bit operations take 3 cycles.
//
// This model enables the post-RA scheduler to reorder instructions to hide
// load-use and multiply latencies where possible.
//
// CPU Manual §3 pipeline overview:
//   IF → ID → EX → (MA) → WB
// A load result is available at the end of MA, so an immediately following
// instruction that consumes the loaded register stalls for 1 cycle.
//
// Scope:
//   - Load-use penalty (latency 2)
//   - 32×32 multiply latency (5 cycles)
//   - Memory bit-operation latency (3 cycles for btst/bclr/bset/bnot)
// Out of scope:
//   - FuncUnit definitions (single-issue, no need)
//   - Memory wait states / bus contention
//   - Branch taken/not-taken cost
//   - pushn/popn variable latency
//   - mac variable latency
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// SchedWrite / SchedRead definitions
//===----------------------------------------------------------------------===//

def WriteALU   : SchedWrite;  // Generic ALU (1 cycle)
def WriteLd    : SchedWrite;  // Memory load [%rb] / [%sp+imm6] (2 cycles)
def WriteLdPI  : SchedWrite;  // Post-increment load [%rb]+ (2 cycles)
def WriteMltW  : SchedWrite;  // mlt.w / mltu.w 32×32→64 (5 cycles)
def WriteMltH  : SchedWrite;  // mlt.h / mltu.h 16×16→32 (1 cycle)
def WriteBitOp : SchedWrite;  // btst/bclr/bset/bnot memory bit ops (3 cycles)

def ReadAfterLd : SchedRead;  // Read consuming a load result

//===----------------------------------------------------------------------===//
// S1C33 Machine Model
//===----------------------------------------------------------------------===//

def S1C33Model : SchedMachineModel {
  let MicroOpBufferSize = 0;   // In-order processor, no reorder buffer
  let CompleteModel = 0;       // Unlisted instructions default to 1 cycle
  let LoadLatency = 2;         // Default load latency (load-use penalty)
}

let SchedModel = S1C33Model in {
  def : WriteRes<WriteALU,   []> { let Latency = 1; }
  def : WriteRes<WriteLd,    []> { let Latency = 2; }
  def : WriteRes<WriteLdPI,  []> { let Latency = 2; }
  def : WriteRes<WriteMltW,  []> { let Latency = 5; }
  def : WriteRes<WriteMltH,  []> { let Latency = 1; }
  def : WriteRes<WriteBitOp, []> { let Latency = 3; }

  def : ReadAdvance<ReadAfterLd, 0>;
}
