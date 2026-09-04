# XP60Studio UI Implementation Architecture

This document is the authoritative UI implementation contract for XP60Studio.

Read it together with:

- [`UI_DESIGN_REFERENCE.md`](UI_DESIGN_REFERENCE.md)
- [`SCREEN_AND_FEATURE_MAP.md`](SCREEN_AND_FEATURE_MAP.md)
- [`COMPONENT_CATALOG.md`](COMPONENT_CATALOG.md)
- [`xp60studio-ui-master-mockup.jpg`](xp60studio-ui-master-mockup.jpg)
- [`../ARCHITECTURE.md`](../ARCHITECTURE.md)

The master mockup defines the approved visual character. This document defines how that visual direction is implemented without coupling UI code to Roland SysEx or device transport.

---

# 1. Final UI technology decision

The primary desktop application stack is:

- **C++20** for domain, protocol, application services, persistence, analysis, and device integration
- **Qt 6.11.x** as the current UI/application framework baseline
- **Qt Quick / QML** for screens, interaction, composition, states, transitions, animation, and responsive layout
- **Qt Quick Controls** as the base control toolkit
- **Qt Quick Layouts** for adaptive layout
- **Qt Quick Shapes / Canvas / custom QQuickItem only when needed** for envelopes, routing, meters, piano ranges, and other musical visualizations
- **Qt Graphs** only for data visualization that genuinely benefits from chart semantics; do not force synth controls into chart components
- **CMake** as the build system
- **libremidi 5.x** behind an internal `IMidiTransport` abstraction for MIDI I/O; pin an exact compatible release/commit when the dependency is first integrated

Do not introduce JUCE, Qt Widgets, Electron, WebView, React, Dear ImGui, Telerik, DevExpress, or another primary UI framework unless the architecture is explicitly revised.

Qt Quick Controls are a foundation, not the finished look. The product must use a custom XP60Studio design system and reusable music-specific components.

---

# 2. Core separation

```text
QML Screens
    |
XP60Studio QML Component Library
    |
Presentation Models / View Models (C++ QObject / QAbstractItemModel)
    |
Application Services / Commands (C++)
    |
XP Domain + Library + Transfer Engines (C++)
    |
Roland SysEx Protocol (C++)
    |
IMidiTransport (C++)
    |
libremidi / platform MIDI backend
    |
XP-60
```

Non-negotiable rules:

1. QML never constructs or parses SysEx.
2. QML never knows Roland byte addresses.
3. QML never calls libremidi directly.
4. Domain objects do not depend on QML.
5. Protocol code does not depend on Qt Quick visual classes.
6. Presentation models expose display-ready state and user intents, not raw protocol machinery.
7. Application services own asynchronous operations, cancellation, progress, retries, validation, and hardware writes.

---

# 3. QML/C++ boundary

Expose UI-facing C++ through intentionally designed presentation types.

Use:

- `QObject` + `Q_PROPERTY` for screen/view state
- signals for state changes/events
- `Q_INVOKABLE` or command objects for explicit user actions
- `QAbstractListModel` / `QAbstractTableModel` for large collections
- typed enums registered with Qt meta-object/QML systems
- queued signal delivery when crossing worker/UI threads

Avoid broad context properties containing service locators or arbitrary backend objects.

Avoid putting business rules in JavaScript blocks inside QML. Small formatting or view-only behavior is acceptable; domain decisions belong in C++.

Representative presentation models may include:

```text
AppShellViewModel
DeviceStatusViewModel
CurrentPatchViewModel
PatchEditorViewModel
ToneViewModel
WaveBrowserViewModel
BankBuilderViewModel
LibraryViewModel
CompareViewModel
ExpansionManagerViewModel
PerformanceViewModel
RhythmViewModel
SnapshotViewModel
LiveModeViewModel
DiagnosticsViewModel
```

Names may evolve, but screen state must remain separated from protocol/domain implementation.

---

# 4. Design-system architecture

Create a reusable QML module rather than styling each screen independently.

Suggested structure:

```text
src/
  app/
  presentation/
  domain/
  midi/
  roland/
  xp60/
  library/
  services/

qml/
  XP60Studio/
    Theme/
      Theme.qml
      Typography.qml
      Metrics.qml
      Motion.qml
    Controls/
    Music/
    DataViz/
    Shell/
    Screens/

resources/
  icons/
  images/
  fonts-license-info/

tests/
  cpp/
  qml/
```

Do not copy controls between screens. If the same visual/interaction pattern occurs twice, evaluate whether it belongs in the component library.

---

# 5. Theme and visual tokens

The mockup is the primary visual reference:

[`xp60studio-ui-master-mockup.jpg`](xp60studio-ui-master-mockup.jpg)

Centralize at least:

- background/surface levels
- border strengths
- text hierarchy
- semantic colors
- Tone 1/2/3/4 identity colors
- success/warning/error/info colors
- connection/live status colors
- radii
- spacing scale
- control heights
- icon sizes
- type scale
- animation durations/easing
- focus/hover/pressed/disabled states
- shadow/elevation policy

Do not hardcode arbitrary colors and spacing values throughout QML files.

Tone identity must remain stable across the application. For example, if Tone 1 is blue in the approved design, its meter, envelope highlight, routing line, comparison bar, and related badges should use the same semantic Tone token.

---

# 6. Responsive desktop behavior

The initial targets are Windows and macOS desktop.

Do not design only for one screenshot resolution.

Every major screen must define:

- preferred desktop width
- minimum usable width
- shrink priorities
- panels that collapse into drawers/tabs when width is constrained
- scroll ownership
- maximum readable content width where appropriate
- high-DPI behavior

The four-Tone editor should preserve all four Tone identities at normal desktop widths. At smaller widths it may switch from four equal cards to a selectable/scrollable Tone strip rather than making controls unreadably small.

Avoid horizontal scrolling for the whole application shell.

---

# 7. Interaction quality

The application should feel like professional music software.

Implement deliberate states for:

- hover
- pressed
- keyboard focus
- selected
- armed/destructive
- disabled
- loading
- live-to-hardware
- locally modified
- unsaved
- unavailable/missing expansion
- warning
- transfer in progress
- verified
- transfer mismatch
- disconnected/reconnecting

Do not communicate critical state using color alone.

Animations should clarify change, not decorate continuously. Respect OS reduced-motion preferences where Qt exposes them or provide an application motion-reduction setting.

---

# 8. Real-time control behavior

Interactive editors such as knobs, sliders, envelopes, ranges, and morph controls must separate visual responsiveness from MIDI transmission rate.

Conceptual flow:

```text
Pointer drag
  -> QML control updates immediately
  -> presentation model receives semantic value
  -> local patch model updates
  -> hardware update scheduler coalesces/throttles writes
  -> transfer service sends safe latest-value updates
  -> UI receives confirmed/error state separately
```

Never make a control feel laggy just because MIDI is slow.

Never transmit every pointer-move event blindly.

---

# 9. Drag/drop rule

Primary drag/drop workflows include Bank Builder and potentially library organization.

Every drag operation must have a non-drag alternative such as:

- Move To...
- Insert At...
- Replace Slot...
- keyboard move commands
- context actions

Drag previews must show exactly what will happen before drop, including replace/insert semantics and compatibility warnings.

---

# 10. Musical custom controls

Custom controls are justified where generic controls cannot express the workflow cleanly.

Expected examples:

- envelope point editor
- four-Tone level meter
- signal-flow/router visualization
- piano key-range selector
- velocity-range selector
- patch bank slot grid
- waveform/source compatibility visualization
- patch-difference bars
- transfer verification timeline

Prefer QML primitives and Shapes first. Use a custom `QQuickItem`/scene-graph implementation only when profiling shows a real performance or interaction need.

---

# 11. Large collections

Library and Wave Browser may contain thousands of entries.

Use model/view virtualization rather than creating all delegates at once.

Filtering/search/sorting should happen in C++ models/services when dataset size or algorithm complexity warrants it.

Do not freeze the UI during:

- SysEx import
- duplicate analysis
- compatibility analysis
- indexing
- bank proposal generation
- snapshot capture
- transfer verification

Every long operation needs progress and cancellation when cancellation is safe.

---

# 12. Navigation shell

Use one consistent application shell matching the master mockup's direction.

Primary navigation targets:

- Dashboard
- Library
- Editor
- Banks
- Performance
- Compare
- Devices
- Settings

Additional contextual destinations may appear inside these areas rather than expanding the permanent navigation indefinitely.

The global header/shell owns persistent device connection status. Individual screens must not invent different connection indicators.

---

# 13. Dialogs and destructive actions

Avoid modal-dialog-heavy workflows.

Use inline panels/drawers for inspection and non-destructive edits where possible.

Use confirmation dialogs only for consequential actions such as:

- overwriting permanent XP-60 memory
- restore operations
- removing unsaved work
- destructive library deletion

Confirmations must state exactly what will change: local library, temporary XP state, or permanent XP User memory.

---

# 14. Accessibility and input

Support mouse/trackpad and keyboard operation from the beginning.

Requirements:

- logical tab/focus order
- visible focus indication
- accessible names/descriptions for custom controls
- keyboard adjustment for knobs/sliders/envelope points where practical
- sufficient text/control contrast
- scalable UI/high-DPI
- tooltips for unfamiliar synth-specific controls
- exact-value entry alternative for drag/knob controls

Expert users should be able to work efficiently without the mouse for many operations.

---

# 15. UI testing strategy

Use both C++ and QML tests.

At minimum test:

- presentation-model state transitions
- commands and validation
- local/dirty/hardware state distinctions
- disabled/enabled rules
- transfer progress and mismatch states
- models used by large lists/grids
- QML component loading
- critical screen smoke tests
- keyboard/focus behavior for reusable controls

Do not rely only on screenshot tests. Visual regression can be added later, but semantic behavior is primary.

---

# 16. Mockup fidelity rule

The mockup is authoritative for:

- visual character
- information hierarchy
- major panel composition
- four-Tone color semantics
- density
- dark professional music-software direction
- Dashboard, Patch Editor, Wave Browser, and Bank Builder concept

The mockup is not authoritative for invented XP-60 data or impossible hardware behavior.

When implementation diverges, document why. Valid reasons include verified hardware behavior, accessibility, performance, platform behavior, or a clearly superior validated interaction.

Do not downgrade the design into a generic CRUD UI merely because stock Qt controls are easier.

---

# 17. Dependency policy

Qt is the UI/application framework. libremidi is the default MIDI transport implementation behind `IMidiTransport`.

Third-party visual component packages may only be added if they:

- solve a substantial problem not reasonably covered by Qt Quick/custom components;
- support Windows/macOS and the project's license/distribution model;
- are actively maintained;
- do not impose a second styling system that conflicts with XP60Studio;
- do not leak into domain/protocol layers.

Do not add a Telerik/DevExpress-style suite merely for visual polish. Build the XP60Studio music-specific component library on Qt Quick Controls.

---

# 18. Implementation order

Even though the final design is already defined, do not implement all screens in Phase 1.

Phase 1 creates:

- Qt/CMake application shell
- design-system foundation sufficient for a minimal diagnostics screen
- C++/QML presentation boundary
- MIDI/protocol foundation

The polished product surfaces are implemented in the roadmap phases where their backing domain behavior becomes trustworthy.
