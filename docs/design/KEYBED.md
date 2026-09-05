# The key range keybed

`KeyboardStrip` draws a Patch key range on a piano keyboard. Drawn rather than
photographed, so it survives any width, both themes and the Tone accent colours,
and stays crisp on a HiDPI display.

Screenshots: [`screenshots/keybed/`](screenshots/keybed/).

## What was wrong

The keybed did not read as a keyboard. Three causes, all in the drawing rather
than in the data.

**Every key was tinted.** A Patch that responds across the whole keyboard is the
common case, and the range wash plus a tall accent glow under the sounding keys
turned the entire keybed into a blue block. The keys are now ivory and barely
change with the range; the bar underneath states it instead, with a shallow glow
and a span line across the top for the exact edges.

**The shadow fattened every black key.** The cast shadow was drawn 1.5 px wider
than the key it belonged to. On a 61-key window that is invisible. On the full
MIDI range, where a white key is under 5 px, it closed the gaps inside the
three-key group until the black keys touched and the octave pattern disappeared
entirely. The shadow now falls *below* the key rather than around it.

**The proportions were approximate.** Black keys are now 0.583 of a white key,
the real ratio (13.7 mm against 23.5 mm), with offsets matching a real keyboard:
the two-key group leans left of its boundaries and the three-key group right,
which is what lets an octave be recognised without counting.

Added alongside those: a felt strip along the back of the keys, a lit front lip
on the whites, a bevel and a side highlight on the blacks, and a two-tone slot
between white keys so the gap still has depth when it is one pixel wide.

## Why the full MIDI range looks dense

The window comes from the C++ model, not from QML. The instrument keybed is 61
keys (C2..C7), widened when the Patch reaches past it, so a Patch whose range is
0..127 draws 128 keys — at a panel width of about 350 px that is a white key
every 4.7 px.

That density is honest: the range really does extend past the instrument's own
keyboard, and the note beside the control says so in words. The fixes above are
what keep it legible at that density, rather than drawing a smaller keyboard and
implying the range is smaller than it is. At the instrument's own 61 keys the
same code produces an unmistakable keybed.
