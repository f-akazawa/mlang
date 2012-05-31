set(LLVM_REQUIRES_EH 1)
set(LLVM_REQUIRES_RTTI 1)
set(LLVM_LINK_COMPONENTS Support)

add_tablegen(mlang-tblgen MLANG
  MlangASTNodesEmitter.cpp
  MlangAttrEmitter.cpp
  MlangDiagnosticsEmitter.cpp
  OptParserEmitter.cpp
  TableGen.cpp
  )
