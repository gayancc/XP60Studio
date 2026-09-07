add_library(xp60studio_interaction STATIC
  EnvelopeGeometry.h
  EnvelopeGeometry.cpp
  EnvelopeDrag.h
  EnvelopeDrag.cpp
)
add_library(xp60studio::interaction ALIAS xp60studio_interaction)
target_include_directories(xp60studio_interaction PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(xp60studio_interaction
  PUBLIC
    xp60studio::roland
    xp60studio::xpmodel
)
xp60studio_apply_warnings(xp60studio_interaction)
