add_library(xp60studio_protocol STATIC
  RequestState.h
  RequestState.cpp
  RequestOperation.h
  RolandRequestTracker.h
  RolandRequestTracker.cpp
  TransferPacing.h
  TransferPacing.cpp
)
add_library(xp60studio::protocol ALIAS xp60studio_protocol)
target_include_directories(xp60studio_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(xp60studio_protocol PUBLIC xp60studio::roland)
xp60studio_apply_warnings(xp60studio_protocol)
