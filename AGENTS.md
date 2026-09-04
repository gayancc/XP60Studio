# XP60Studio Agent Guide

This file is the root instruction map for Codex and other coding agents working in this repository.

## Mission

Build a modern, professional Roland XP-60 sound workstation: editor, librarian, bank-management system, sound-design environment, backup/restore utility, and live-performance tool.

Patch Base is a functional baseline for the expected capabilities of an XP-60 editor. Do not copy Patch Base source code, assets, proprietary implementation, or UI. XP60Studio must be an original product with stronger usability, transfer reliability, library intelligence, sound analysis, and bank engineering.

The initial hardware target is the Roland XP-60. Related XP/JV models are future targets only after XP-60 behavior is proven.

## Required reading before implementation

Read the documents relevant to the task before changing code:

- `docs/PRODUCT_VISION.md` — what the product should become
- `docs/FEATURE_BASELINE.md` — mature editor baseline and XP60Studio differentiators
- `docs/ARCHITECTURE.md` — required technical separation and domain boundaries
- `docs/ENGINEERING_PRINCIPLES.md` — correctness, testing, data safety, and implementation rules
- `docs/ROADMAP.md` — ordered development phases
- `docs/PHASE_1_PROTOCOL_FOUNDATION.md` — current first milestone
- `docs/design/UI_DESIGN_REFERENCE.md` — approved UI direction and master mockup for later UI phases

## Approved visual direction

The master visual reference is `docs/design/xp60studio-ui-master-mockup.jpg`. When implementing Dashboard, Patch Editor, Wave Browser, Bank Builder, or related design-system work, read `docs/design/UI_DESIGN_REFERENCE.md` and use the mockup as the primary visual direction together with the product and architecture docs.

The mockup is a design direction, not a command to copy pixels blindly. Real XP-60 behavior, accessibility, platform constraints, and validated usability take precedence while preserving the approved visual principles.

## Technology direction

Production application:

- C++
- JUCE

Python may be used for research utilities, SysEx analysis, fixture generation, and bulk-data investigation, but it must not become the core production runtime.

Initial targets are Windows and macOS. Do not introduce platform assumptions unnecessarily.

## Non-negotiable architectural rule

The UI must never construct Roland SysEx directly.

Keep these concerns separated:

1. UI
2. application services / orchestration
3. library intelligence
4. XP domain model
5. Roland SysEx protocol
6. MIDI transport

Hardware/protocol code must remain independent from visual controls.

## Protocol-first rule

Do not build elaborate editor UI before the Roland SysEx foundation and XP-60 data model are trustworthy.

The required confidence loop is:

`XP-60 -> fetch -> decode -> model -> encode -> send -> fetch again -> compare`

Every unexplained difference must be investigated. Never hide a mismatch simply to make a test pass.

## Source-of-truth order for hardware behavior

Prefer, in order:

1. official Roland XP-60/XP-80 MIDI/SysEx documentation
2. official Roland manuals
3. verified hardware captures
4. known-good supplied SysEx files
5. experimentally verified round trips against a physical XP-60
6. carefully evaluated secondary references

Never guess addresses or parameter interpretations and then spread the guess through the codebase. Represent unknowns explicitly until verified.

## Testing rule

When a test fails, determine whether the implementation, fixture, assumption, model, or test is wrong before changing production logic.

Important deterministic test areas include:

- Roland checksum
- address arithmetic
- message serialization/parsing
- RQ1 / DT1 construction
- parameter conversions
- range validation
- patch encode/decode
- round-trip integrity
- corrupted/truncated SysEx handling
- duplicate fingerprints
- compatibility analysis

## User-data safety

Local editing and experimentation should be non-destructive by default.

Writing to the XP-60 must always be explicit. The UI must eventually distinguish clearly between:

- LOCAL
- ON XP-60
- MODIFIED / UNSAVED

Never silently replace missing expansion waveforms, discard imported patches, or overwrite hardware state.

## Implementation behavior

For every phase:

- inspect existing implementation before changing it
- reuse sound abstractions instead of duplicating logic
- avoid speculative generic architecture
- keep protocol facts traceable
- build frequently
- run relevant tests
- investigate failures properly
- do not present placeholders as finished functionality
- keep long-running import/analysis/transfer work off the UI thread
- provide cancellation/progress for meaningful background operations

When physical hardware verification is required, state exactly what must be tested and what observation is needed. Never claim hardware verification without hardware evidence.

## Current priority

Start with **Phase 1 — Protocol Foundation** unless the user explicitly directs otherwise.

Do not jump ahead into Patch DNA, AI classification, morphing, elaborate dashboards, smart-bank generation, or setlists until the protocol foundation is reliable.

## Product standard

A musician should not need to understand hundreds of Roland parameters merely to answer basic questions such as:

- Which Tone is producing this part of the sound?
- Which waveform is missing?
- Which imported patches actually work on my installed expansions?
- Are these two patches really duplicates?
- Did all 128 patches actually reach the keyboard?
- Can I restore exactly what was on the XP-60 before this change?

The application must preserve expert depth while making these workflows straightforward.
