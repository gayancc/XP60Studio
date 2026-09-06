# XP domain model & codecs (no Qt).
#
# Everything here is driven by parameter tables. The tables in generated/ are
# produced from the transcribed Roland Parameter Address Map — Patch Common and
# Patch Tone by tools/generate_patch_tables.py from
# docs/protocol/XP60_PATCH_PARAMETER_MAP.md, Performance Common and Performance
# Part by tools/generate_performance_tables.py from
# docs/protocol/XP60_PERFORMANCE_PARAMETER_MAP.md, and Rhythm Common and Rhythm
# Note by tools/generate_rhythm_tables.py from
# docs/protocol/XP60_RHYTHM_PARAMETER_MAP.md. Do not edit them by hand.
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
  Xp60BankLocation.h
  Xp60BankLocation.cpp
  Xp60PatchLayout.h
  Xp60PatchLayout.cpp
  Xp60Patch.h
  Xp60Effects.h
  Xp60PatchRouting.h
  Xp60Patch.cpp
  Xp60PatchCodec.h
  Xp60PatchCodec.cpp
  Xp60PatchDiff.h
  Xp60PatchDiff.cpp
  Xp60WaveIdentifier.h
  Xp60WaveIdentifier.cpp
  generated/Xp60PatchTables.h
  generated/Xp60PatchTables.cpp
  generated/Xp60PerformanceTables.h
  generated/Xp60PerformanceTables.cpp
  generated/Xp60RhythmTables.h
  generated/Xp60RhythmTables.cpp
  Xp60PerformanceLayout.h
  Xp60PerformanceLayout.cpp
  Xp60Performance.h
  Xp60Performance.cpp
  Xp60PerformanceCodec.h
  Xp60PerformanceCodec.cpp
)
add_library(xp60studio::xpmodel ALIAS xp60studio_xpmodel)
target_include_directories(xp60studio_xpmodel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(xp60studio_xpmodel PUBLIC xp60studio::roland xp60studio::xp60 xp60studio::midi xp60studio::protocol)
xp60studio_apply_warnings(xp60studio_xpmodel)
