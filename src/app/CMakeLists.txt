qt_add_executable(xp60studio
  main.cpp
)

set(XP60STUDIO_ICON_DIR "${PROJECT_SOURCE_DIR}/docs/assets")
qt_add_resources(xp60studio app_icons
  PREFIX "/icons"
  BASE "${XP60STUDIO_ICON_DIR}"
  FILES "${XP60STUDIO_ICON_DIR}/xp60studio.png"
)
if(WIN32)
  enable_language(RC)
  configure_file(app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
  target_sources(xp60studio PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
  set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/app.rc" PROPERTIES
    OBJECT_DEPENDS "${XP60STUDIO_ICON_DIR}/xp60studio.ico")
elseif(APPLE)
  target_sources(xp60studio PRIVATE "${XP60STUDIO_ICON_DIR}/xp60studio.icns")
  set_source_files_properties("${XP60STUDIO_ICON_DIR}/xp60studio.icns" PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(xp60studio PROPERTIES MACOSX_BUNDLE_ICON_FILE xp60studio.icns)
endif()
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
  xp60studio::simulation
  xp60studio::midi_libremidi
  xp60studio_ui
)
xp60studio_apply_warnings(xp60studio)
