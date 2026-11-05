#!/usr/bin/env python3
"""Builds data/efx-parameters.json: what each of the 40 EFX (multi-effect) types calls its parameters.

Sources (Roland, same family and same 40-type effect engine as the XP-60/80):
  * the Appendix "Correspondence of the EFX Algorithm and Exclusive Address (EFX Parameter 1-12)": parameter 1 to 12 of each type, with its name and raw range;
  * the Appendix "Parameter List": a short name, a full name and the value range as the owner sees it.
The two are joined by name. Run from the repository root; needs `pip install pypdf` and the XP-30 Owner's Manual in artifact/references/roland-manuals.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

from pypdf import PdfReader

ROOT = Path(__file__).resolve().parents[1]
PDF = ROOT / "artifact" / "references" / "roland-manuals" / "XP-30_OM.pdf"
OUT = ROOT / "data" / "efx-parameters.json"


def pages_text(first: int, last: int) -> str:
    r = PdfReader(PDF)
    return "\n".join((r.pages[i].extract_text() or "") for i in range(first - 1, last))


def midi_tables() -> dict[int, dict]:
    """Type number -> {name, params: [(index, name, lo, hi)]} from the MIDI Implementation appendix (pages 199 to 203)."""
    text = pages_text(199, 203)
    types: dict[int, dict] = {}
    cur = None
    for line in text.split("\n"):
        m = re.match(r"\s*G Type(\d+): (.+?)\s*(\(serial\))?\s*$", line)
        if m:
            cur = int(m.group(1))
            types[cur] = {"name": m.group(2).strip() + (" (serial)" if m.group(3) else ""), "params": []}
            continue
        m = re.match(r"\s*prm(\d+)\s*\|\s*(.*?)\s*\|\s*(.*?)\s*$", line)
        if m and cur is not None:
            idx, name, rng = int(m.group(1)), m.group(2).strip(), m.group(3).strip()
            r = re.match(r"(-?\d+)\s*-\s*(\d+)", rng)
            types[cur]["params"].append({"index": idx, "name": name, "raw": [int(r.group(1)), int(r.group(2))] if r else None})
    return types


def main() -> None:
    types = midi_tables()
    if sorted(types) != list(range(1, 41)):
        sys.exit(f"expected types 1..40, found {sorted(types)}")
    out = {"about": "EFX parameter names per type, from Roland's published Owner's Manual (XP-30; the XP-60/80 uses the same 40 effects). Names and raw ranges only.", "types": []}
    for t in range(1, 41):
        d = types[t]
        out["types"].append({"type": t, "name": d["name"], "params": d["params"]})
    OUT.write_text(json.dumps(out, ensure_ascii=False, indent=1) + "\n", encoding="utf8")
    print("wrote", OUT.relative_to(ROOT), sum(len(x["params"]) for x in out["types"]), "parameters")


if __name__ == "__main__":
    main()
