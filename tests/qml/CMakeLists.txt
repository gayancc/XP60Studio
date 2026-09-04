# QML smoke tests for the shell and the reusable foundation controls.
# The setup registers the presentation types and provides real view models
# backed by the loopback transport as context properties.
qt_add_executable(tst_qml
  tst_qml_main.cpp
)
target_link_libraries(tst_qml PRIVATE
  Qt6::Quick
  Qt6::QuickTest
  Qt6::QuickControls2
  xp60studio::presentation
  xp60studio_ui
)
set_target_properties(tst_qml PROPERTIES AUTOMOC ON)
target_include_directories(tst_qml PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../cpp)
target_compile_definitions(tst_qml PRIVATE
  QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

add_test(NAME tst_qml COMMAND tst_qml -input ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(tst_qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=Basic")
