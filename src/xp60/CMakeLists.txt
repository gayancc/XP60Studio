add_library(xp60studio_xp60 STATIC
  Xp60Device.h
  Xp60Device.cpp
)
add_library(xp60studio::xp60 ALIAS xp60studio_xp60)
target_include_directories(xp60studio_xp60 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(xp60studio_xp60 PUBLIC xp60studio::roland)
xp60studio_apply_warnings(xp60studio_xp60)
