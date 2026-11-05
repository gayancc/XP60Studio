# XP60Studio Product Vision

## Product definition

XP60Studio is an XP-60-first **sound workstation**, not merely a parameter editor.

The product should combine:

- hardware editor
- SysEx librarian
- sound-design environment
- intelligent patch library
- 128-slot bank engineering
- backup/restore
- Performance and Rhythm editing
- live-performance organization

Patch Base establishes a useful baseline for what a capable XP-60 editor/librarian should support. XP60Studio must remain an original product and should go further in usability, explainability, verification, and large-library workflows.

## Core UX principle

The application should answer three questions quickly:

1. **What sound am I working with?**
2. **What is actually producing that sound?**
3. **What will change if I edit this?**

Do not force users to understand the entire XP-60 parameter structure before accomplishing ordinary musical tasks.

## Progressive disclosure

Use three conceptual levels over the same underlying patch model.

### Play

For musicians who want to:

- browse and search sounds
- audition patches
- manage favourites
- organize banks
- use setlists
- transfer sounds safely

### Design

For musicians actively shaping sounds:

- four-Tone overview
- waveform selection
- tuning
- filter
- amplitude
- envelopes
- modulation
- effects

### Expert

Expose every supported XP-60 parameter with exact values and technical details.

Simplified modes must never make advanced settings inaccessible.

---

# Home / Current Sound Experience

Do not make the initial screen resemble a database or file manager.

The opening view should communicate:

- XP-60 connection state
- current patch
- patch category / character
- active Tone layers
- current library state
- current user bank state
- primary actions

Conceptual example:

```text
┌──────────────────────────────────────────────────┐
│ XP STUDIO                       XP-60 ● ONLINE    │
├──────────────────────────────────────────────────┤
│ CURRENT SOUND                                    │
│ USER 034 — Warm Strings                          │
│ Strings • Warm • Wide • Layered                  │
│                                                  │
│ T1 ███████   T2 ██████   T3 ████   T4 ─────     │
│                                                  │
│ EDIT   COMPARE   SAVE AS   ADD TO BANK           │
├──────────────────────────────────────────────────┤
│ Library       Bank Builder        Sound Design   │
│ 2,843         106 / 128           Create         │
└──────────────────────────────────────────────────┘
```

Exact visual design can evolve; the information hierarchy is the important part.

---

# Connection Experience

Connection state must be obvious and trustworthy.

Show more than merely the presence of a MIDI endpoint.

Example:

```text
XP-60 ● CONNECTED

MIDI IN       WIDI Master
MIDI OUT      WIDI Master
Device ID     17
SysEx         ✓ Working
Read test     ✓
Write test    ✓
```

Where technically possible, verify actual Roland communication.

For transfer workflows, show individual progress and verification rather than a generic spinner.

---

# Visual Patch Architecture

The application should visually explain how the four-Tone patch is constructed.

Conceptually:

```text
                    PATCH
                      │
         ┌────────────┼────────────┐
         │            │            │
       TONE 1       TONE 2       TONE 3 ...
         │            │            │
       Wave         Wave         Wave
         │            │            │
      Pitch         Pitch        Pitch
         │            │            │
       TVF           TVF          TVF
         │            │            │
       TVA           TVA          TVA
         └────────────┼────────────┘
                      │
                  Structure
                      │
                     MFX
                      │
              Chorus / Reverb
```

A musician should understand the architecture of a patch in seconds.

---

# Four-Tone Mixer

Provide a dedicated four-Tone workspace showing key properties across all layers simultaneously.

Example:

```text
              TONE 1     TONE 2     TONE 3     TONE 4
Enabled          ●          ●          ●          ○
Level           100         82         61          -
Pan              -8         +8          0          -
Octave            0         +1         -1          -
Wave          Strings     Choir       Bell         -
SOLO             [S]        [S]        [S]        [S]
MUTE             [M]        [M]        [M]        [M]
```

Important behavior:

- Solo/Mute should be temporary and non-destructive where possible.
- The user should be able to hear what each Tone contributes.
- The full patch should be easy to restore instantly.

---

# Waveform Browser

Wave selection should not be presented as a raw numeric dropdown.

Provide search and filters for:

- internal waveforms
- expansion board source
- instrument family
- availability

Example filters:

- Strings
- Piano
- Brass
- Winds
- Vocal
- Synth
- Attack
- FX

Each waveform should clearly show whether it is available on the user's configured hardware.

---

# Expansion Board Manager

The user configures what is physically installed in:

- EXP-A
- EXP-B
- EXP-C
- EXP-D

Example:

```text
MY XP-60

EXP-A   SR-JV80-02 Orchestral
EXP-B   SR-JV80-04 Vintage Synth
EXP-C   Empty
EXP-D   Empty
```

This hardware profile drives patch compatibility analysis.

---

# Patch Compatibility Analysis

Imported patches should be checked against the user's expansion configuration.

Example:

```text
JP Strings 07

Compatibility: 75%

Tone 1   Internal        ✓
Tone 2   SR-JV80-02      ✓
Tone 3   Internal        ✓
Tone 4   SR-JV80-08      ✕
```

Possible explicit actions:

- Find Replacement
- Disable Tone
- Keep Anyway

Never silently replace missing waveforms.

---

# Intelligent Library

The library must handle many SysEx files as a dataset rather than forcing the user to inspect one bank at a time.

Example import set:

```text
xp50bank1.syx
xp60.syx
kasun.syx
amal.syx
hasi.syx
fb-bulk.syx
old-band.syx
backup-2004.syx
```

Analysis result may include:

```text
Files              8
Patches          896
Unique           613
Exact duplicates 181
Near duplicates   54
Unsupported       21
Invalid/corrupt    2
```

Preserve provenance: every imported patch should retain the source file/bank it came from.

---

# Duplicate and Similarity Analysis

Do not compare patch names as the primary duplicate rule.

Use canonicalized real synthesis data.

Exact duplicates:

```text
SUPER STR
Strings G
MyStrings

Actual synthesis data: identical
```

Near duplicates should explain the difference:

```text
96.8% similar

Tone 2 Level       82 → 86
Tone 3 Fine Tune   +3 → +5
Reverb Level       31 → 35
```

Similarity scoring must remain explainable.

---

# Categorization

Support manual categories and later classification assistance.

Useful categories include:

- Acoustic Piano
- Electric Piano
- Organ
- Strings
- Pads
- Brass
- Sax
- Woodwind
- Flute
- Lead
- Synth
- Bass
- Guitar
- Percussion
- Drums
- FX
- Vocal/Choir
- Ethnic
- Other

A patch may have multiple tags rather than being forced into one category.

---

# Patch DNA

Create deterministic, human-readable metadata derived from actual parameters.

Example:

```text
PATCH DNA

Layers       4
Brightness   71%
Attack       Fast
Movement     Medium
Stereo       Wide
Density      Dense
Reverb       Medium

Character
Warm
Wide
Layered
Slow Release
Orchestral
```

Patch DNA values are interpretations, not Roland parameters. The UI and data model must keep that distinction clear.

---

# Visual Envelope Editing

Pitch, TVF, and TVA envelopes should be graphical and draggable.

Graphical edits and numeric XP values must remain synchronized.

Expert view should expose exact values.

When dragging, avoid flooding the MIDI connection; use appropriate throttling/coalescing while preserving responsiveness.

---

# Patch Comparison

Allow any two patches to be compared structurally.

Compare meaningful differences across:

- active Tones
- waveforms
- tuning
- structures
- Pitch
- Pitch envelopes
- TVF
- TVF envelopes
- TVA
- TVA envelopes
- LFO
- modulation/controllers
- key/velocity ranges
- effects
- common parameters

Do not overwhelm the user with unchanged values.

Allow component copy where valid, for example:

- entire Tone
- TVF envelope
- TVA envelope
- LFO configuration
- effects configuration

---

# A/B Audition and History

Every editing session should support:

```text
A — ORIGINAL
B — CURRENT
```

Switch quickly between them without forcing save/reload.

Maintain lightweight local version history:

```text
Warm Strings

v12  Increased Tone 2 level
v11  Changed reverb
v10  Added expansion strings
v09  Original import
```

Version history must distinguish local versions from states actually written to the XP-60.

---

# Intelligent Variation

Do not limit randomization to uncontrolled random values.

Provide musically constrained variation such as:

```text
Variation Amount: 32%

Keep:
✓ Waves
✓ Effects
□ Envelopes
□ Filter
□ LFO
✓ Controllers
```

Later transformations may include:

- Warmer
- Brighter
- Wider
- Softer
- Bigger
- More Movement
- More Expressive

These should be deterministic parameter operations where possible, not vague claims of AI understanding.

---

# Mutation and Morphing

Future advanced sound-design workflows may include evolutionary variation:

```text
Original
   ├── Variation A
   ├── Variation B
   └── Variation C
```

The user chooses a preferred child and can create descendants.

Morphing between two patches should interpolate only parameters that have continuous meaning. Discrete identifiers such as waveform IDs need explicit transition rules and must never be numerically interpolated blindly.

---

# Bank Builder

The XP-60 User Patch bank has 128 slots. Do not present it only as a spreadsheet.

Allow user-defined grouping such as:

```text
LIVE BAND BANK

Piano         01–12
Strings       13–28
Pads          29–40
Brass         41–55
Sax/Winds     56–68
Leads         69–80
Sri Lankan    81–105
FX/Utility   106–128
```

Support:

- drag/drop
- multi-select
- insert/replace
- duplicate warnings
- missing-expansion warnings
- empty slots
- provenance
- undo/redo

The example categories are not hardcoded product rules.

---

# Smart Bank Builder

Later, allow the user to request a bank composition by category allocation.

Candidate selection can consider:

- compatibility
- category/tags
- exact duplicates
- near duplicates
- favourites
- user rating
- requested allocation

Generate an editable proposal. Never silently discard user patches.

---

# Transfer Verification

A bank transfer should not end with an unverified "Done" message.

Where hardware behavior permits, use:

```text
SEND
  ↓
READ BACK
  ↓
COMPARE
  ↓
VERIFY
```

Example success:

```text
128 / 128 VERIFIED
```

Example mismatch:

```text
127 / 128 verified
USER 073 differs from expected data.

Retry
Inspect Difference
Keep Keyboard Version
Send Again
```

Provide configurable pacing because MIDI interfaces and wireless links can behave differently.

---

# Device Diagnostics

Advanced diagnostics may include:

- input/output endpoint
- incoming/outgoing message counts
- round-trip test
- latency
- timeout count
- checksum failures
- retry count
- transfer rate
- raw SysEx view

Normal users should see understandable messages; experts can expand technical details.

---

# Snapshot / Backup

Provide a high-confidence workflow such as:

```text
SNAPSHOT MY XP-60
```

Eventually include supported:

- User Patches
- Performances
- Rhythm data
- System data
- expansion-profile metadata

Preserve original raw SysEx wherever practical alongside decoded structured data.

Before destructive restore, offer to create a safety snapshot.

---

# Performance Editor

Represent the XP-60's 16-part Performance mode as a musical mixer/workstation rather than a giant property sheet.

Show and edit relevant:

- Part
- Patch
- MIDI channel
- level
- pan
- transpose
- key range
- velocity range
- effects routing
- supported Part parameters

Expert mode must retain full supported access.

---

# Rhythm Editor

Use a keyboard/drum-map presentation.

Example:

```text
C1    Kick
C#1   Kick 2
D1    Snare
D#1   Clap
E1    Snare 2
```

Selecting a key reveals its Rhythm parameters.

---

# Setlists / Live Mode

After core editor reliability is established, add performance-oriented organization.

A setlist can map songs or sections to:

- Patch
- Performance
- optional notes
- ordering

Stage mode should use large, low-distraction information and must prevent accidental destructive editing.

---

# Audio Preview — Later Phase

XP60Studio is not initially a software synthesizer and does not need to emulate XP-60 audio generation.

A later feature may record the XP-60 audio output and associate previews with patches.

This can make large-library auditioning much faster.

---

# Sound-Aware Search — Later Phase

Once sufficient parameter metadata and/or real audio previews exist, support searches such as:

- warm strings
- bright brass
- brass with long fall
- soft evolving pad
- expressive flute lead
- wide orchestral strings

Initial search should rely on deterministic metadata and parameter analysis. Audio-similarity features should only be introduced when actual audio data exists.

---

# Product Standard

The finished product should let a musician answer questions such as:

- Which of my imported patches work with my installed expansion boards?
- Which banks contain true duplicates?
- Which patches are nearly identical, and exactly how do they differ?
- Which Tone produces the attack I am hearing?
- Which waveform is missing?
- Can I copy just this envelope?
- Can I make this sound wider without losing the original?
- Can I compare yesterday's version?
- Can I build one clean 128-patch live bank from many old banks?
- Did all 128 patches actually reach the XP-60?
- Can I restore exactly what was on the keyboard before I changed it?

That is the product goal: preserve the depth of the XP-60 while making it far easier to understand and use.
