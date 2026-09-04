# XP60Studio

XP60Studio is an XP-60-first sound workstation for the Roland XP/JV family.

The goal is not merely to expose Roland parameters. The application should make the XP-60 substantially easier to understand, edit, organize, preserve, transfer, compare, and perform with while retaining access to the full depth of the synthesizer.

## Product direction

XP60Studio starts from the functional baseline expected from a mature XP-60 editor/librarian and goes further with:

- complete Patch / Tone editing
- visual four-Tone sound architecture
- reliable Roland SysEx communication with read-back verification
- searchable waveform and expansion-board awareness
- intelligent SysEx library management
- exact and near-duplicate detection based on real parameters
- patch compatibility analysis against installed SR-JV boards
- visual patch comparison and component copy
- non-destructive A/B and version history
- intelligent variation, mutation, and later morphing
- 128-slot bank engineering and smart bank construction
- Performance and Rhythm editing
- safe snapshots and restore
- live setlist / stage workflows
- optional later audio-preview and sound-aware search features

## Technology direction

Primary production stack:

- C++
- JUCE

Initial desktop targets:

- Windows
- macOS

The architecture should remain open to future iPad/iOS and other Roland XP/JV models without compromising XP-60 correctness.

## Repository guide

Start with [`AGENTS.md`](AGENTS.md). Codex and human contributors should use it as the project map.

Detailed project documentation lives in [`docs/`](docs/):

- `PRODUCT_VISION.md` — product goals, UX model, feature direction
- `ARCHITECTURE.md` — technical boundaries and core layers
- `ENGINEERING_PRINCIPLES.md` — correctness, testing, safety, and workflow rules
- `ROADMAP.md` — phased development plan
- `PHASE_1_PROTOCOL_FOUNDATION.md` — first implementation milestone

## Current status

Repository initialized. Development should begin with Phase 1: MIDI and Roland SysEx protocol foundation. Do not begin advanced UI or intelligent-library features until protocol correctness and round-trip behavior are established.
