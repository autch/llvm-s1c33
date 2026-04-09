add_lldb_library(lldbPluginABIS1C33 PLUGIN
  ABISysV_s1c33.cpp

  LINK_COMPONENTS
    Support
    TargetParser
  LINK_LIBS
    lldbCore
    lldbSymbol
    lldbTarget
    lldbPluginProcessUtility
    lldbValueObject
  )
