//===- S1C33.td - Describe the S1C33 Target Machine --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// S1C33000 LLVM Backend — top-level target definition.
// EPSON S1C33000 32-bit RISC CPU core (target: Aquaplus P/ECE S1C33209 SoC).
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// S1C33 Subtarget Features
//===----------------------------------------------------------------------===//

// S1C33209 has an optional hardware multiplier.
def FeatureHWMul : SubtargetFeature<"hwmul", "HasHWMul", "true",
                                     "Enable S1C33209 hardware multiplier">;

// Predicate for instruction selection: true when hardware multiplier is present.
def HasHWMul : Predicate<"Subtarget->hasHWMul()">,
               AssemblerPredicate<(all_of FeatureHWMul), "Hardware Multiplier">;

//===----------------------------------------------------------------------===//
// S1C33 Processors
//===----------------------------------------------------------------------===//

def : ProcessorModel<"generic", NoSchedModel, []>;

// S1C33209: the SoC used in P/ECE, has optional multiplier.
def : ProcessorModel<"s1c33209", NoSchedModel, [FeatureHWMul]>;

//===----------------------------------------------------------------------===//
// Register File, Instruction Descriptions
//===----------------------------------------------------------------------===//

include "S1C33RegisterInfo.td"
include "S1C33CallingConv.td"
include "S1C33InstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<GR32>;

//===----------------------------------------------------------------------===//
// Assembly Printer
//===----------------------------------------------------------------------===//

def S1C33InstPrinter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
  bit isMCAsmWriter = 1;
}

//===----------------------------------------------------------------------===//
// Declare the target
//===----------------------------------------------------------------------===//

def S1C33InstrInfo : InstrInfo;

def S1C33 : Target {
  let InstructionSet = S1C33InstrInfo;
  let AssemblyWriters = [S1C33InstPrinter];
}
