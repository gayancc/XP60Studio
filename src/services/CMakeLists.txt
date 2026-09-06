add_library(xp60studio_services STATIC
  DeviceSession.h
  DeviceSession.cpp
  PatchTransfer.h
  PatchTransfer.cpp
  PatchSyncState.h
  PatchSyncState.cpp
  PatchWorkspace.h
  PatchWorkspace.cpp
  UserMemoryWrite.h
  UserMemoryWrite.cpp
  UserPerformanceWrite.h
  UserPerformanceWrite.cpp
  UserBankRead.h
  UserBankRead.cpp
  LibraryImportService.h
  LibraryImportService.cpp
  LibraryExportService.h
  LibraryExportService.cpp
  LibraryDuplicateAnalysis.h
  LibraryDuplicateAnalysis.cpp
  CategorySuggestion.h
  CategorySuggestion.cpp
  SnapshotStore.h
  SnapshotStore.cpp
  SnapshotCapture.h
  SnapshotCapture.cpp
  RestorePlan.h
  RestorePlan.cpp
)
add_library(xp60studio::services ALIAS xp60studio_services)
target_include_directories(xp60studio_services PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(xp60studio_services
  PUBLIC
    Qt6::Core
    Qt6::Concurrent
    xp60studio::roland
    xp60studio::xp60
    xp60studio::midi
    xp60studio::protocol
    xp60studio::diagnostics
    xp60studio::xpmodel
    xp60studio::library
)
set_target_properties(xp60studio_services PROPERTIES AUTOMOC ON)
xp60studio_apply_warnings(xp60studio_services)
