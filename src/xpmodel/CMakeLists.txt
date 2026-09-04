# XP domain model & codecs (no Qt).
#
# Everything here is driven by parameter tables. The generic machinery
# (descriptors, block codec, memory image, SysEx stream parsing) is complete;
# the XP-60 Patch tables only contain entries confirmed from the Roland
# Parameter Address Map and declare themselves Partial until the map is
# transcribed (see docs/protocol/XP60_PATCH_PARAMETER_MAP.md).
add_library(xp60studio_xpmodel STATIC
  ParameterDescriptor.h
  ParameterDescriptor.cpp
  ParameterTable.h
  ParameterTable.cpp
  BlockCodec.h
  BlockCodec.cpp
  MemoryImage.h
  MemoryImage.cpp
  SysExStream.h
  SysExStream.cpp
  PatchName.h
  PatchName.cpp
  Xp60PatchLayout.h
  Xp60PatchLayout.cpp
)
add_library(xp60studio::xpmodel ALIAS xp60studio_xpmodel)
target_include_directories(xp60studio_xpmodel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(xp60studio_xpmodel PUBLIC xp60studio::roland xp60studio::xp60 xp60studio::midi)
xp60studio_apply_warnings(xp60studio_xpmodel)
