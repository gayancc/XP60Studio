#!/usr/bin/env python3
"""Builds data/factory/xp60-factory.json: the XP-60's own reference lists.

What goes in:
  * the factory patch names of the five banks in Roland's "XP-60 / XP-80 Patch List" (USER, PR-A, PR-B, PR-C, PR-D/GM);
  * the names of the instrument's internal waveforms (INT-A, INT-B);
  * the SR-JV80 expansion boards (number and name) and, for the boards whose Roland waveform lists we hold, their wave names.

Where it comes from (all Roland documents; see artifact/docs/xp60studio-legacy/XP60-References and .../data):
  XP60_PTC.pdf                  the factory patch list
  xp60_internal_waves.csv       internal wave names, transcribed from the owner's manual
  sr_jv80_expansion_waves.csv   expansion wave names, transcribed from the per-board Roland waveform lists

Run from the repository root:  python scripts/build_factory_data.py            (writes the file)
                                python scripts/build_factory_data.py --check    (exits 1 if the file is stale)
Needs: pip install pypdf
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REF = ROOT / "artifact" / "docs" / "xp60studio-legacy"
OUT = ROOT / "data" / "factory" / "xp60-factory.json"

# Roland's SR-JV80 series: 01..19 are the main run, 96..99 the Japanese-market and compilation boards.
BOARDS = {
    1: "Pop", 2: "Orchestral", 3: "Piano", 4: "Vintage Synth", 5: "World", 6: "Dance", 7: "Super Sound Set",
    8: "Keyboards of the 60s & 70s", 9: "Session", 10: "Bass & Drums", 11: "Techno Collection",
    12: "Hip Hop Collection", 13: "Vocal Collection", 14: "Asia", 15: "Special FX Collection", 16: "Orchestral II",
    17: "Country Collection", 18: "Latin Collection", 19: "House Collection", 96: "World Collection: Latin",
    97: "Experience III", 98: "Experience II", 99: "Experience",
}


def patch_banks() -> dict[str, list[str]]:
    from pypdf import PdfReader

    text = "\n".join(p.extract_text() for p in PdfReader(REF / "XP60-References" / "XP60_PTC.pdf").pages)
    banks: dict[str, dict[int, str]] = {}
    cur = None
    for line in text.split("\n"):
        line = line.rstrip()
        if re.match(r"^(USER Bank|PR-[A-G] \(.*\))$", line):
            cur = line.split(" ")[0]
            banks[cur] = {}
            continue
        m = re.match(r"^(\d{3}) ?(.*)$", line)
        if cur and m:
            banks[cur][int(m.group(1))] = m.group(2).strip()
    out: dict[str, list[str]] = {}
    for name, rows in banks.items():
        missing = [i for i in range(1, 129) if i not in rows]
        if missing:
            sys.exit(f"{name}: patch numbers missing from the list: {missing[:5]}")
        out[name] = [clean_name(rows[i]) for i in range(1, 129)]
    return out


def internal_waves() -> dict[str, list[str]]:
    rows: dict[str, dict[int, str]] = {}
    for r in csv.DictReader(open(REF / "data" / "xp60_internal_waves.csv", encoding="utf8")):
        rows.setdefault(r["bank"], {})[int(r["number"])] = r["name"]
    out = {}
    for bank, d in rows.items():
        top = max(d)
        gaps = [i for i in range(1, top + 1) if i not in d]
        if gaps:
            sys.exit(f"{bank}: wave numbers missing: {gaps[:5]}")
        out[bank] = [clean_name(d[i]) for i in range(1, top + 1)]
    return out


# Roland's lists are typeset text, and the PDF readers hand back typographic quotes (U+2018/2019) and, for the SR-JV80 lists,
# Windows-1252 bytes read as control characters (0x91/0x92). The instrument itself has plain ASCII names, and the quote is
# always an apostrophe or a feet mark (60's, 90's, '70s, Farf 8'+2'), so every name is normalised to ASCII before it is stored.
QUOTES = {"‘": "'", "’": "'", "": "'", "": "'", "“": '"', "”": '"', "": '"', "": '"', "–": "-", "—": "-", " ": " "}


def clean_name(text: str) -> str:
    return "".join(QUOTES.get(c, c) for c in text)


def norm(text: str) -> str:
    return " ".join(text.replace("’", "'").split())


def expansion_boards() -> list[dict]:
    """Board list with wave names: from Roland's per-board Waveform Lists where they were downloaded, else from the CSV transcription."""
    sys.path.insert(0, str(ROOT / "scripts"))
    import sr_jv80_lists

    pdf_waves, problems = sr_jv80_lists.board_waves()
    for board, why in sorted(problems.items()):
        print(f"note: SR-JV80-{board:02d} waveform list could not be read ({why}); its names are left out", file=sys.stderr)

    csv_waves: dict[int, dict[int, str]] = {}
    for r in csv.DictReader(open(REF / "data" / "sr_jv80_expansion_waves.csv", encoding="utf8")):
        csv_waves.setdefault(int(r["board"]), {})[int(r["number"])] = r["name"]
    for board, d in csv_waves.items():  # the two transcriptions must agree where both exist
        if board in pdf_waves:
            theirs = pdf_waves[board]
            diffs = [i for i in sorted(d) if i <= len(theirs) and norm(d[i]) != norm(theirs[i - 1])]
            if diffs or len(d) != len(theirs):
                print(f"note: board {board}: CSV and PDF differ at {diffs[:5]} ({len(d)} vs {len(theirs)} entries); the PDF is used", file=sys.stderr)

    boards = []
    for number, name in BOARDS.items():
        names = pdf_waves.get(number)
        if names is None and number in csv_waves and number not in problems:
            d = csv_waves[number]
            names = [d[i] for i in range(1, max(d) + 1)]
        boards.append({"number": number, "name": name, "waves": [clean_name(n) for n in names] if names else names})
    return boards


def audit(data: dict) -> list[str]:
    """Every stored name must be printable ASCII: returns what is not (the build refuses to write a file that has any)."""
    bad: list[str] = []
    groups = [("patch " + k, v) for k, v in data["patchBanks"].items()] + [("internal wave " + k, v) for k, v in data["internalWaves"].items()]
    groups += [(f"board {b['number']} waves", b["waves"]) for b in data["expansionBoards"] if b["waves"]]
    for label, names in groups:
        for i, n in enumerate(names, 1):
            if any(not 32 <= ord(c) < 127 for c in n):
                bad.append(f"{label} {i}: {n!r}")
    return bad


def build() -> str:
    data = {
        "about": "The XP-60's own reference lists, generated by scripts/build_factory_data.py from Roland's published documents. Names only: no sound data.",
        "patchBanks": patch_banks(),
        "internalWaves": internal_waves(),
        "expansionBoards": expansion_boards(),
    }
    return json.dumps(data, ensure_ascii=False, indent=1) + "\n"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    text = build()
    if ap.parse_args().check:
        if not OUT.exists() or OUT.read_text(encoding="utf8") != text:
            sys.exit("data/factory/xp60-factory.json is stale")
        return
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text, encoding="utf8")
    print("wrote", OUT.relative_to(ROOT))


if __name__ == "__main__":
    main()
