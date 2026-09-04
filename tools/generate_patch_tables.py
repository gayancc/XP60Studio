#!/usr/bin/env python3
"""Generate the XP-60 Patch parameter tables from the protocol document.

Source of truth: docs/protocol/XP60_PATCH_PARAMETER_MAP.md (transcribed from the
Roland XP-60/XP-80 MIDI Implementation). This script turns its Patch Common
and Patch Tone tables into src/xpmodel/generated/Xp60PatchTables.{h,cpp} so
that every descriptor in code is provably a row in the document.

Usage:
    tools/generate_patch_tables.py            # rewrite the generated files
    tools/generate_patch_tables.py --check    # exit 1 if the files are stale

The generated files are committed; a CTest runs --check when Python is available.
"""
from __future__ import annotations

import argparse
import hashlib
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / "docs" / "protocol" / "XP60_PATCH_PARAMETER_MAP.md"
OUT_DIR = ROOT / "src" / "xpmodel" / "generated"
OUT_H = OUT_DIR / "Xp60PatchTables.h"
OUT_CPP = OUT_DIR / "Xp60PatchTables.cpp"

STATUS_MAP = {
    "Documentation-derived": "DocumentationDerived",
    "Project-defined": "ProjectDefined",
    "Hardware-verified": "HardwareVerified",
    "Unknown": "Unknown",
}


@dataclass
class Row:
    block: str
    offset: int
    name: str
    encoding: str  # SevenBit / Nibble / Ascii
    byte_count: int
    raw_min: int
    raw_max: int
    display: str
    status: str
    source: str
    # derived
    ident: str = ""
    enumerator: str = ""
    display_offset: int = 0
    display_scale: int = 1
    display_style: str = "Number"
    labels: List[str] = field(default_factory=list)
    category: str = ""
    unit: str = ""


def roland_offset(text: str) -> int:
    hi, lo = text.strip("` ").split()
    return int(hi, 16) * 128 + int(lo, 16)


def parse_int(text: str) -> int:
    return int(text.strip().replace("+", ""))


def parse_tables(doc: str) -> Tuple[List[Row], Dict[int, List[str]], Dict[str, int], List[int]]:
    rows: List[Row] = []
    block: Optional[str] = None
    footnotes: Dict[int, List[str]] = {}
    sizes: Dict[str, int] = {}
    tone_offsets: List[int] = []
    for line in doc.splitlines():
        if line.startswith("## 1. Patch Common"):
            block = "common"
        elif line.startswith("## 2. Patch Tone"):
            block = "tone"
        elif line.startswith("## 3."):
            block = None
        m = re.match(r"^\*\*Patch (Common|Tone) total size:\*\* `([0-9A-F ]+)` = \*\*(\d+) bytes\*\*", line)
        if m:
            sizes[m.group(1).lower()] = int(m.group(3))
            declared = roland_offset(m.group(2)[6:]) if len(m.group(2)) > 5 else None
            if declared is not None and declared != int(m.group(3)):
                raise SystemExit(f"size notation {m.group(2)} does not equal {m.group(3)} bytes")
        m = re.match(r"^(\d+)\. [^:`]+: `([^`]+)`\s*$", line)
        if m:
            footnotes[int(m.group(1))] = [x.strip() for x in m.group(2).split(",")]
        m = re.match(r"^\| `(\d\d \d\d)` \| Patch Tone (\d) \|", line)
        if m:
            tone_offsets.append(roland_offset(m.group(1)))
        if block and line.startswith("| `"):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if len(cells) != 7:
                raise SystemExit(f"unexpected column count in row: {line}")
            offset, name, enc, raw, display, status, source = cells
            if enc.startswith("1 / ASCII"):
                encoding, byte_count = "Ascii", 1
            elif enc.startswith("2 / nibble"):
                encoding, byte_count = "Nibble", 2
            elif enc.startswith("4 / nibble"):
                encoding, byte_count = "Nibble", 4
            elif enc.startswith("1 / 7-bit"):
                encoding, byte_count = "SevenBit", 1
            else:
                raise SystemExit(f"unknown encoding '{enc}' for {name}")
            rm = re.match(r"^(-?\d+)\.\.(-?\d+)$", raw)
            if not rm:
                raise SystemExit(f"unparseable raw range '{raw}' for {name}")
            if status not in STATUS_MAP:
                raise SystemExit(f"unknown status '{status}' for {name}")
            rows.append(Row(block, roland_offset(offset), name, encoding, byte_count, int(rm.group(1)), int(rm.group(2)),
                            display, STATUS_MAP[status], source))
    if not rows or "common" not in sizes or "tone" not in sizes or len(tone_offsets) != 4:
        raise SystemExit("document structure not recognised (rows / sizes / tone offsets)")
    return rows, footnotes, sizes, tone_offsets


def pascal(name: str) -> str:
    tokens = re.findall(r"[A-Za-z0-9]+", name.replace("&", " "))
    return "".join(t[:1].upper() + t[1:].lower() for t in tokens)


def snake(name: str) -> str:
    tokens = re.findall(r"[A-Za-z0-9]+", name.replace("&", " "))
    return "_".join(t.lower() for t in tokens)


def ident_for(row: Row) -> Tuple[str, str]:
    m = re.match(r"^Patch Name (\d+)$", row.name)
    if m:
        return f"common.name.{m.group(1)}", f"PatchName{m.group(1)}"
    return f"{row.block}.{snake(row.name)}", pascal(row.name)


def expand_list(text: str) -> List[str]:
    out: List[str] = []
    for item in [x.strip().strip("`") for x in text.split(",")]:
        m = re.match(r"^(\d+)\.\.(\d+)$", item)
        if m:
            out.extend(str(v) for v in range(int(m.group(1)), int(m.group(2)) + 1))
        else:
            out.append(item)
    return out


def resolve_display(row: Row, footnotes: Dict[int, List[str]]) -> None:
    d = row.display
    count = row.raw_max - row.raw_min + 1
    if d == "character":
        return
    if d == "same as raw" or d.startswith("effect-specific"):
        return
    m = re.match(r"^see Tone footnote (\d+)$", d)
    if m:
        labels = footnotes.get(int(m.group(1)))
        if labels is None:
            raise SystemExit(f"{row.name}: footnote {m.group(1)} not found")
        if len(labels) != count:
            raise SystemExit(f"{row.name}: footnote {m.group(1)} has {len(labels)} labels for {count} values")
        row.labels = labels
        return
    m = re.match(r"^L(\d+)\.\.(\d+)R$", d)
    if m:
        row.display_style = "Pan"
        row.display_offset = -int(m.group(1)) - row.raw_min
        if row.raw_max + row.display_offset != int(m.group(2)):
            raise SystemExit(f"{row.name}: pan range {d} does not match raw {row.raw_min}..{row.raw_max}")
        return
    if d in ("C-1..Upper", "Lower..G9", "C-1..G9"):
        row.display_style = "NoteName"
        return
    if d in ("1..Upper", "Lower..127"):
        return
    m = re.match(r"^([+-]?\d+)\.\.([+-]?\d+)$", d)
    if m:
        d0, d1 = parse_int(m.group(1)), parse_int(m.group(2))
        raw_span = row.raw_max - row.raw_min
        if raw_span == 0:
            raise SystemExit(f"{row.name}: degenerate raw range")
        if (d1 - d0) % raw_span != 0:
            raise SystemExit(f"{row.name}: display {d} is not an integer scaling of raw {row.raw_min}..{row.raw_max}")
        row.display_scale = (d1 - d0) // raw_span
        row.display_offset = d0 - row.display_scale * row.raw_min
        return
    # Comma separated enumeration (possibly containing sub-ranges such as "OFF, 1..3").
    labels = expand_list(d)
    if len(labels) != count:
        raise SystemExit(f"{row.name}: {len(labels)} labels for {count} raw values in '{d}'")
    row.labels = labels


CATEGORY_RULES = [
    ("Patch Name", "Name"),
    ("EFX", "EFX"),
    ("Chorus", "Chorus"),
    ("Reverb", "Reverb"),
    ("Delay Feedback", "Reverb"),
    ("Portamento", "Portamento"),
    ("Structure", "Structure"),
    ("Booster", "Structure"),
    ("Wave", "Wave"),
    ("FXM", "Wave"),
    ("Tone Delay", "Tone Delay"),
    ("Velocity Range", "Range"),
    ("Keyboard Range", "Range"),
    ("Velocity Cross Fade", "Range"),
    ("Control Switch", "Control Switches"),
    ("Controller", "Controllers"),
    ("LFO1", "LFO1"),
    ("LFO2", "LFO2"),
    ("Pitch", "Pitch"),
    ("Coarse Tune", "Pitch"),
    ("Fine Tune", "Pitch"),
    ("Filter", "TVF"),
    ("Cutoff", "TVF"),
    ("Resonance", "TVF"),
    ("Level Envelope", "TVA Envelope"),
    ("Level LFO", "TVA"),
    ("Tone Level", "TVA"),
    ("Bias", "TVA"),
    ("Pan", "Pan"),
    ("Output Assign", "Output"),
    ("Send Level", "Output"),
]


def category_for(row: Row) -> str:
    for needle, category in CATEGORY_RULES:
        if needle in row.name:
            if category == "Pitch" and "Envelope" in row.name:
                return "Pitch Envelope"
            if category == "TVF" and "Envelope" in row.name:
                return "TVF Envelope"
            return category
    return "Common" if row.block == "common" else "Tone"


def cpp_str(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def generate(rows: List[Row], footnotes: Dict[int, List[str]], sizes: Dict[str, int], tone_offsets: List[int],
             digest: str) -> Tuple[str, str]:
    for row in rows:
        row.ident, row.enumerator = ident_for(row)
        resolve_display(row, footnotes)
        row.category = category_for(row)

    # Structural checks: contiguous, in order, total size.
    for block in ("common", "tone"):
        cursor = 0
        for row in [r for r in rows if r.block == block]:
            if row.offset != cursor:
                raise SystemExit(f"{block}: {row.name} at {row.offset}, expected {cursor} (gap or overlap)")
            cursor += row.byte_count
        if cursor != sizes[block]:
            raise SystemExit(f"{block}: rows cover {cursor} bytes but the documented size is {sizes[block]}")
    names = [r.enumerator for r in rows if r.block == "common"] + [r.enumerator for r in rows if r.block == "tone"]
    for block in ("common", "tone"):
        enums = [r.enumerator for r in rows if r.block == block]
        if len(set(enums)) != len(enums):
            raise SystemExit(f"{block}: duplicate enumerator names")

    header = f"""// GENERATED FILE — DO NOT EDIT.
//
// Produced by tools/generate_patch_tables.py from
// docs/protocol/XP60_PATCH_PARAMETER_MAP.md (Roland XP-60/XP-80 MIDI
// Implementation, Parameter Address Map, pp.223-225). Edit the document and
// re-run the generator; tst_generated_tables fails when this file is stale.
//
// Source digest: {digest}
#pragma once

#include "xpmodel/ParameterTable.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace xp60studio::xpmodel::xp60tables {{

inline constexpr std::string_view kSourceDocument = "docs/protocol/XP60_PATCH_PARAMETER_MAP.md";
inline constexpr std::string_view kSourceDigest = "{digest}";

inline constexpr std::uint32_t kPatchCommonSize = {sizes['common']};
inline constexpr std::uint32_t kPatchToneSize = {sizes['tone']};
// Byte offsets of Tone 1..4 within a Patch (Roland 10 00, 12 00, 14 00, 16 00).
inline constexpr std::array<std::uint32_t, 4> kToneOffsets{{{{{', '.join(str(o) for o in tone_offsets)}}}}};
// Distance from the Patch base to the byte after Tone 4.
inline constexpr std::uint32_t kPatchSpan = {tone_offsets[-1] + sizes['tone']};

// One enumerator per Patch Common parameter, in table order.
enum class CommonParameter : std::uint16_t {{
"""
    for r in [r for r in rows if r.block == "common"]:
        header += f"    {r.enumerator}, // {r.offset:3d}  {r.name}\n"
    header += """    Count,
};

// One enumerator per Patch Tone parameter, in table order.
enum class ToneParameter : std::uint16_t {
"""
    for r in [r for r in rows if r.block == "tone"]:
        header += f"    {r.enumerator}, // {r.offset:3d}  {r.name}\n"
    header += """    Count,
};

[[nodiscard]] const ParameterTable& patchCommonTable() noexcept;
[[nodiscard]] const ParameterTable& patchToneTable() noexcept;
[[nodiscard]] const ParameterDescriptor& descriptor(CommonParameter parameter) noexcept;
[[nodiscard]] const ParameterDescriptor& descriptor(ToneParameter parameter) noexcept;

} // namespace xp60studio::xpmodel::xp60tables
"""

    cpp = f"""// GENERATED FILE — DO NOT EDIT. See Xp60PatchTables.h for provenance.
// Source digest: {digest}
#include "xpmodel/generated/Xp60PatchTables.h"

#include "xp60/Xp60Device.h"

namespace xp60studio::xpmodel::xp60tables {{

namespace {{

using xp60::VerificationStatus;

"""
    # Label arrays (deduplicated by content).
    label_sets: Dict[Tuple[str, ...], str] = {}
    for r in rows:
        if r.labels:
            key = tuple(r.labels)
            if key not in label_sets:
                label_sets[key] = f"kLabels{len(label_sets) + 1}"
    for key, name in label_sets.items():
        cpp += f"constexpr std::array<std::string_view, {len(key)}> {name}{{{{{', '.join(cpp_str(x) for x in key)}}}}};\n"
    cpp += "\n"

    def emit_rows(block: str, array_name: str) -> str:
        block_rows = [r for r in rows if r.block == block]
        out = f"const std::array<ParameterDescriptor, {len(block_rows)}> {array_name}{{{{\n"
        for r in block_rows:
            labels = f"std::span<const std::string_view>({label_sets[tuple(r.labels)]})" if r.labels else "{}"
            out += ("    ParameterDescriptor{" + f"{cpp_str(r.ident)}, {cpp_str(r.name)}, {r.offset}, ParameterEncoding::{r.encoding}, "
                    f"{r.byte_count}, {r.raw_min}, {r.raw_max}, {r.display_offset}, {r.display_scale}, DisplayStyle::{r.display_style}, "
                    f"{cpp_str(r.unit)}, {labels}, {cpp_str(r.category)}, VerificationStatus::{r.status}, {cpp_str(r.source)}" + "},\n")
        out += "}};\n\n"
        return out

    cpp += emit_rows("common", "kCommonRows")
    cpp += emit_rows("tone", "kToneRows")
    cpp += f"""}} // namespace

const ParameterTable& patchCommonTable() noexcept
{{
    static const ParameterTable kTable{{"XP-60 Patch Common", kPatchCommonSize,
                                       std::span<const ParameterDescriptor>(kCommonRows.data(), kCommonRows.size()),
                                       TableCompleteness::Complete,
                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-3-1 (pp.223-224)"}};
    return kTable;
}}

const ParameterTable& patchToneTable() noexcept
{{
    static const ParameterTable kTable{{"XP-60 Patch Tone", kPatchToneSize,
                                       std::span<const ParameterDescriptor>(kToneRows.data(), kToneRows.size()),
                                       TableCompleteness::Complete,
                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-3-2 (pp.224-225)"}};
    return kTable;
}}

const ParameterDescriptor& descriptor(CommonParameter parameter) noexcept
{{
    return kCommonRows[static_cast<std::size_t>(parameter)];
}}

const ParameterDescriptor& descriptor(ToneParameter parameter) noexcept
{{
    return kToneRows[static_cast<std::size_t>(parameter)];
}}

}} // namespace xp60studio::xpmodel::xp60tables
"""
    return header, cpp


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="verify the generated files are up to date")
    args = parser.parse_args(argv)

    doc = DOC.read_text(encoding="utf-8")
    rows, footnotes, sizes, tone_offsets = parse_tables(doc)
    # Digest of the rows that matter (not of prose), so wording edits do not force regeneration.
    material = "\n".join(f"{r.block}|{r.offset}|{r.name}|{r.encoding}|{r.byte_count}|{r.raw_min}|{r.raw_max}|{r.display}|{r.status}"
                         for r in rows)
    material += "\n" + repr(sorted(footnotes.items())) + repr(sizes) + repr(tone_offsets)
    digest = "sha256:" + hashlib.sha256(material.encode("utf-8")).hexdigest()[:16]
    header, cpp = generate(rows, footnotes, sizes, tone_offsets, digest)

    if args.check:
        stale = []
        for path, content in ((OUT_H, header), (OUT_CPP, cpp)):
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                stale.append(str(path.relative_to(ROOT)))
        if stale:
            print("generated tables are stale; run tools/generate_patch_tables.py:", *stale, file=sys.stderr)
            return 1
        print(f"generated tables up to date ({len(rows)} rows, digest {digest})")
        return 0

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    OUT_H.write_text(header, encoding="utf-8")
    OUT_CPP.write_text(cpp, encoding="utf-8")
    common = sum(1 for r in rows if r.block == "common")
    print(f"wrote {OUT_H.relative_to(ROOT)} and {OUT_CPP.relative_to(ROOT)}: {common} common + {len(rows) - common} tone rows, digest {digest}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
