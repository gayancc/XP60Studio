add_library(xp60studio_roland STATIC
  RolandTypes.h
  SevenBitQuad.h
  SevenBitQuad.cpp
  RolandAddress.h
  RolandAddress.cpp
  RolandSize.h
  RolandSize.cpp
  RolandChecksum.h
  RolandChecksum.cpp
  RolandCommand.h
  RolandCommand.cpp
  RolandDeviceId.h
  RolandDeviceId.cpp
  RolandModelId.h
  RolandModelId.cpp
  RolandSysExMessage.h
  RolandSysExMessage.cpp
  RolandParseError.h
  RolandParseError.cpp
  RolandCodec.h
  RolandCodec.cpp
  HexFormat.h
  HexFormat.cpp
)
add_library(xp60studio::roland ALIAS xp60studio_roland)
target_include_directories(xp60studio_roland PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(xp60studio_roland PUBLIC cxx_std_20)
xp60studio_apply_warnings(xp60studio_roland)
