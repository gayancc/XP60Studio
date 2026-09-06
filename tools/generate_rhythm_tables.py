#!/usr/bin/env python3
"""Generate the XP-60 Rhythm Setup parameter tables from the protocol document.

Source of truth: docs/protocol/XP60_RHYTHM_PARAMETER_MAP.md (transcribed from
the Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map §1-4). This
script turns its Rhythm Common and Rhythm Note tables into
src/xpmodel/generated/Xp60RhythmTables.{h,cpp}, so every descriptor in code is
provably a row in the document.

The display/enumeration logic is shared with the Patch generator, and the parse
and emit are deliberately the same shape as the Performance generator's: three
documents, one set of conventions.

Usage:
    tools/generate_rhythm_tables.py            # rewrite the generated files
    tools/generate_rhythm_tables.py --check    # exit 1 if the files are stale
"""
from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))

from generate_patch_tables import (  # noqa: E402  (path set above)
    STATUS_MAP,
    Row,
    cpp_str,
    pascal,
    resolve_display,
    roland_offset,
    snake,
)

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / "docs" / "protocol" / "XP60_RHYTHM_PARAMETER_MAP.md"
OUT_DIR = ROOT / "src" / "xpmodel" / "generated"
OUT_H = OUT_DIR / "Xp60RhythmTables.h"
OUT_CPP = OUT_DIR / "Xp60RhythmTables.cpp"

# Sixty-four keys, 35..98, addressed by MIDI note number.
FIRST_KEY = 35
LAST_KEY = 98
KEY_COUNT = LAST_KEY - FIRST_KEY + 1

CATEGORY_RULES = [
    ("Rhythm Name", "Name"),
    ("EFX", "EFX"),
    ("Chorus", "Chorus"),
    ("Reverb", "Reverb"),
    ("Delay Feedback", "Reverb"),
    ("Wave", "Wave"),
    ("Tone Switch", "Wave"),
    ("Source Key", "Wave"),
    ("Mute Group", "Wave"),
    ("Envelope Mode", "Wave"),
    ("Bend Range", "Pitch"),
    ("Fine Tune", "Pitch"),
    ("Random Pitch", "Pitch"),
    ("Pitch Envelope", "Pitch Envelope"),
    ("Filter Envelope", "TVF Envelope"),
    ("Filter Type", "TVF"),
    ("Cutoff", "TVF"),
    ("Resonance", "TVF"),
    ("Level Envelope", "TVA Envelope"),
    ("Tone Level", "TVA"),
    ("Control Switch", "Controllers"),
    ("Pan", "Pan"),
    ("Output Assign", "Output"),
    ("Send Level", "Output"),
]


def category_for(row: Row) -> str:
    for needle, category in CATEGORY_RULES:
        if needle in row.name:
            return category
    return "Common" if row.block == "common" else "Part"


def ident_for(row: Row) -> Tuple[str, str]:
    m = re.match(r"^Rhythm Name (\d+)$", row.name)
    if m:
        return f"common.name.{m.group(1)}", f"RhythmName{m.group(1)}"
    return f"{row.block}.{snake(row.name)}", pascal(row.name)


def parse_tables(doc: str) -> Tuple[List[Row], Dict[str, int]]:
    """Rows of both tables, and each table's declared total size."""
    rows: List[Row] = []
    sizes: Dict[str, int] = {}
    block = None
    for line in doc.splitlines():
        if line.startswith("## 1. Rhythm Common"):
            block = "common"
        elif line.startswith("## 2. Rhythm Note"):
            block = "note"
        elif line.startswith("## ") and not line.startswith("## 1.") and not line.startswith("## 2."):
            block = None
        m = re.match(r"^Total size `([0-9A-F ]+)` \((\d+) bytes", line)
        if m and block:
            declared = m.group(1).split()
            if len(declared) != 4:
                raise SystemExit(f"total size {m.group(1)} is not a four-byte Roland size")
            value = int(declared[2], 16) * 128 + int(declared[3], 16)
            if value != int(m.group(2)):
                raise SystemExit(f"total size `{m.group(1)}` is not {m.group(2)} bytes")
            sizes[block] = value
        if block and line.startswith("| `00 "):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if len(cells) != 7:
                raise SystemExit(f"unexpected column count in row: {line}")
            offset, name, enc, raw, display, status, source = cells
            if enc.startswith("1 / ASCII"):
                encoding, byte_count = "Ascii", 1
            elif enc.startswith("2 / nibble"):
                encoding, byte_count = "Nibble", 2
            elif enc.startswith("1 / 7-bit"):
                encoding, byte_count = "SevenBit", 1
            else:
                raise SystemExit(f"unknown encoding '{enc}' for {name}")
            rm = re.match(r"^(-?\d+)\.\.(-?\d+)$", raw)
            if not rm:
                raise SystemExit(f"unparseable raw range '{raw}' for {name}")
            if status not in STATUS_MAP:
                raise SystemExit(f"unknown status '{status}' for {name}")
            rows.append(Row(block, roland_offset(offset), name, encoding, byte_count,
                            int(rm.group(1)), int(rm.group(2)), display, STATUS_MAP[status], source))
    for required in ("common", "note"):
        if required not in sizes:
            raise SystemExit(f"no total size found for Performance {required}")
    if not rows:
        raise SystemExit("no parameter rows found")
    return rows, sizes


def check_tiling(rows: List[Row], sizes: Dict[str, int]) -> None:
    """Every byte of each block is covered exactly once.

    This is the check that makes the transcription trustworthy: a dropped row or
    a mis-typed offset shifts everything after it, and silently decoding the
    wrong byte is far worse than failing here.
    """
    for block, size in sizes.items():
        covered: Dict[int, str] = {}
        for row in (r for r in rows if r.block == block):
            for byte in range(row.offset, row.offset + row.byte_count):
                if byte in covered:
                    raise SystemExit(f"{block}: byte {byte:#04x} claimed by both "
                                     f"'{covered[byte]}' and '{row.name}'")
                covered[byte] = row.name
        missing = [b for b in range(size) if b not in covered]
        if missing:
            raise SystemExit(f"{block}: {len(missing)} byte(s) uncovered, first {missing[0]:#04x}")
        beyond = [b for b in covered if b >= size]
        if beyond:
            raise SystemExit(f"{block}: byte {min(beyond):#04x} lies past the declared size {size}")


def label_arrays(rows: List[Row]) -> Tuple[List[str], Dict[Tuple[str, ...], str]]:
    """One constexpr array per distinct label set, shared between rows."""
    lines: List[str] = []
    names: Dict[Tuple[str, ...], str] = {}
    for row in rows:
        if not row.labels:
            continue
        key = tuple(row.labels)
        if key in names:
            continue
        name = f"kLabels{len(names) + 1}"
        names[key] = name
        joined = ", ".join(cpp_str(label) for label in row.labels)
        lines.append(f"constexpr std::array<std::string_view, {len(row.labels)}> {name}{{{{{joined}}}}};")
    return lines, names


def descriptor_literal(row: Row, label_names: Dict[Tuple[str, ...], str]) -> str:
    labels = label_names[tuple(row.labels)] if row.labels else "{}"
    return (f"    ParameterDescriptor{{{cpp_str(row.ident)}, {cpp_str(row.name)}, {row.offset}, "
            f"ParameterEncoding::{row.encoding}, {row.byte_count}, {row.raw_min}, {row.raw_max}, "
            f"{row.display_offset}, {row.display_scale}, DisplayStyle::{row.display_style}, "
            f"{cpp_str(row.unit)}, {labels}, {cpp_str(row.category)}, "
            f"VerificationStatus::{row.status}, {cpp_str(row.source)}}},")


def generate(rows: List[Row], sizes: Dict[str, int], digest: str) -> Tuple[str, str]:
    common = [r for r in rows if r.block == "common"]
    part = [r for r in rows if r.block == "note"]

    header = [
        "// GENERATED FILE — DO NOT EDIT.",
        "//",
        "// Produced by tools/generate_rhythm_tables.py from",
        "// docs/protocol/XP60_RHYTHM_PARAMETER_MAP.md (Roland XP-60/XP-80 MIDI",
        "// Implementation, Parameter Address Map §1-4). Edit the document and re-run",
        "// the generator; tst_rhythm_tables fails when this file is stale.",
        "//",
        f"// Source digest: {digest}",
        "#pragma once",
        "",
        '#include "xpmodel/ParameterTable.h"',
        "",
        "#include <array>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace xp60studio::xpmodel::xp60rhythm {",
        "",
        'inline constexpr std::string_view kSourceDocument = "docs/protocol/XP60_RHYTHM_PARAMETER_MAP.md";',
        f'inline constexpr std::string_view kSourceDigest = "{digest}";',
        "",
        f"inline constexpr std::uint32_t kRhythmCommonSize = {sizes['common']};",
        f"inline constexpr std::uint32_t kRhythmNoteSize = {sizes['note']};",
        f"inline constexpr int kFirstRhythmKey = {FIRST_KEY};",
        f"inline constexpr int kLastRhythmKey = {LAST_KEY};",
        f"inline constexpr int kRhythmNoteCount = {KEY_COUNT};",
        "// Byte offsets of the Notes, indexed by key - kFirstRhythmKey. A Note's",
        "// offset is its MIDI note number: Key# 35 lives at Roland 23 00.",
        "inline constexpr std::array<std::uint32_t, {}> kRhythmNoteOffsets{{{{{}}}}};".format(
            KEY_COUNT, ", ".join(str(roland_offset(f"{k:02X} 00")) for k in range(FIRST_KEY, LAST_KEY + 1))),
        "// Distance from the Rhythm Setup base to the byte after the last Note.",
        "inline constexpr std::uint32_t kRhythmSetupSpan = {};".format(
            roland_offset(f"{LAST_KEY:02X} 00") + sizes["note"]),
        "",
        "// One enumerator per Performance Common parameter, in table order.",
        "enum class RhythmCommonParameter : std::uint16_t {",
    ]
    for index, row in enumerate(common):
        header.append(f"    {row.enumerator}, // {index:3d}  {row.name}")
    header += ["    Count,", "};", "",
               "// One enumerator per Performance Part parameter, in table order.",
               "enum class RhythmNoteParameter : std::uint16_t {"]
    for index, row in enumerate(part):
        header.append(f"    {row.enumerator}, // {index:3d}  {row.name}")
    header += [
        "    Count,",
        "};",
        "",
        "[[nodiscard]] const ParameterTable& rhythmCommonTable() noexcept;",
        "[[nodiscard]] const ParameterTable& rhythmNoteTable() noexcept;",
        "[[nodiscard]] const ParameterDescriptor& descriptor(RhythmCommonParameter parameter) noexcept;",
        "[[nodiscard]] const ParameterDescriptor& descriptor(RhythmNoteParameter parameter) noexcept;",
        "",
        "} // namespace xp60studio::xpmodel::xp60rhythm",
        "",
    ]

    label_lines, label_names = label_arrays(rows)
    source = [
        "// GENERATED FILE — DO NOT EDIT. See Xp60RhythmTables.h for provenance.",
        f"// Source digest: {digest}",
        '#include "xpmodel/generated/Xp60RhythmTables.h"',
        "",
        '#include "xp60/Xp60Device.h"',
        "",
        "namespace xp60studio::xpmodel::xp60rhythm {",
        "",
        "namespace {",
        "",
        "using xp60::VerificationStatus;",
        "",
    ]
    source += label_lines
    source += ["", f"const std::array<ParameterDescriptor, {len(common)}> kCommonRows{{{{"]
    source += [descriptor_literal(row, label_names) for row in common]
    source += ["}};", "", f"const std::array<ParameterDescriptor, {len(part)}> kPartRows{{{{"]
    source += [descriptor_literal(row, label_names) for row in part]
    source += [
        "}};",
        "",
        "} // namespace",
        "",
        "const ParameterTable& rhythmCommonTable() noexcept",
        "{",
        '    static const ParameterTable kTable{"XP-60 Rhythm Common", kRhythmCommonSize,',
        "                                       std::span<const ParameterDescriptor>(kCommonRows.data(), kCommonRows.size()),",
        "                                       TableCompleteness::Complete,",
        '                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-4-1"};',
        "    return kTable;",
        "}",
        "",
        "const ParameterTable& rhythmNoteTable() noexcept",
        "{",
        '    static const ParameterTable kTable{"XP-60 Rhythm Note", kRhythmNoteSize,',
        "                                       std::span<const ParameterDescriptor>(kPartRows.data(), kPartRows.size()),",
        "                                       TableCompleteness::Complete,",
        '                                       "Roland XP-60/XP-80 MIDI Implementation, Parameter Address Map 1-4-2"};',
        "    return kTable;",
        "}",
        "",
        "const ParameterDescriptor& descriptor(RhythmCommonParameter parameter) noexcept",
        "{",
        "    return kCommonRows[static_cast<std::size_t>(parameter)];",
        "}",
        "",
        "const ParameterDescriptor& descriptor(RhythmNoteParameter parameter) noexcept",
        "{",
        "    return kPartRows[static_cast<std::size_t>(parameter)];",
        "}",
        "",
        "} // namespace xp60studio::xpmodel::xp60rhythm",
        "",
    ]
    return "\n".join(header), "\n".join(source)


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    text = DOC.read_text(encoding="utf-8")
    digest = "sha256:" + hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]
    rows, sizes = parse_tables(text)
    for row in rows:
        row.ident, row.enumerator = ident_for(row)
        row.category = category_for(row)
        resolve_display(row, {})
    check_tiling(rows, sizes)

    header, source = generate(rows, sizes, digest)
    if args.check:
        for path, produced in ((OUT_H, header), (OUT_CPP, source)):
            if not path.exists() or path.read_text(encoding="utf-8") != produced:
                print(f"{path.name} is stale; run tools/generate_rhythm_tables.py", file=sys.stderr)
                return 1
        return 0
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    OUT_H.write_text(header, encoding="utf-8", newline="\n")
    OUT_CPP.write_text(source, encoding="utf-8", newline="\n")
    print(f"{len([r for r in rows if r.block == 'common'])} Common + "
          f"{len([r for r in rows if r.block == 'note'])} Note rows written")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
