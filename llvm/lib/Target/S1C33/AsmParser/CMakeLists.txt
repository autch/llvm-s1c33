add_llvm_component_library(LLVMS1C33AsmParser
  S1C33AsmParser.cpp

  LINK_COMPONENTS
  S1C33Desc
  S1C33Info
  MC
  MCParser
  Support

  ADD_TO_COMPONENT
  S1C33
)
