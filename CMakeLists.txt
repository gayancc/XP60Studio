cmake_minimum_required(VERSION 3.22)

project(XP60Studio
  VERSION 0.1.0
  DESCRIPTION "XP60Studio - Roland XP-60 sound workstation"
  LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Language / toolchain baseline
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(XP60STUDIO_BUILD_TESTS "Build XP60Studio automated tests" ON)
option(XP60STUDIO_BUILD_APP "Build the Qt Quick application" ON)
option(XP60STUDIO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors for XP60Studio targets" OFF)

include(CompilerWarnings)

# ---------------------------------------------------------------------------
# Qt
#
# Project baseline: Qt 6.11.x (see AGENTS.md / docs/ARCHITECTURE.md).
# The CMake floor is 6.4 so the code can also be compiled and tested on
# distribution-packaged Qt (the Phase 1 verification environment only offered
# Qt 6.4.2).  Everything in this repository is written against APIs that exist
# unchanged in Qt 6.11.
# ---------------------------------------------------------------------------
set(XP60STUDIO_QT_BASELINE "6.11")
set(XP60STUDIO_QT_MINIMUM "6.4")

find_package(Qt6 ${XP60STUDIO_QT_MINIMUM} REQUIRED COMPONENTS
  Core
  Gui
  Qml
  Quick
  QuickControls2
)

if(Qt6_VERSION VERSION_LESS XP60STUDIO_QT_BASELINE)
  message(WARNING
    "XP60Studio: building against Qt ${Qt6_VERSION}, below the project baseline "
    "Qt ${XP60STUDIO_QT_BASELINE}. The build is supported for verification, but "
    "release builds should use the baseline.")
endif()

qt_standard_project_setup()
set(QT_QML_GENERATE_QMLLS_INI ON)

# ---------------------------------------------------------------------------
# Third-party
# ---------------------------------------------------------------------------
include(Dependencies)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
add_subdirectory(src)

if(XP60STUDIO_BUILD_APP)
  add_subdirectory(qml)
endif()

if(XP60STUDIO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
