add_llvm_component_library(LLVMS1C33Info
  S1C33TargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  S1C33
  )
