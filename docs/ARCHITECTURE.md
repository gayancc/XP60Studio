# XP60Studio Architecture

## Architecture goal

Keep hardware/protocol correctness independent from UI design so the XP-60 engine can be tested rigorously and later reused for related Roland XP/JV models.

Conceptual layers:

```text
XP60Studio
│
├── UI
├── Application Services
├── Library Intelligence
├── XP Domain Model
├── Roland SysEx Protocol
└── MIDI Transport
       │
       └── XP-60
```

The UI must never manually build Roland SysEx packets.

The SysEx layer must never depend on visual components.

---

# 1. MIDI Transport

Responsibilities:

- enumerate MIDI inputs/outputs
- open/close devices
- send ordinary MIDI
- receive ordinary MIDI
- send SysEx
- receive SysEx
- handle long SysEx messages
- reassemble fragmented input where needed
- cancellation
- timeout handling
- pacing/throttling
- diagnostics
- connection state
- transfer progress
- retry orchestration hooks

JUCE should be the production MIDI abstraction unless evidence shows a specific limitation requiring another approach.

Do not mix Roland protocol knowledge into the transport layer.

The transport should move bytes/messages and report communication state.

---

# 2. Roland SysEx Protocol Layer

Responsibilities:

- Roland manufacturer SysEx structure
- device ID handling
- model ID handling
- command representation
- DT1
- RQ1
- Roland address representation
- Roland size representation
- checksum generation
- checksum validation
- message serialization
- message parsing
- request/response correlation support
- payload validation

Avoid magic byte arrays spread across the application.

Represent addresses, commands, and payload boundaries through explicit types.

Potential concepts:

```text
RolandAddress
RolandSize
RolandChecksum
RolandCommand
RolandSysExMessage
RolandRequest
RolandResponse
```

Names can evolve, but the type boundaries should remain clear.

---

# 3. XP-60 Device Protocol

This layer applies XP-60-specific model IDs, address maps, memory regions, and supported requests on top of generic Roland SysEx behavior.

Potential responsibilities:

- XP-60 device identification
- temporary Patch area
- User Patch bank
- Performance memory
- Rhythm memory
- System memory
- request chunk sizing
- safe transfer pacing defaults
- known writable/readable ranges

Do not generalize prematurely for every JV/XP synth.

Correct XP-60 behavior comes first.

---

# 4. XP Domain Model

The core model should be strongly structured and understandable.

Conceptually:

```text
XP60
├── System
├── Temporary Area
├── User Patch Bank
├── User Performance Bank
├── User Rhythm Bank
│
├── Patch
│   ├── Common
│   ├── Effects
│   ├── Tone 1
│   ├── Tone 2
│   ├── Tone 3
│   └── Tone 4
│
├── Tone
│   ├── Wave
│   ├── Pitch
│   ├── Pitch Envelope
│   ├── TVF
│   ├── TVF Envelope
│   ├── TVA
│   ├── TVA Envelope
│   ├── LFO
│   ├── Key / Velocity behavior
│   └── Controllers / modulation
│
├── Performance
│   └── 16 Parts
│
└── Rhythm
    └── Drum tones
```

Do not flatten all behavior into an untyped key/value dictionary.

A metadata system may exist alongside the domain model, but the domain should remain readable in C++.

---

# 5. Parameter Metadata

Maintain metadata describing the relationship between a domain parameter and Roland data.

Where applicable, capture:

- stable parameter ID
- human-readable name
- address/offset
- raw range
- interpreted range
- default
- unit
- enum labels
- display formatter
- owning object
- parameter category
- compatibility constraints

Example concept:

```text
ToneCoarseTune
Raw      16..112
Display  -48..+48 semitones
Owner    Tone
Category Pitch
```

Metadata should support:

- decoding/encoding validation
- UI value presentation
- patch diff
- canonical fingerprints
- search filters
- developer inspection

Do not use metadata as an excuse to erase strong domain types.

---

# 6. Codec Layer

Use explicit codecs between raw XP memory bytes and domain objects.

Conceptually:

```text
Raw XP bytes
   ↓
PatchDecoder
   ↓
Patch
   ↓
PatchEncoder
   ↓
Raw XP bytes
```

Round-trip properties are critical:

```text
encode(decode(bytes)) == bytes
```

unless a documented reason explains normalization.

When normalization exists, tests must document it clearly.

---

# 7. Application Services

Orchestrate user-level operations without placing business logic into UI controls.

Potential services:

- DeviceConnectionService
- PatchTransferService
- BankTransferService
- SnapshotService
- LibraryImportService
- PatchComparisonService
- ExpansionCompatibilityService
- PatchVersionService

Names are illustrative, not mandatory.

These services should coordinate lower-level components and expose cancellation/progress to the UI.

---

# 8. Library Persistence

`.syx` files are import/export formats, not the application's database.

The persistent local library should support:

- normalized searchable metadata
- raw original SysEx preservation
- patch provenance
- source file/bank
- categories/tags
- favourites
- user ratings
- fingerprints
- compatibility metadata
- version history
- optional later audio-preview references

Choose the persistence technology after the data model and query patterns are understood.

Do not add cloud/distributed architecture prematurely.

The initial product is a local professional desktop application.

---

# 9. Library Intelligence

Keep derived intelligence separate from canonical Roland data.

Examples:

- exact fingerprints
- near-duplicate similarity
- Patch DNA
- automatic categorization assistance
- compatibility analysis
- smart bank selection

Every derived value must be distinguishable from actual hardware parameters.

Prefer deterministic, explainable algorithms first.

---

# 10. UI Architecture

The UI should consume domain/application-service state rather than protocol bytes.

Major conceptual areas:

- Home / Current Sound
- Connection / Diagnostics
- Patch Designer
- Four-Tone Mixer
- Wave Browser
- Envelope Editors
- Effects
- Library
- Compare
- Bank Builder
- Expansion Manager
- Performance Editor
- Rhythm Editor
- Snapshot/Restore
- Live Mode

Use progressive disclosure: Play, Design, Expert.

---

# 11. Real-Time Editing

Parameter changes should update hardware promptly where safe.

However, UI interaction must not flood MIDI.

Controls such as envelope dragging or slider movement may require:

- throttling
- coalescing
- latest-value wins
- transaction grouping for multi-parameter operations

The UI should remain responsive even with slower MIDI interfaces.

---

# 12. Transfer Engine

Transfers should be modeled as observable operations, not fire-and-forget writes.

Potential state machine:

```text
Queued
  ↓
Sending
  ↓
AwaitingResponse / Delay
  ↓
ReadBack
  ↓
Comparing
  ↓
Verified
```

Failure states may include:

- timeout
- invalid response
- checksum mismatch
- device mismatch
- cancelled
- read-back mismatch

The exact protocol will depend on verified XP-60 behavior.

---

# 13. Diagnostics and Raw Inspector

Maintain expert/developer visibility into protocol activity.

Useful diagnostic data:

- timestamp
- direction
- endpoint
- command
- address
- length
- checksum validity
- raw hex
- correlated operation

Eventually provide a Hex Inspector capable of relating raw SysEx to decoded fields.

This is an expert feature; normal musician workflows should not require hex knowledge.

---

# 14. Snapshot Model

A snapshot should preserve enough information to restore known XP-60 state safely.

Where practical retain:

- raw SysEx
- decoded representation
- capture date/time
- device profile
- expansion metadata
- verification result

Do not assume every system state is writable until verified.

---

# 15. Future Multi-Device Support

Only extract shared XP/JV abstractions when common behavior is demonstrated by documentation or fixtures.

Possible future structure:

```text
Roland Core
   │
   ├── XP60 Device Definition
   ├── XP80 Device Definition
   ├── XP50 Device Definition
   ├── XP30 Device Definition
   ├── JV1080 Device Definition
   └── JV2080 Device Definition
```

Do not compromise the XP-60 model merely to achieve a generic abstraction early.
