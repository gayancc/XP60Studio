# ---------------------------------------------------------------------------
# libremidi 5.x  (default IMidiTransport backend)
#
# Pinned to the exact v5.4.3 release commit. Update deliberately: bump both the
# version comment and the commit hash together, then re-run the transport tests.
#
#   release : v5.4.3
#   commit  : 390707b5d18b590509e823386f03fa712ef6ac1b
#   license : MIT-style (see the libremidi repository LICENSE.md)
# ---------------------------------------------------------------------------
include(FetchContent)

set(XP60STUDIO_LIBREMIDI_VERSION "5.4.3")
set(XP60STUDIO_LIBREMIDI_COMMIT "390707b5d18b590509e823386f03fa712ef6ac1b")

# Keep the dependency surface small: XP60Studio only needs the native MIDI 1
# backends on Windows (WinMM) and macOS (CoreMIDI), plus ALSA for Linux
# development machines. Everything else is disabled to avoid extra downloads.
set(LIBREMIDI_NO_JACK ON CACHE BOOL "libremidi: disable JACK" FORCE)
set(LIBREMIDI_NO_PIPEWIRE ON CACHE BOOL "libremidi: disable PipeWire" FORCE)
set(LIBREMIDI_NO_NETWORK ON CACHE BOOL "libremidi: disable network backend" FORCE)
set(LIBREMIDI_NO_KEYBOARD ON CACHE BOOL "libremidi: disable computer keyboard backend" FORCE)
set(LIBREMIDI_NO_BOOST ON CACHE BOOL "libremidi: use std::vector for messages" FORCE)
set(LIBREMIDI_NO_WINMIDI ON CACHE BOOL "libremidi: disable Windows MIDI Services (MIDI 2) backend" FORCE)
set(LIBREMIDI_NO_WINUWP ON CACHE BOOL "libremidi: disable WinUWP backend" FORCE)
set(LIBREMIDI_DOWNLOAD_CPPWINRT OFF CACHE BOOL "libremidi: no cppwinrt download" FORCE)
set(LIBREMIDI_EXAMPLES OFF CACHE BOOL "libremidi: no examples" FORCE)
set(LIBREMIDI_TESTS OFF CACHE BOOL "libremidi: no tests" FORCE)
set(LIBREMIDI_NO_WARNINGS ON CACHE BOOL "libremidi: quiet third-party warnings" FORCE)

FetchContent_Declare(libremidi
  GIT_REPOSITORY https://github.com/celtera/libremidi.git
  GIT_TAG        ${XP60STUDIO_LIBREMIDI_COMMIT}
  GIT_SHALLOW    OFF
)
FetchContent_MakeAvailable(libremidi)

if(NOT TARGET libremidi::libremidi)
  message(FATAL_ERROR "libremidi target was not created")
endif()
