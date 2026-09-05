add_library(xp60studio_simulation STATIC
  SimulatedXp60.h
  SimulatedXp60.cpp
  DemoFixture.h
  DemoFixture.cpp
  DemoReplyPump.h
  DemoReplyPump.cpp
  DemoBootstrap.h
  DemoBootstrap.cpp
)
add_library(xp60studio::simulation ALIAS xp60studio_simulation)
target_include_directories(xp60studio_simulation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(xp60studio_simulation
  PUBLIC
    Qt6::Core
    xp60studio::midi
    xp60studio::roland
    xp60studio::xp60
    xp60studio::xpmodel
    xp60studio::protocol
)
set_target_properties(xp60studio_simulation PROPERTIES AUTOMOC ON)
xp60studio_apply_warnings(xp60studio_simulation)

# Embed the golden bank so Demo Mode does not need XP60STUDIO_FIXTURE_DIR.
set(XP60STUDIO_DEMO_FIXTURE_DIR "${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")
qt_add_resources(xp60studio_simulation demo_fixtures
  PREFIX "/demo"
  BASE "${XP60STUDIO_DEMO_FIXTURE_DIR}"
  FILES "${XP60STUDIO_DEMO_FIXTURE_DIR}/user-bank-amal.syx"
)
