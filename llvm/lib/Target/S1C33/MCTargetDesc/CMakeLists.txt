add_llvm_component_library(LLVMS1C33Desc
  S1C33AsmBackend.cpp
  S1C33ELFObjectWriter.cpp
  S1C33InstPrinter.cpp
  S1C33MCAsmInfo.cpp
  S1C33MCCodeEmitter.cpp
  S1C33MCTargetDesc.cpp

  LINK_COMPONENTS
  S1C33Info
  MC
  MCDisassembler
  Support
  TargetParser

  ADD_TO_COMPONENT
  S1C33
)
