# Documentation aid: renders a shell screen headless against the golden
# fixture. Built with the tests, never registered as one.
qt_add_executable(xp60studio_screenshot screenshot_harness.cpp)
target_link_libraries(xp60studio_screenshot PRIVATE
  Qt6::Quick
  Qt6::QuickControls2
  xp60studio::presentation
  xp60studio::simulation
  xp60studio_ui
)
set_target_properties(xp60studio_screenshot PROPERTIES AUTOMOC ON WIN32_EXECUTABLE FALSE)
target_include_directories(xp60studio_screenshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../cpp)
target_compile_definitions(xp60studio_screenshot PRIVATE
  XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

add_executable(xp60studio_midi_probe midi_probe.cpp)
target_link_libraries(xp60studio_midi_probe PRIVATE xp60studio::midi_libremidi)

# Read-only physical XP-60 acceptance probe. Sends RQ1 only; never a DT1.
add_executable(xp60studio_hardware_probe hardware_probe.cpp)
target_link_libraries(xp60studio_hardware_probe PRIVATE
  xp60studio::midi_libremidi
  xp60studio::protocol
  xp60studio::roland
  xp60studio::xp60
  xp60studio::xpmodel)

# Phase 3 write round trip against a physical XP-60. Drives
# services::PatchTransfer, so the armed write flow's safety properties apply.
qt_add_executable(xp60studio_write_roundtrip write_roundtrip_harness.cpp)
target_link_libraries(xp60studio_write_roundtrip PRIVATE
  Qt6::Core
  xp60studio::midi_libremidi
  xp60studio::services
  xp60studio::xpmodel)
set_target_properties(xp60studio_write_roundtrip PROPERTIES AUTOMOC ON WIN32_EXECUTABLE FALSE)

# Area 10: an exported .syx sent back to a physical XP-60. Validates every
# message against the temporary Patch area before transmitting.
qt_add_executable(xp60studio_export_roundtrip export_roundtrip_harness.cpp)
target_link_libraries(xp60studio_export_roundtrip PRIVATE
  Qt6::Core
  xp60studio::library
  xp60studio::midi_libremidi
  xp60studio::services
  xp60studio::xpmodel)
set_target_properties(xp60studio_export_roundtrip PROPERTIES AUTOMOC ON WIN32_EXECUTABLE FALSE)
