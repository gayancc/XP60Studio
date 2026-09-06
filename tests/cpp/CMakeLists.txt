# One executable per test area. All tests are deterministic: no hardware,
# no real MIDI backend, no wall-clock dependence.

function(xp60studio_add_test name)
  cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
  qt_add_executable(${name} ${ARG_SOURCES})
  target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  set_target_properties(${name} PROPERTIES AUTOMOC ON WIN32_EXECUTABLE FALSE)
  xp60studio_apply_warnings(${name})
  add_test(NAME ${name} COMMAND ${name} -o "${CMAKE_BINARY_DIR}/${name}-results.txt,txt")
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

# The XP-60 patch-selection hierarchy and the bank being built from it.
xp60studio_add_test(tst_bank_location
  SOURCES tst_bank_location.cpp
  LIBS xp60studio::xpmodel)

xp60studio_add_test(tst_bank_draft
  SOURCES tst_bank_draft.cpp
  LIBS xp60studio::library)

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

# Phase 8 — the Performance model
xp60studio_add_test(tst_xp60_performance
  SOURCES tst_xp60_performance.cpp
  LIBS xp60studio::xpmodel)
target_compile_definitions(tst_xp60_performance
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_xp60_system
  SOURCES tst_xp60_system.cpp
  LIBS xp60studio::xpmodel)

xp60studio_add_test(tst_xp60_rhythm
  SOURCES tst_xp60_rhythm.cpp
  LIBS xp60studio::xpmodel)
target_compile_definitions(tst_xp60_rhythm
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_performance_editor
  SOURCES tst_performance_editor.cpp
  LIBS xp60studio::presentation)
target_compile_definitions(tst_performance_editor
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_patch_diff
  SOURCES tst_patch_diff.cpp
  LIBS xp60studio::xpmodel)
target_compile_definitions(tst_patch_diff
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_patch_similarity
  SOURCES tst_patch_similarity.cpp
  LIBS xp60studio::library)
target_compile_definitions(tst_patch_similarity
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_patch_structure
  SOURCES tst_patch_structure.cpp
  LIBS xp60studio::library)
target_compile_definitions(tst_patch_structure
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_library_duplicates
  SOURCES tst_library_duplicates.cpp
  LIBS xp60studio::services)
target_compile_definitions(tst_library_duplicates
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_category_suggestion
  SOURCES tst_category_suggestion.cpp
  LIBS xp60studio::services)
target_compile_definitions(tst_category_suggestion
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

# Derived Sound DNA runtime: complete feature retention, hard evidence gates,
# deterministic scoring/contribution and constrained whole-Patch transforms.
xp60studio_add_test(tst_sounddna
  SOURCES tst_sounddna.cpp
  LIBS xp60studio::sounddna xp60studio::simulation)
target_compile_definitions(tst_sounddna
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_patch_transfer
  SOURCES tst_patch_transfer.cpp
  LIBS xp60studio::services xp60studio::simulation)
target_compile_definitions(tst_patch_transfer
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

# Phase 4 — Patch Editor presentation
xp60studio_add_test(tst_patch_routing
  SOURCES tst_patch_routing.cpp
  LIBS xp60studio::services xp60studio::simulation)
target_compile_definitions(tst_patch_routing
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_patch_editor
  SOURCES tst_patch_editor.cpp
  LIBS xp60studio::presentation xp60studio::simulation)
target_compile_definitions(tst_patch_editor
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

# Demo Mode simulation layer
xp60studio_add_test(tst_simulation
  SOURCES tst_simulation.cpp
  LIBS xp60studio::simulation xp60studio::services)
target_compile_definitions(tst_simulation
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

# Phase 5 — librarian foundation
xp60studio_add_test(tst_library_import
  SOURCES tst_library_import.cpp
  LIBS xp60studio::library)
target_compile_definitions(tst_library_import
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_library_database
  SOURCES tst_library_database.cpp
  LIBS xp60studio::library)
target_compile_definitions(tst_library_database
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_bank_store
  SOURCES tst_bank_store.cpp
  LIBS xp60studio::library)
target_compile_definitions(tst_bank_store
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_patch_workspace
  SOURCES tst_patch_workspace.cpp
  LIBS xp60studio::services)
target_compile_definitions(tst_patch_workspace
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_user_memory_write
  SOURCES tst_user_memory_write.cpp
  LIBS xp60studio::services xp60studio::simulation)
target_compile_definitions(tst_user_memory_write
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_expansion_compatibility
  SOURCES tst_expansion_compatibility.cpp
  LIBS xp60studio::library xp60studio::presentation xp60studio::services)
target_compile_definitions(tst_expansion_compatibility
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_user_bank_read
  SOURCES tst_user_bank_read.cpp
  LIBS xp60studio::services xp60studio::simulation)
target_compile_definitions(tst_user_bank_read
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_patch_sync
  SOURCES tst_patch_sync.cpp
  LIBS xp60studio::presentation xp60studio::services)
target_compile_definitions(tst_patch_sync
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_bank_builder
  SOURCES tst_bank_builder.cpp
  LIBS xp60studio::presentation xp60studio::services)
target_compile_definitions(tst_bank_builder
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_library_export
  SOURCES tst_library_export.cpp
  LIBS xp60studio::services)
target_compile_definitions(tst_library_export
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

xp60studio_add_test(tst_library_model
  SOURCES tst_library_model.cpp
  LIBS xp60studio::presentation)
target_compile_definitions(tst_library_model
  PRIVATE XP60STUDIO_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/xp60")

# The generated tables must match the protocol document they come from.
xp60studio_add_test(tst_wave_browser
  SOURCES tst_wave_browser.cpp
  LIBS xp60studio::presentation xp60studio::library)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME tst_wave_catalog_current
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/generate_wave_catalog.py --check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(NAME tst_generated_tables_current
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/generate_patch_tables.py --check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  # The hardware-session capture comparator resolves addresses against the same
  # transcribed Parameter Address Map; its self-test fails if that drifts.
  add_test(NAME tst_performance_tables_current
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/generate_performance_tables.py --check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(NAME tst_system_tables_current
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/generate_system_tables.py --check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(NAME tst_rhythm_tables_current
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/generate_rhythm_tables.py --check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(NAME tst_expansion_waves_current
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/generate_expansion_waves.py --check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(NAME tst_capture_diff
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/capture_diff.py --self-test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(NAME tst_sounddna_model_current
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/generate_sounddna_model.py --check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(NAME tst_sounddna_model_validation
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/generate_sounddna_model.py --self-test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(NAME tst_sounddna_analysis
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/sounddna_analysis.py self-test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(NAME tst_sounddna_stimulus
    COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/generate_sounddna_stimulus.py --self-test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

xp60studio_add_test(tst_wave_identifier
  SOURCES tst_wave_identifier.cpp
  LIBS xp60studio::xpmodel)

xp60studio_add_test(tst_library_export_service
  SOURCES tst_library_export_service.cpp
  LIBS xp60studio::services)
