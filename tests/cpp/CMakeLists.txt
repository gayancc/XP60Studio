# One executable per test area. All tests are deterministic: no hardware,
# no real MIDI backend, no wall-clock dependence.

function(xp60studio_add_test name)
  cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
  qt_add_executable(${name} ${ARG_SOURCES})
  target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
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
