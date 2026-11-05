"""Reads wave names out of Roland's SR-JV80 "Waveform List" PDFs (downloaded to artifact/references/roland-sr-jv80 by fetch.py).

The PDFs come in two layouts: a table of columns ("001 Grand sft 1A 017 SApiano p 1B ...") and one entry per line ("1 AcGtr Pick A").
Both are read, then checked: the numbers must run 1..N with no gap and no repeat, or the board is reported and left out.
"""
from __future__ import annotations

import re
from pathlib import Path

REFS = Path(__file__).resolve().parents[1] / "artifact" / "references" / "roland-sr-jv80"

# a three-digit zero-padded entry number that is not part of a word or a "120:" style prefix
_COLUMN = re.compile(r"(?<![\w:.\-])(\d{3})(?=\s)")
_SINGLE = re.compile(r"^\s*(\d{1,3})\s+(\S.*)$")
MAX_WAVE = 255  # a wave number is one byte (nibble pair) in the instrument and Roland numbers stop at 255

# Entries whose name is missing from Roland's PDF text (the row is cut off in the document): left blank rather than guessed.
# SR-JV80-12 (Hip Hop): the row that holds 022, 067 and 112 stops after "112"; 112 sits between "Voice MENU 1" and "Voice MENU 3" and
# 152 between "Jungle Kick" and "Ninja Kick", but their names are not printed, so they stay unnamed.
BLANKS: dict[int, set[int]] = {12: {112, 152}}

# Printing slips in Roland's own lists, repaired by hand and stated here: (board, the text as printed) -> the number it must be.
FIXES: dict[tuple[int, str], int] = {
    # SR-JV80-03 prints "039 E. Grand 2" between wave 28 (E. Grand 1C) and wave 30 (Rhodes 1 p); 29 is the only number missing
    (3, "039 E. Grand 2"): 29,
}
_HEADER = re.compile(r"(Roland Corporation|For use with|Waveform List|Waveform Listing|Faxback|Expansion Board)", re.I)


def _clean(name: str) -> str:
    name = re.sub(r"\s+", " ", name.replace("’", "'")).strip()
    return re.sub(r"\s*\*\d*$", "", name)  # Roland's footnote marker ("Bodhran MENU *1")


def parse_text(text: str, board: int = 0) -> dict[int, str]:
    entries: dict[int, str] = {}
    dupes: list[int] = []

    def put(n: int, name: str) -> None:
        name = _clean(name)
        if not name:
            return
        if n in entries and entries[n] != name:
            dupes.append(n)
        entries[n] = name

    for line in text.split("\n"):
        line = line.rstrip()
        if not line or _HEADER.search(line):
            continue
        hits = list(_COLUMN.finditer(line))
        if len(hits) >= 1 and (len(hits) >= 2 or line.lstrip().startswith(hits[0].group(1))):
            hits = [m for m in hits if 1 <= int(m.group(1)) <= MAX_WAVE] or hits
            for i, m in enumerate(hits):
                end = hits[i + 1].start() if i + 1 < len(hits) else len(line)
                n = int(m.group(1))
                n = FIXES.get((board, f"{m.group(1)} {_clean(line[m.end():end])}"), n)
                if 1 <= n <= MAX_WAVE:
                    put(n, line[m.end():end])
            continue
        m = _SINGLE.match(line)
        if m and 1 <= int(m.group(1)) <= MAX_WAVE:
            put(int(m.group(1)), m.group(2))
    if dupes:
        raise ValueError(f"conflicting repeats of entry numbers {sorted(set(dupes))[:6]}")
    return entries


def waves_from_pdf(path: Path, board: int = 0) -> list[str]:
    from pypdf import PdfReader

    text = "\n".join(p.extract_text() or "" for p in PdfReader(path).pages)
    entries = parse_text(text, board)
    if not entries:
        raise ValueError("no entries found")
    top = max(entries)
    gaps = [i for i in range(1, top + 1) if i not in entries and i not in BLANKS.get(board, set())]
    if gaps:
        raise ValueError(f"missing entry numbers {gaps[:8]} (of {top})")
    return [entries.get(i, "") for i in range(1, top + 1)]


def board_waves() -> tuple[dict[int, list[str]], dict[int, str]]:
    """Wave names per board number, and the reason for each board that could not be read."""
    out: dict[int, list[str]] = {}
    problems: dict[int, str] = {}
    for pdf in sorted(REFS.glob("sr-jv80-*-wave-*.pdf")):
        board = int(pdf.name.split("-")[2])
        try:
            out[board] = waves_from_pdf(pdf, board)
        except Exception as e:  # reported, never silently skipped
            problems[board] = str(e)
    return out, problems


if __name__ == "__main__":
    waves, problems = board_waves()
    for b, w in sorted(waves.items()):
        print(f"board {b:02d}: {len(w)} waves, first {w[0]!r}, last {w[-1]!r}")
    for b, why in sorted(problems.items()):
        print(f"board {b:02d}: NOT READ: {why}")
