# Deterministic, local Sound DNA runtime.  This layer is derived from the XP
# domain model and deliberately has no Qt, MIDI, UI, or device dependency.
add_library(xp60studio_sounddna STATIC
  SoundDnaTypes.h
  PatchFeatureExtractor.h
  PatchFeatureExtractor.cpp
  SoundDnaKnowledgeModel.h
  SoundDnaKnowledgeModel.cpp
  SoundDnaAnalyzer.h
  SoundDnaAnalyzer.cpp
  SoundDnaTransformationEngine.h
  SoundDnaTransformationEngine.cpp
  generated/SoundDnaModel.generated.h
)
add_library(xp60studio::sounddna ALIAS xp60studio_sounddna)
target_include_directories(xp60studio_sounddna PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(xp60studio_sounddna PUBLIC xp60studio::xpmodel)
xp60studio_apply_warnings(xp60studio_sounddna)
