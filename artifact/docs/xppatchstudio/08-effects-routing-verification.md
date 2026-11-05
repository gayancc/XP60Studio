# Effects routing: what the app does, and what proves it

Checked 2026-11-05 against the XP-60/80 Owner's Manual (page numbers are the manual's own; text read from the full-manual OCR) and the project's recon notes.
"Text" = the manual says it in words. "Example" = the manual's routing examples on p.64 are consistent with it. "Inferred" = not stated, concluded from nearby text.
Nothing here is confirmed on the instrument (hardware ledger H-068, H-069).

| # | Rule the Routing tab follows | Evidence | Status |
|---|---|---|---|
| 1 | A Tone's Output Assign is MIX, EFX or DIR | p.61-63 value descriptions; p.199 value column | Text |
| 2 | The instrument has two physical outputs, MIX OUTPUT and DIRECT OUTPUT (so `OUTPUT-2` is not a jack, and is not offered) | p.8 "Multiple outputs: MIX OUTPUT and DIRECT OUTPUT stereo outputs" | Text |
| 3 | A send level of 0 sends nothing | p.62 "At 0 level, nothing is sent and no effects are connected" | Text |
| 4 | With Output Assign DIR only the direct sound goes to DIRECT OUT; chorus and reverb settings have no effect | p.61 "DIR: ... the effect settings have no effect"; p.75 "cannot be set" | Text |
| 5 | EFX Output Assign is MIX or DIR; with DIR the EFX's chorus and reverb settings have no effect | p.63 | Text |
| 6 | With EFX Output Assign MIX, the EFX Chorus and Reverb Send Levels feed chorus and reverb | p.63 descriptions; p.64 examples "EFX Chorus Send Level / EFX Reverb Send Level: 1-127" | Text + example |
| 7 | Chorus Output Assign: MIX, REVERB (to reverb only) or MIX+REV | p.63, p.66 | Text |
| 8 | Reverb goes to the MIX output | p.64 examples (reverb level only, no reverb output assign exists) | Example |
| 9 | Structure Type 2 to 10 on a pair makes the first Tone's output settings ignored (combined into the second Tone) | p.62 | Text |
| 10 | The Tone's own Chorus and Reverb Sends also feed chorus and reverb when the Tone is on EFX | p.63 lists them on the EFX case's screen; every p.64 EFX example sets them to 0, so no example proves the effect | Partly text, unproven: H-069 (a) |
| 11 | An EFX that is fed but whose EFX Output Level is 0 passes nothing on (shown as a dead end) | p.63 "EFX Output Level: volume level of the output sound that passes through EFX"; every p.64 example that uses the EFX sets it to 1-127 | Inferred |
| 12 | Chorus Level 0 or Reverb Level 0 leaves no sound from that unit (dead end) | p.63, p.66 "volume level of the chorus/reverb sound"; p.64 examples 1-127 | Inferred |
| 13 | The [EFX], [CHORUS] and [REVERB] panel switches turn each effect on or off | p.62 basic procedure step 1 | Text; **not read by the diagram** (System setting) |

## Naming

The address table calls parameter `00 1A` "EFX Mix Out Send Level"; the manual's screens call the matching parameter "EFX Output Level". The two are paired by position in the
manual's parameter list and the table (EFX Output Assign, then the level, then the EFX Chorus Send Level, then the EFX Reverb Send Level): that is a pairing by order, not an explicit statement.
The app shows the manual's name (the manual outranks the table) and keeps the table name in the hint.

## Where the research ran out

- Patch Base (the reference bundle) holds no XP routing text: its parameter names are not stored as readable strings, so it could not be used to cross-check naming or behaviour.
- The manual does not say whether a Tone on EFX also feeds chorus and reverb itself (rule 10), nor what a level of 0 does to the EFX output (rules 11 and 12), in so many words.
