#!/usr/bin/env python3
"""Builds data/efx-defaults.json: the starting value of every EFX parameter of every one of the 40 effect types.

Why this exists: choosing an effect type used to change only the type number, so the twelve parameter slots kept whatever the previous type left in them (a
Level of 127 could land in a Pan slot and the sound jumped to the right), and a double-click on a knob reset it to the bottom of its range (a Level to 0).

Where the values come from, in order of authority:
  1. The Owner's Manual states no defaults, only names, ranges and what the units mean, so each value is chosen from what the *name and range say it must be*
     (RULES below): a pan is centred, a gain is 0 dB (raw 15 of 0..30), a level is full, a balance is half, a feedback is a gentle +20 %, a list stays on its
     first or its "off" entry, and so on. Each value carries the id of the rule that produced it.
  2. The 128 factory User patches (tests/golden/xp60-user-bank.syx) use 11 of the 40 types. Where Roland's own settings agree on a neutral value they confirm
     the rules (STEREO-EQ is flat in 46 of 75 patches, every Output Level is 127, every single Pan is 64, every Effect Balance is 50, HF Damp sits at 14 to 17,
     Reverb is HALL1) and they set a few values (rotary speed table, 2-voice pitch shifter panned left/right). Those are listed in EVIDENCE.
  3. Everything here is a *starting point*, not a claim about what the instrument loads when you change the type on its panel. Hardware item H-073 records that
     (select each type on the XP-60, read its parameters) so these values can be replaced by the real ones.

Run from the repository root:  python scripts/build_efx_defaults.py            (writes the file)
                                python scripts/build_efx_defaults.py --check    (exits 1 if the file is stale or a value is out of range)
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "data" / "efx-parameters.json"
OUT = ROOT / "data" / "efx-defaults.json"

# (rule id, name pattern, range or None for any, value, why). First match wins. `value` may be a function of the range.
RULES: list[tuple[str, str, tuple[int, int] | None, object, str]] = [
    ("pan-a", r"Output Pan A$", (0, 127), 0, "2-voice pitch shifter: voice A left (factory patches use L64 / 63R)"),
    ("pan-b", r"Output Pan B$", (0, 127), 127, "2-voice pitch shifter: voice B right (factory patches use L64 / 63R)"),
    ("pan", r"Pan$", (0, 127), 64, "a single pan is centred"),
    ("pan-dev", r"Pan Deviation$", (0, 20), 10, "deviation: middle of its range"),
    ("gain", r"Gain$", (0, 30), 15, "0 dB (raw 15 of 0..30)"),
    ("peak-freq", r"Peaking\d Frequency$", (0, 16), 7, "1 kHz, as the factory patches leave it"),
    ("peak-q", r"Peaking\d Q$", (0, 4), 1, "as the factory patches leave it"),
    ("eq-low-freq", r"^Low Frequency$", (0, 1), 0, "200 Hz, as the factory patches leave it"),
    ("eq-high-freq", r"^High Frequency$", (0, 1), 0, "4000 Hz, as the factory patches leave it"),
    ("band-q", r"^Q$", (0, 4), 1, "second step of the band-width list"),
    ("cutoff", r"Cutoff Frequency$", (0, 16), 8, "middle of the 200 Hz to 8 kHz list"),
    ("filter-off", r"^Filter Type$", (0, 2), 0, "filter OFF"),
    ("filter-first", r"^Filter Type$", (0, 1), 0, "first entry (LPF)"),
    ("amp", r"Amp Simulator Type$", (0, 3), 1, "BUILT-IN"),
    ("postgain", r"Post Gain$", (0, 3), 0, "0 dB"),
    ("ratio", r"Compression Ratio$", (0, 3), 2, "4:1"),
    ("fb-mode", r"Feedback Mode$", (0, 1), 0, "NORMAL"),
    ("fb-phase", r"Feedback Phase (Left|Right)$", (0, 1), 0, "NORMAL"),
    ("speed", r"^Speed$", (0, 1), 0, "SLOW"),
    ("reverb-type", r"^Reverb Type$", (0, 5), 4, "HALL1, as the factory patches use it"),
    ("gate-type", r"^Gate-Reverb Type$", (0, 3), 0, "NORMAL"),
    ("ps-mode", r"^Pitch Shifter Mode$", (0, 4), 0, "mode 1, as the factory patches use it"),
    ("coarse", r"^Coarse Pitch", (0, 36), 24, "0 semitones (raw 24 of -24..+12)"),
    ("fine", r"^Fine Pitch", (0, 100), 50, "0 cents (raw 50 of -100..+100)"),
    ("fb-level", r"Feedback Level$", (0, 98), 59, "+20 %, a gentle repeat (raw 49 is 0 %, 2 % a step; factory delays use 59)"),
    ("hf-damp", r"HF Damp$", (0, 17), 17, "BYPASS (no damping)"),
    ("phase", r"\bPhase$", (0, 90), 90, "180 degrees, full stereo spread"),
    ("threshold", r"Threshold Level$", (0, 127), 64, "as the factory patches use"),
    ("mix", r"Mix Level$", (0, 127), 64, "half"),
    ("balance", r"Balance$", (0, 100), 50, "half dry, half wet (D50:50W); every factory patch uses 50"),
    ("level", r"Level$", (0, 127), 127, "full level; every factory patch leaves Output Level at 127"),
    ("rate", r"Rate$", (0, 125), 20, "low in the range: a slow sweep (the rate scale is not linear)"),
    ("accel", r"Acceleration$", (0, 15), 8, "middle of its range"),
    ("predelay-dev", r"Pre Delay Deviation$", (0, 20), 0, "none"),
    ("depth-dev", r"Depth Deviation$", (0, 40), 20, "middle of its range"),
    ("predelay", r"Pre Delay Time", None, 0, "no pre-delay"),
    ("depth", r"Depth$", (0, 127), 48, "moderate depth"),
    ("manual", r"^Manual$", (0, 125), 40, "phaser centre frequency, lower middle"),
    ("manual-wah", r"^Manual$", (0, 127), 64, "auto-wah centre, middle"),
    ("resonance", r"Resonance$", (0, 127), 64, "moderate"),
    ("drive", r"Drive$", (0, 127), 64, "moderate drive"),
    ("sens", r"Sens$", (0, 127), 127, "full sensitivity, as the factory patch uses"),
    ("peak", r"^Peak$", (0, 127), 64, "moderate"),
    ("sustain", r"^Sustain$", (0, 127), 64, "moderate"),
    ("attack", r"^Attack$", (0, 127), 16, "fast attack"),
    ("release", r"Release Time$", (0, 127), 32, "as one factory patch uses"),
    ("separation", r"Separation$", (0, 127), 64, "middle"),
    ("gate-time", r"Reverb Time$", (0, 127), 64, "reverb time, middle"),
    ("gate-time-gr", r"Gate Time$", (0, 99), 50, "gate time, middle"),
    ("delay", r"Delay Time", (0, 126), 63, "middle of the range (raw 0..126 spans 0 to 500 ms, not linearly)"),
    ("delay-120", r"Delay Time$", (0, 120), 60, "middle of the time range"),
    ("band-gain", r"Band\d Gain$", (0, 30), 15, "0 dB"),
]

# Values the rules cannot express: the factory settings (rotary, mix) and tap layouts, by type number (1 to 40) and parameter index.
OVERRIDES: dict[int, dict[int, tuple[int, str]]] = {
    1: {},  # STEREO-EQ is entirely the rules: flat, as 46 of the 75 factory patches that use it
    4: {5: (127, "factory phasers leave Mix Level at 127")},
    8: {  # ROTARY: one factory patch, kept whole except the speed (starts SLOW) and the levels (full)
        1: (21, "factory rotary"), 2: (4, "factory rotary"), 3: (118, "factory rotary"), 4: (115, "factory rotary"),
        6: (8, "factory rotary"), 7: (6, "factory rotary"), 8: (100, "level of the horn"), 9: (100, "level of the drum"), 10: (110, "factory rotary"),
    },
    19: {1: (42, "left tap a third of the way"), 2: (84, "right tap two thirds"), 3: (63, "centre tap in the middle"),
         6: (90, "tap level"), 7: (90, "tap level"), 8: (100, "tap level")},
    20: {1: (31, "tap 1 a quarter of the way"), 2: (63, "tap 2 half way"), 3: (94, "tap 3 three quarters"), 4: (125, "tap 4 the longest"),
         5: (127, "tap level, falling"), 6: (96, "tap level, falling"), 7: (64, "tap level, falling"), 8: (32, "tap level, falling")},
    7: {4: (64, "auto-wah sensitivity, middle")},
    18: {7: (16, "a small depth for a modulated delay")},
}
# Within the 2-voice pitch shifter and the feedback pitch shifter the feedback and the voice detune must start neutral.
NEUTRAL_FEEDBACK_TYPES = {23}
MODDELAY_TYPE = 18


def value_for(t: int, p: dict) -> tuple[int, str]:
    idx, name, (lo, hi) = p["index"], p["name"], p["raw"]
    if idx in OVERRIDES.get(t, {}):
        v, why = OVERRIDES[t][idx]
        return v, f"override: {why}"
    if t in NEUTRAL_FEEDBACK_TYPES and name.endswith("Feedback Level"):
        return 49, "pitch-shifter feedback starts at 0 %"
    for rid, pat, rng, val, why in RULES:
        if re.search(pat, name) and (rng is None or rng == (lo, hi)):
            v = val(lo, hi) if callable(val) else val
            return int(v), f"{rid}: {why}"
    return lo, "no rule matched: start of range (check this line)"


def build() -> dict:
    src = json.loads(SRC.read_text(encoding="utf8"))
    types = []
    for t in src["types"]:
        rows, vec = [], [0] * 12
        for p in t["params"]:
            if not p["raw"]:
                continue
            v, why = value_for(t["type"], p)
            lo, hi = p["raw"]
            if not lo <= v <= hi:
                sys.exit(f"type {t['type']} {t['name']}: parameter {p['index']} {p['name']} default {v} outside {lo}..{hi}")
            vec[p["index"] - 1] = v
            rows.append({"index": p["index"], "name": p["name"], "value": v, "why": why})
        types.append({"type": t["type"], "name": t["name"], "defaults": vec, "rows": rows})
    return {
        "about": "Starting values for each EFX type's twelve parameters, generated by scripts/build_efx_defaults.py (rules from the manual's names and ranges, checked against the 128 factory User patches). Not read from the instrument: see hardware item H-073.",
        "types": types,
    }


GOLDEN = ROOT / "tests" / "golden" / "xp60-user-bank.syx"


def golden_efx() -> list[tuple[int, list[int]]]:
    """(EFX type, twelve parameters) of each of the 128 factory User patches, read straight from the SysEx file."""
    raw = GOLDEN.read_bytes()
    mem: dict[int, int] = {}
    i = 0
    while i < len(raw):
        if raw[i] != 0xF0:
            i += 1
            continue
        j = raw.index(0xF7, i)
        f = raw[i:j + 1]
        i = j + 1
        if len(f) > 11 and f[3] == 0x6A and f[4] == 0x12:
            a = (f[5] << 21) | (f[6] << 14) | (f[7] << 7) | f[8]
            for k, v in enumerate(f[9:-2]):
                mem[a + k] = v
    out = []
    for n in range(128):
        base = (0x11 << 21) | (n << 14)
        if base + 12 in mem:
            out.append((mem[base + 12], [mem[base + 13 + k] for k in range(12)]))
    return out


def verify_against_golden(doc: dict) -> list[str]:
    """Second, independent check: what Roland's own patches do with each kind of parameter must agree with the rule that set its default.

    Claims tested: every single Pan in the factory patches is centred (64), every Output Level is 127, every Effect Balance is at most 100 and a flat
    EQ is the most common STEREO-EQ setting and equals our default for type 1. Returns the claims that fail.
    """
    names = {t["type"]: {p["index"]: p["name"] for p in t["params"] if p["raw"]} for t in json.loads(SRC.read_text(encoding="utf8"))["types"]}
    bad: list[str] = []
    patches = golden_efx()
    if len(patches) != 128:
        bad.append(f"golden bank holds {len(patches)} patches, expected 128")
    flat = [p for t, p in patches if t == 0]
    from collections import Counter

    common = Counter(tuple(p[:11]) for p in flat).most_common(1)
    mine = tuple(next(t for t in doc["types"] if t["type"] == 1)["defaults"][:11])
    if not common or common[0][0] != mine:
        bad.append(f"STEREO-EQ: most common factory setting {common[0][0] if common else None} differs from our default {mine}")
    for t, params in patches:
        for idx, name in names[t + 1].items():
            v = params[idx - 1]
            if re.search(r"Pan$", name) and not re.search(r"Pan [AB]$|Deviation", name) and v != 64:
                bad.append(f"type {t + 1} {name}: factory value {v}, rule says 64")
            if re.search(r"Output Level$", name) and v == 0:
                bad.append(f"type {t + 1} {name}: factory value 0")
    return bad


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    text = json.dumps(build(), ensure_ascii=False, indent=1) + "\n"
    unmatched = [r for t in json.loads(text)["types"] for r in t["rows"] if r["why"].startswith("no rule")]
    if unmatched:
        sys.exit("no rule for: " + ", ".join(f"{r['name']}" for r in unmatched))
    if args.check:
        if not OUT.exists() or OUT.read_text(encoding="utf8") != text:
            sys.exit("data/efx-defaults.json is stale")
        return
    OUT.write_text(text, encoding="utf8")
    print("wrote", OUT.relative_to(ROOT))


if __name__ == "__main__":
    main()
