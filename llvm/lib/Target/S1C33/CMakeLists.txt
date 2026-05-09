add_llvm_component_group(S1C33)

set(LLVM_TARGET_DEFINITIONS S1C33.td)

tablegen(LLVM S1C33GenAsmMatcher.inc         -gen-asm-matcher)
tablegen(LLVM S1C33GenAsmWriter.inc          -gen-asm-writer)
tablegen(LLVM S1C33GenCallingConv.inc        -gen-callingconv)
tablegen(LLVM S1C33GenDAGISel.inc            -gen-dag-isel)
tablegen(LLVM S1C33GenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM S1C33GenInstrInfo.inc          -gen-instr-info)
tablegen(LLVM S1C33GenMCCodeEmitter.inc      -gen-emitter)
tablegen(LLVM S1C33GenRegisterInfo.inc       -gen-register-info)
tablegen(LLVM S1C33GenSubtargetInfo.inc      -gen-subtarget)

add_public_tablegen_target(S1C33CommonTableGen)

add_llvm_target(S1C33CodeGen
  S1C33AsmPrinter.cpp
  S1C33DelaySlotFiller.cpp
  S1C33ExpandExtPseudos.cpp
  S1C33FrameLowering.cpp
  S1C33HoistImmInLoop.cpp
  S1C33InstrInfo.cpp
  S1C33ISelDAGToDAG.cpp
  S1C33ISelLowering.cpp
  S1C33RegisterInfo.cpp
  S1C33Subtarget.cpp
  S1C33TargetMachine.cpp
  S1C33TargetTransformInfo.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  S1C33Desc
  S1C33Info
  MC
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  S1C33
)

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
