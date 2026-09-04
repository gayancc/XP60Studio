add_library(xp60studio_diagnostics STATIC
  ProtocolLogEntry.h
  ProtocolLogEntry.cpp
)
add_library(xp60studio::diagnostics ALIAS xp60studio_diagnostics)
target_include_directories(xp60studio_diagnostics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(xp60studio_diagnostics PUBLIC xp60studio::roland xp60studio::midi)
xp60studio_apply_warnings(xp60studio_diagnostics)
