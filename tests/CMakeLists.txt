find_package(Qt6 ${XP60STUDIO_QT_MINIMUM} REQUIRED COMPONENTS Test QuickTest)

add_subdirectory(cpp)

if(XP60STUDIO_BUILD_APP)
  add_subdirectory(qml)
  add_subdirectory(tools)
endif()
