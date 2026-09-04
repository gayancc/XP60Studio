# XP60Studio Engineering Principles

## 1. Correctness before presentation

The XP-60 is hardware with a defined memory/protocol model. UI polish cannot compensate for incorrect SysEx behavior.

Protocol and codec correctness come before elaborate visual design.

## 2. Round-trip integrity

The key validation loop is:

```text
XP-60
  ↓ Fetch
Raw SysEx
  ↓ Decode
XP Model
  ↓ Encode
Raw SysEx
  ↓ Send
XP-60
  ↓ Fetch Again
Compare
```

Every unexplained mismatch must be investigated.

Do not normalize or discard bytes unless the reason is known and documented.

## 3. Do not blindly fix tests

When a test fails, investigate whether the problem is:

- production logic
- fixture data
- an outdated test
- an incorrect assumption
- a recently corrected model
- undocumented hardware behavior

Never change correct logic simply to make an assertion green.

## 4. Preserve evidence

Known-good SysEx fixtures, hardware captures, and documentation-derived mappings are valuable project assets.

Where licensing permits, retain fixtures and document:

- origin
- device/model
- expected interpretation
- checksum status
- whether verified on physical hardware

## 5. Do not guess protocol facts

Unknown address, parameter, enum, or memory behavior should be recorded as unknown until verified.

Avoid speculative names that later look authoritative.

## 6. Source-of-truth priority

Prefer:

1. official Roland technical MIDI/SysEx documentation
2. official Roland manuals
3. verified physical hardware captures
4. known-good supplied SysEx
5. hardware round-trip experiments
6. carefully evaluated secondary references

## 7. Hardware verification must be explicit

When Codex cannot test against the user's physical XP-60, it should say exactly what must be tested.

Example:

```text
Hardware check required:
1. Connect XP-60 via MIDI IN/OUT.
2. Request temporary Patch address X.
3. Capture returned SysEx.
4. Confirm whether byte N changes when Tone 2 Level is moved from A to B.
```

Never claim a behavior is hardware-verified unless actual evidence exists.

## 8. Non-destructive by default

Imports, comparisons, variations, morphing, categorization, bank proposals, and local edits should not overwrite source material.

Hardware writes must always be explicit.

Before potentially destructive restore/overwrite operations, provide clear warning and a safety-backup path where possible.

## 9. Preserve original SysEx

When importing a file, preserve the raw original data alongside decoded records whenever practical.

This allows future decoder corrections without losing the source evidence.

## 10. Provenance matters

A patch should retain where it came from:

- source file
- source bank
- slot/index
- import timestamp
- relevant device/model metadata

Large-library cleanup is impossible to trust without provenance.

## 11. Derived metadata is not hardware data

Values such as:

- Patch DNA
- similarity score
- category prediction
- brightness score
- compatibility score

are derived application metadata.

Never serialize them as though they were Roland parameters.

## 12. Explainable similarity first

Exact duplicate detection should use canonical parameter representation.

Near-duplicate scoring must be explainable through actual differences.

Avoid opaque similarity scores that cannot tell the user why two patches are considered related.

## 13. Strong domain types

Use explicit types for concepts such as:

- Roland addresses
- Roland sizes
- device/model IDs
- Patch slots
- Tone index
- raw vs displayed parameter values

Avoid spreading anonymous integers and raw byte offsets throughout the codebase.

## 14. UI must not own protocol logic

Visual controls request domain operations.

They do not build DT1 messages, calculate Roland checksums, or know XP memory addresses.

## 15. Avoid premature generic abstraction

XP-50, XP-80, XP-30, JV-1080, and JV-2080 are future opportunities.

Do not create an over-general device framework before the XP-60 is understood.

Extract common behavior when there is actual evidence of commonality.

## 16. Performance and responsiveness

Do not perform substantial parsing, library analysis, database work, or long transfers on the UI thread.

Meaningful long-running operations should provide:

- progress
- cancellation
- error reporting
- resumable/retry behavior where appropriate

## 17. MIDI pacing matters

Different interfaces may tolerate different SysEx rates.

The transfer system should eventually support configurable pacing and safe defaults rather than assuming maximum-speed bulk transmission is reliable.

## 18. Validate transfers

Where the XP-60 supports reliable read-back, prefer:

```text
send -> read back -> compare -> verify
```

A successful API call or completed byte write is not the same thing as verified hardware state.

## 19. Actionable errors

Bad:

```text
Error -12
```

Better:

```text
The XP-60 did not respond to the patch read request.
MIDI OUT: WIDI Master
MIDI IN: WIDI Master
Last successful response: 4.2 seconds ago
```

Provide deeper technical details separately for experts.

## 20. Build vertically, but in order

Each roadmap phase should become demonstrably usable before large amounts of work are added above it.

Do not build Patch DNA before patch decoding exists.
Do not build smart bank generation before the librarian exists.
Do not build a polished stage mode before reliable device communication exists.

## 21. Review existing work first

Before changing a subsystem:

- inspect current code
- understand existing abstractions
- find tests and fixtures
- reuse valid components
- change only what the task requires

Avoid rewriting working code simply because a different design is aesthetically preferable.

## 22. Build and test continuously

After implementation:

- build the affected targets
- run relevant tests
- investigate failures
- keep the repository in a coherent state

Do not leave known compile failures hidden behind incomplete work.

## 23. No fake completeness

If an implementation supports only part of an XP structure, mark it clearly as partial.

Do not fill unknown fields with invented defaults and call the feature complete.

## 24. User experience over parameter dumping

The product is for musicians, not only protocol engineers.

Expose the full XP-60 depth, but organize it through progressive disclosure, context, visualization, and musical terminology.

## 25. Keep expert inspection available

User-friendly design must not hide evidence.

Experts should eventually be able to inspect:

- raw SysEx
- decoded command
- address
- payload
- checksum
- corresponding parameter

This is essential for debugging and long-term trust.
