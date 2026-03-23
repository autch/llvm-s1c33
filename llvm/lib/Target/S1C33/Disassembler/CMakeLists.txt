add_llvm_component_library(LLVMS1C33Disassembler
  S1C33Disassembler.cpp

  LINK_COMPONENTS
  MCDisassembler
  S1C33Desc
  S1C33Info
  MC
  Support

  ADD_TO_COMPONENT
  S1C33
)
