qt_add_executable(xp60studio
  main.cpp
)
set_target_properties(xp60studio PROPERTIES
  OUTPUT_NAME XP60Studio
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_GUI_IDENTIFIER com.xp60studio.app
  MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
  MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)
target_compile_definitions(xp60studio PRIVATE XP60STUDIO_VERSION="${PROJECT_VERSION}")
target_link_libraries(xp60studio PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Qml
  Qt6::Quick
  Qt6::QuickControls2
  xp60studio::presentation
  xp60studio::midi_libremidi
  xp60studio_ui
)
xp60studio_apply_warnings(xp60studio)
