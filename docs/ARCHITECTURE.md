# XP60Studio Architecture

## Architecture goal

Keep hardware/protocol correctness independent from UI design while providing a premium Qt Quick/QML product surface. The XP-60 engine must be rigorously testable without the GUI and later reusable for related Roland XP/JV models.

Authoritative UI implementation details live in [`design/UI_IMPLEMENTATION_ARCHITECTURE.md`](design/UI_IMPLEMENTATION_ARCHITECTURE.md).

Visual reference: [`design/xp60studio-ui-master-mockup.jpg`](design/xp60studio-ui-master-mockup.jpg).

Conceptual layers:

```text
XP60Studio
│
├── Qt Quick / QML Screens
├── XP60Studio QML Component Library
├── C++ Presentation Models
├── Application Services / Commands
├── Library Intelligence & Persistence
├── XP Domain Model & Codecs
├── XP-60 Device Protocol
├── Roland SysEx Protocol
├── IMidiTransport
└── libremidi / platform MIDI backend
       │
       └── XP-60
```

The UI must never manually build Roland SysEx packets.

The SysEx layer must never depend on visual components.

---

# 1. Technology baseline

Production application:

- C++20
- Qt 6.11.x current project baseline
- Qt Quick / QML
- Qt Quick Controls
- Qt Quick Layouts
- Qt Quick Shapes/custom `QQuickItem` where required for specialized musical graphics
- Qt Graphs only where chart semantics are appropriate
- CMake
- libremidi 5.x behind `IMidiTransport`; pin an exact compatible release/commit when integrated

Do not introduce JUCE as the application/UI framework.

Python remains allowed only for research/analysis/fixture utilities.

---

# 2. MIDI Transport

Define an internal transport interface independent from libremidi and Qt Quick.

Conceptual responsibilities:

```text
IMidiTransport
├── enumerateInputs()
├── enumerateOutputs()
├── openInput()
├── openOutput()
├── close()
├── sendMessage()
├── sendSysEx()
├── receive callback/event
└── transport diagnostics
```

Responsibilities:

- enumerate MIDI inputs/outputs
- open/close devices
- hot-plug/reconnect awareness where supported
- send ordinary MIDI
- receive ordinary MIDI
- send SysEx
- receive SysEx
- handle long SysEx messages
- cancellation hooks
- timeout/pacing support at higher transfer layer
- connection/transport diagnostics

libremidi is the default implementation because it is a dedicated modern cross-platform C++ MIDI library. Keep it replaceable behind `IMidiTransport`.

Do not mix Roland protocol knowledge into the transport layer.

---

# 3. Roland SysEx Protocol Layer

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

Represent addresses, commands, and payload boundaries through explicit C++ types.

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

---

# 4. XP-60 Device Protocol

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
- known readable/writable ranges

Do not generalize prematurely for every JV/XP synth. Correct XP-60 behavior comes first.

---

# 5. XP Domain Model

The core model should be strongly structured and understandable.

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

---

# 6. Parameter Metadata

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

Metadata should support decoding/encoding validation, UI display, patch diff, fingerprints, search, and developer inspection without replacing strong domain types.

---

# 7. Codec Layer

Use explicit codecs between raw XP memory bytes and domain objects.

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

`encode(decode(bytes)) == bytes`

unless a documented reason explains normalization.

---

# 8. Application Services

Orchestrate user-level operations without placing business logic into QML or visual controls.

Representative services:

- DeviceConnectionService
- PatchTransferService
- BankTransferService
- SnapshotService
- LibraryImportService
- PatchComparisonService
- ExpansionCompatibilityService
- PatchVersionService
- SearchService
- AnalysisService

Services own cancellation, progress, retries, validation, and hardware side effects.

---

# 9. Presentation Layer

This layer is the intentional boundary between C++ application logic and QML.

Use:

- `QObject` + `Q_PROPERTY` for UI-facing state
- signals/slots for change notification
- `Q_INVOKABLE` or explicit command objects for user intents
- `QAbstractListModel` / `QAbstractTableModel` for large collections
- Qt enums/meta-types for semantic states

Representative presentation models:

```text
AppShellViewModel
DeviceStatusViewModel
CurrentPatchViewModel
PatchEditorViewModel
ToneViewModel
WaveBrowserViewModel
LibraryViewModel
BankBuilderViewModel
CompareViewModel
ExpansionManagerViewModel
PerformanceViewModel
RhythmViewModel
SnapshotViewModel
LiveModeViewModel
DiagnosticsViewModel
```

QML must not receive protocol addresses, checksum responsibilities, raw service locators, or libremidi objects.

---

# 10. QML UI and Design System

The UI is implemented with Qt Quick/QML and the reusable XP60Studio component system described in:

- [`design/UI_IMPLEMENTATION_ARCHITECTURE.md`](design/UI_IMPLEMENTATION_ARCHITECTURE.md)
- [`design/COMPONENT_CATALOG.md`](design/COMPONENT_CATALOG.md)
- [`design/SCREEN_AND_FEATURE_MAP.md`](design/SCREEN_AND_FEATURE_MAP.md)

Major destinations:

- Dashboard
- Library
- Editor
- Banks
- Performance
- Compare
- Devices
- Settings

Additional workflows such as Expansion Manager, Snapshot/Restore, Rhythm, and Live Mode may be nested/contextual based on the screen map rather than each becoming permanent top-level navigation.

The approved visual anchor is [`design/xp60studio-ui-master-mockup.jpg`](design/xp60studio-ui-master-mockup.jpg).

---

# 11. UI state model

The product must distinguish canonical state dimensions rather than collapsing them into one generic loading/dirty flag.

Examples:

- connection: disconnected / connecting / connected / reconnecting / error
- data origin: local library / fetched from XP-60 / imported file
- edit state: clean / modified / unsaved
- hardware state: not written / writing / written / read-back verified / mismatch / failed
- compatibility: compatible / missing expansion / unknown / unsupported
- async operation: idle / queued / running / cancelling / completed / failed

Presentation models translate these into screen-ready states. QML renders them consistently through shared components.

---

# 12. Library Persistence

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

Choose the persistence technology after query patterns are understood. Do not add cloud/distributed architecture prematurely.

---

# 13. Library Intelligence

Keep derived intelligence separate from canonical Roland data.

Examples:

- exact fingerprints
- near-duplicate similarity
- Patch DNA
- automatic categorization assistance
- compatibility analysis
- smart bank selection

Every derived value must be distinguishable from actual hardware parameters. Prefer deterministic, explainable algorithms first.

---

# 14. Real-Time Editing

Parameter changes should update local state immediately and hardware promptly where safe.

Do not bind pointer-motion frequency directly to MIDI send frequency.

Use:

- throttling
- coalescing
- latest-value wins where semantically safe
- transaction grouping for multi-parameter operations
- separate UI-local state and hardware-confirmation state

QML remains visually responsive while C++ services schedule hardware writes.

---

# 15. Transfer Engine

Transfers are observable operations, not fire-and-forget writes.

Potential state machine:

```text
Queued
  ↓
Sending
  ↓
Awaiting / Receiving
  ↓
ReadBack
  ↓
Comparing
  ↓
Verified
```

Failure states include timeout, invalid response, checksum mismatch, device mismatch, cancelled, and read-back mismatch.

One shared transfer model should drive Editor, Bank, Snapshot, Performance, and Rhythm transfer UI.

---

# 16. Threading and responsiveness

Rules:

- MIDI callbacks do minimal work and hand data to controlled processing
- protocol parsing can run off the UI thread where appropriate
- library import/analysis/search indexing never blocks the QML scene/UI thread
- long operations expose progress and cancellation
- QML-visible model updates are marshalled safely to the Qt/UI thread
- avoid creating thousands of QML delegates; use virtualized model/view controls

---

# 17. Diagnostics and Raw Inspector

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

Eventually provide a Hex Inspector capable of relating raw SysEx to decoded fields. This is an expert feature; normal musician workflows should not require hex knowledge.

---

# 18. Snapshot Model

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

# 19. Future Multi-Device Support

Only extract shared XP/JV abstractions when common behavior is demonstrated by documentation or fixtures.

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

Do not compromise the XP-60 model merely to achieve generic abstraction early.
