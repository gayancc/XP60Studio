# Transport abstraction, SysEx assembly and the in-memory loopback transport.
add_library(xp60studio_midi STATIC
  MidiTypes.h
  MidiTypes.cpp
  IMidiTransport.h
  IMidiTransport.cpp
  SysExAssembler.h
  SysExAssembler.cpp
  LoopbackMidiTransport.h
  LoopbackMidiTransport.cpp
)
add_library(xp60studio::midi ALIAS xp60studio_midi)
target_include_directories(xp60studio_midi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(xp60studio_midi PUBLIC cxx_std_20)
xp60studio_apply_warnings(xp60studio_midi)

# libremidi backend. Kept as a separate target so deterministic tests never
# link a platform MIDI backend.
add_library(xp60studio_midi_libremidi STATIC
  LibremidiTransport.h
  LibremidiTransport.cpp
)
add_library(xp60studio::midi_libremidi ALIAS xp60studio_midi_libremidi)
target_link_libraries(xp60studio_midi_libremidi
  PUBLIC xp60studio::midi
  PRIVATE libremidi::libremidi
)
xp60studio_apply_warnings(xp60studio_midi_libremidi)
