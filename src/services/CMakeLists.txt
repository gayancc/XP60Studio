add_library(xp60studio_services STATIC
  DeviceSession.h
  DeviceSession.cpp
  PatchTransfer.h
  PatchTransfer.cpp
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
)
set_target_properties(xp60studio_services PROPERTIES AUTOMOC ON)
xp60studio_apply_warnings(xp60studio_services)
