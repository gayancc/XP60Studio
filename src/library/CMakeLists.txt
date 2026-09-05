# Library intelligence and persistence (Qt Core).
#
# Everything here is DERIVED from, or metadata about, the canonical Roland data
# that xpmodel owns — fingerprints, provenance, the user's own tags and
# ratings. It never becomes a second source of hardware truth: a Patch's
# parameters and its original SysEx bytes always come from xpmodel.
add_library(xp60studio_library STATIC
  PatchFingerprint.h
  PatchFingerprint.cpp
  PatchProvenance.h
  PatchProvenance.cpp
  LibraryEntry.h
  LibraryEntry.cpp
  BankDraft.h
  BankDraft.cpp
  SyxImport.h
  SyxImport.cpp
  SyxExport.h
  SyxExport.cpp
  LibraryDatabase.h
  LibraryDatabase.cpp
)
add_library(xp60studio::library ALIAS xp60studio_library)
target_include_directories(xp60studio_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(xp60studio_library
  PUBLIC
    Qt6::Core
    Qt6::Sql
    xp60studio::roland
    xp60studio::xp60
    xp60studio::xpmodel
)
xp60studio_apply_warnings(xp60studio_library)
