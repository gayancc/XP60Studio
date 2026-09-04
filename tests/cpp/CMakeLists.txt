# One executable per test area. All tests are deterministic: no hardware,
# no real MIDI backend, no wall-clock dependence.

function(xp60studio_add_test name)
  cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
  qt_add_executable(${name} ${ARG_SOURCES})
  target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  set_target_properties(${name} PROPERTIES AUTOMOC ON)
  xp60studio_apply_warnings(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

xp60studio_add_test(tst_roland_checksum
  SOURCES tst_roland_checksum.cpp
  LIBS xp60studio::roland)

xp60studio_add_test(tst_roland_address_size
  SOURCES tst_roland_address_size.cpp
  LIBS xp60studio::roland)

xp60studio_add_test(tst_roland_codec
  SOURCES tst_roland_codec.cpp
  LIBS xp60studio::roland xp60studio::xp60)

xp60studio_add_test(tst_xp60_device
  SOURCES tst_xp60_device.cpp
  LIBS xp60studio::xp60)

xp60studio_add_test(tst_sysex_assembler
  SOURCES tst_sysex_assembler.cpp
  LIBS xp60studio::midi)

xp60studio_add_test(tst_request_tracker
  SOURCES tst_request_tracker.cpp
  LIBS xp60studio::protocol xp60studio::xp60)

xp60studio_add_test(tst_protocol_log
  SOURCES tst_protocol_log.cpp
  LIBS xp60studio::diagnostics xp60studio::xp60)

xp60studio_add_test(tst_device_session
  SOURCES tst_device_session.cpp
  LIBS xp60studio::services)

xp60studio_add_test(tst_presentation
  SOURCES tst_presentation.cpp
  LIBS xp60studio::presentation)

# Phase 2 — XP domain model & codecs
xp60studio_add_test(tst_parameter_table
  SOURCES tst_parameter_table.cpp
  LIBS xp60studio::xpmodel)

xp60studio_add_test(tst_block_codec
  SOURCES tst_block_codec.cpp
  LIBS xp60studio::xpmodel)

xp60studio_add_test(tst_memory_image
  SOURCES tst_memory_image.cpp
  LIBS xp60studio::xpmodel)

xp60studio_add_test(tst_sysex_stream
  SOURCES tst_sysex_stream.cpp
  LIBS xp60studio::xpmodel)

xp60studio_add_test(tst_patch_layout
  SOURCES tst_patch_layout.cpp
  LIBS xp60studio::xpmodel)

xp60studio_add_test(tst_generated_tables
  SOURCES tst_generated_tables.cpp
  LIBS xp60studio::xpmodel)

xp60studio_add_test(tst_xp60_patch
  SOURCES tst_xp60_patch.cpp
  LIBS xp60studio::xpmodel)

# Validates the model against real XP-60 user data.
# See tests/fixtures/xp60/README.md for provenance.
xp60studio_add_test(tst_golden_fixture
  SOURCES tst_golden_fixture.cpp
  LIBS xp60studio::xpmodel)
target_compile_definitions(tst_golden_fixture
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_patch_diff
  SOURCES tst_patch_diff.cpp
  LIBS xp60studio::xpmodel)
target_compile_definitions(tst_patch_diff
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_patch_transfer
  SOURCES tst_patch_transfer.cpp
  LIBS xp60studio::services)
target_compile_definitions(tst_patch_transfer
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

# Phase 4 — Patch Editor presentation
xp60studio_add_test(tst_patch_editor
  SOURCES tst_patch_editor.cpp
  LIBS xp60studio::presentation)
target_compile_definitions(tst_patch_editor
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

# The generated tables must match the protocol document they come from.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME tst_generated_tables_current
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/generate_patch_tables.py --check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
