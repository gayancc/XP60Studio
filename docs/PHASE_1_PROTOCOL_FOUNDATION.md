# Phase 1 — Protocol Foundation

## Objective

Build the smallest trustworthy foundation that can communicate with a Roland XP-60 over MIDI/SysEx and provide enough diagnostics to investigate problems.

Phase 1 also establishes the final application shell technology so later UI work does not require a framework rewrite.

Phase 1 is **not** the polished product UI phase.

Do not begin Patch DNA, smart-bank generation, patch morphing, setlists, audio preview, or the full Dashboard/Patch Editor/Bank Builder here.

---

# First Assignment for Codex

Before implementation:

1. Inspect the entire repository.
2. Read `AGENTS.md` and all Phase 1-relevant documents.
3. Read the design implementation docs so the shell does not conflict with later UI work.
4. Confirm the current build state.
5. Establish the C++20 + Qt 6.11.x + QML + CMake project foundation if it does not exist.
6. Establish `IMidiTransport` and integrate libremidi behind it; pin an exact compatible libremidi 5.x release/commit.
7. Research only the Roland protocol facts necessary for the immediate implementation.
8. Record uncertain facts explicitly rather than guessing.
9. Implement deterministic protocol code first.
10. Add tests.
11. Build.
12. Run tests.
13. Correct failures based on actual logic rather than blindly adapting implementation to assertions.
14. Document what is verified, inferred, and still requires physical XP-60 testing.

Relevant UI docs:

- `design/UI_IMPLEMENTATION_ARCHITECTURE.md`
- `design/COMPONENT_CATALOG.md`
- `design/SCREEN_AND_FEATURE_MAP.md`
- `design/UI_ACCEPTANCE_CRITERIA.md`

The master mockup is `design/xp60studio-ui-master-mockup.jpg`, but Phase 1 should implement only the shell/design-system foundations and Devices/Diagnostics surface needed for protocol work.

---

# Scope

## A. Project Foundation

Create a clean C++20 / Qt 6.11.x / Qt Quick application suitable for Windows and macOS development.

Use CMake.

Phase 1 foundation should include:

- Qt Quick application entry point
- QML module structure
- XP60Studio theme/token foundation
- application navigation/shell skeleton
- C++ presentation-layer pattern (`QObject`/Qt models)
- unit-test targets
- no protocol logic in QML

Suggested high-level source boundaries:

```text
src/
  app/
  presentation/
  midi/
  roland/
  xp60/
  diagnostics/

qml/
  XP60Studio/
    Theme/
    Controls/
    Shell/
    Screens/

tests/
  cpp/
  qml/
```

Do not create deep speculative folder hierarchies before real code requires them.

---

## B. MIDI Transport Abstraction

Define an internal `IMidiTransport`-style abstraction before allowing libremidi types to spread through application code.

The transport must support:

- enumerate available MIDI inputs
- enumerate available MIDI outputs
- stable endpoint presentation/identity as far as the backend permits
- open selected input
- open selected output
- safely close/reconnect endpoints
- send ordinary MIDI
- send SysEx
- receive ordinary MIDI
- receive SysEx
- transport-level diagnostics/errors

Use libremidi as the default backend implementation.

The connection model must support separate input/output endpoints because many MIDI interfaces expose them separately.

---

## C. MIDI Receive Pipeline

Implement a safe receive path for:

- normal MIDI messages
- SysEx messages
- long SysEx data
- partial/fragmented SysEx if backend/platform behavior requires explicit reconstruction

Incoming processing must not perform heavy UI work on the MIDI callback thread.

Provide a controlled handoff from real-time/input callbacks to application processing and, separately, to Qt presentation state.

---

## D. MIDI Send Pipeline

Implement:

- ordinary MIDI send foundation
- SysEx send
- cancellation-aware transfer-operation abstraction
- configurable inter-message pacing foundation

Do not hardcode wireless-specific behavior.

Pacing should be configurable later because USB/DIN/WIDI interfaces may have different reliability limits.

---

# Roland Protocol Types

Create explicit C++ representations for the fundamental protocol concepts.

At minimum investigate and implement appropriate equivalents for:

```text
RolandAddress
RolandSize
RolandChecksum
RolandCommand
RolandSysExMessage
```

Do not expose raw byte indices throughout higher-level code.

---

# Roland SysEx Parsing

The parser should validate, where applicable:

- SysEx start/end
- Roland manufacturer ID
- device ID shape/range
- model ID
- command
- address bytes
- payload
- checksum

Parsing failures should return meaningful structured error information.

Do not throw away malformed messages silently.

Examples of distinguishable failures:

- not Roland SysEx
- unsupported command
- unsupported model
- truncated message
- invalid checksum
- invalid address length
- invalid size

---

# Roland Checksum

Implement the Roland checksum algorithm as an isolated deterministic function/type.

Required tests should include:

- known official examples where available
- generated address/data combinations
- checksum validation
- intentional corruption rejection

This code must not depend on QML, Qt Quick controls, or MIDI devices.

---

# Address and Size Arithmetic

Implement Roland address and size handling explicitly and test:

- construction from bytes
- serialization to bytes
- comparison
- valid arithmetic required by reads/writes
- carry behavior
- invalid-byte rejection where appropriate

Do not scatter manual carry logic around transfer code.

---

# DT1 Support

Implement Data Set 1 message construction/parsing according to verified Roland documentation.

The API should accept meaningful inputs such as:

- device ID
- model ID
- target address
- payload

and produce a validated SysEx message.

No caller outside the protocol layer should manually append a checksum byte.

---

# RQ1 Support

Implement Data Request 1 message construction/parsing according to verified documentation.

The API should accept:

- device ID
- model ID
- source address
- requested size

and produce a valid request.

Request handling should be designed so incoming DT1 data can later be correlated with an outstanding RQ1 operation.

---

# Request / Response Correlation Foundation

Create an operation model capable of representing:

```text
Request Sent
Awaiting Data
Receiving
Completed
Timed Out
Cancelled
Failed Validation
```

Do not assume one request always produces exactly one incoming callback.

The exact chunking behavior should be verified against the XP-60 before overly strict assumptions are embedded.

---

# Diagnostics

For each relevant message/operation retain useful metadata such as:

- timestamp
- direction (IN/OUT)
- endpoint
- manufacturer/model interpretation
- command
- address
- payload length
- checksum valid/invalid
- raw hex (advanced view/log)
- correlated request ID when applicable

Normal status messages should remain human-readable.

---

# Minimal Phase 1 UI

Only build enough Qt Quick UI to exercise and debug the protocol while establishing the permanent XP60Studio shell/design system correctly.

Use the same global navigation/header/component language defined in the design docs, but do not attempt to reproduce all four polished anchor screens yet.

The Devices/Diagnostics screen may contain:

- MIDI Input selector
- MIDI Output selector
- Connect / Disconnect
- connection state
- XP-60 device ID field/configuration if required
- raw/decoded MIDI activity panel
- manual RQ1 test area
- manual DT1 test area only where safe
- diagnostics/errors

Use reusable XP60Studio controls rather than default unstyled Qt controls, but keep the visual scope proportional to Phase 1.

The diagnostic interface can remain as an Expert/Developer tool later.

---

# Logging

Provide useful structured logs without requiring a debugger.

Avoid logging only raw hex.

Example conceptual log:

```text
20:14:03.115 OUT Roland RQ1 device=17 address=11 00 00 00 size=00 00 0C 00
20:14:03.143 IN  Roland DT1 device=17 address=11 00 00 00 bytes=256 checksum=OK
```

---

# Tests

Phase 1 should have deterministic automated coverage for at least:

## Checksum

- known checksum generation
- checksum verification
- invalid checksum detection

## Address

- byte serialization
- arithmetic/carry behavior
- invalid data rejection

## Message Codec

- DT1 encode
- DT1 decode
- RQ1 encode
- RQ1 decode if meaningful for received data
- truncated SysEx rejection
- non-Roland SysEx handling
- wrong model handling
- checksum corruption handling

## Round Trip

For messages where canonical serialization is expected:

```text
decode(encode(model)) == model
encode(decode(bytes)) == bytes
```

## Presentation / QML foundation

- application shell loads
- Devices/Diagnostics presentation model state transitions
- endpoint models update correctly
- Connect/Disconnect command enablement
- QML smoke tests for critical reusable foundation controls

---

# Hardware Verification Script

Codex must prepare a concrete test procedure for the physical XP-60 once the deterministic engine is ready.

The procedure should establish, at minimum:

1. MIDI IN/OUT open successfully.
2. XP-60 responds to a safe known RQ1 request.
3. Returned DT1 is parsed successfully.
4. Checksum validates.
5. Device/model IDs match expectations.
6. Address and payload size match requested data or documented chunking behavior.
7. Repeated reads are stable.
8. A safe temporary-area write can be performed and read back without touching permanent User memory, only after target-memory semantics are verified.

Do not invent a temporary-memory address; determine it from reliable XP-60 documentation during implementation.

---

# Explicit Non-Goals

Phase 1 does not require:

- complete Patch decoding
- full parameter map
- polished Dashboard
- polished four-Tone Patch Editor
- Wave Browser
- Bank Builder
- librarian/database
- expansion-board database
- duplicate analysis
- Patch DNA
- sound classification
- Performance editor
- Rhythm editor
- stage mode
- audio recording

---

# Definition of Done

Phase 1 is complete when:

- C++20/Qt/QML/CMake project builds on the supported development environment;
- application shell and minimal Devices/Diagnostics UI load;
- protocol unit tests pass;
- MIDI endpoints can be discovered and opened through `IMidiTransport`/libremidi;
- SysEx can be sent and received;
- Roland checksum logic is tested;
- Roland address/size handling is tested;
- DT1 messages can be constructed and parsed;
- RQ1 messages can be constructed correctly;
- received data can be associated with a request foundation;
- errors/timeouts/cancellation are modeled coherently;
- diagnostic output is useful;
- unknown XP-60-specific facts are documented rather than guessed;
- a precise physical-hardware validation procedure is ready;
- no advanced product features have been built on unverified assumptions;
- no JUCE application/UI dependency has been introduced.

---

# Phase 1 Exit Report

At completion Codex should update project documentation with:

## Implemented

What actually exists and builds.

## Automated verification

Which behaviors are covered by tests and fixtures.

## Hardware verified

Only behaviors actually observed on the physical XP-60.

## Requires hardware verification

Exact tests still needing the user's keyboard.

## Known unknowns

Protocol or address details not yet established.

## UI foundation status

Which Qt/QML shell/design-system pieces exist and what remains intentionally deferred.

## Next step

Only then begin Phase 2 — XP-60 Patch Model.
