add_llvm_library(LLVMDebugInfoMsf
  ByteStream.cpp
  IndexedStreamData.cpp
  MappedBlockStream.cpp
  MsfBuilder.cpp
  MsfCommon.cpp
  MsfError.cpp
  StreamReader.cpp
  StreamWriter.cpp
  ADDITIONAL_HEADER_DIRS
  "${LLVM_MAIN_INCLUDE_DIR}/llvm/DebugInfo/MSF"
  )
