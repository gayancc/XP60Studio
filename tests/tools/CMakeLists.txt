# Documentation aid: renders a shell screen headless against the golden
# fixture. Built with the tests, never registered as one.
qt_add_executable(xp60studio_screenshot screenshot_harness.cpp)
target_link_libraries(xp60studio_screenshot PRIVATE
  Qt6::Quick
  Qt6::QuickControls2
  xp60studio::presentation
  xp60studio_ui
)
set_target_properties(xp60studio_screenshot PROPERTIES AUTOMOC ON)
target_include_directories(xp60studio_screenshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../cpp)
target_compile_definitions(xp60studio_screenshot PRIVATE
  XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")
