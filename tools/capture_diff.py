#!/usr/bin/env python3
"""Compare two Roland XP-60 SysEx captures and name every parameter that moved.

Research utility only (see AGENTS.md): it is not part of the production runtime.

The deferred physical-device sessions all have the same shape: capture the
instrument, change exactly one thing on the front panel, capture again, and
record which Patch bytes moved. `docs/PHASE_4_EFFECT_ROUTING.md` needs this to
establish the EFX Parameter 1-12 byte slots, `docs/PHASE_4_WAVE_BROWSER.md`
needs it at the INT-A/INT-B bank boundaries, and
`docs/HARDWARE_VALIDATION_XP60.md` step 8 needs it for read-back comparison.
Doing that by eye over 2 945 bytes per Patch is where mistakes get made.

This tool assembles the DT1 payloads of both files into address images,
compares them, and resolves each changed address against the same transcribed
Parameter Address Map that generates the C++ tables
(`docs/protocol/XP60_PATCH_PARAMETER_MAP.md`), so a reported name is provably a
row in the document rather than a guess.

Nothing is normalised away. Bytes present in only one capture, addresses
outside a documented Patch region, gaps between documented blocks and values
outside the transcribed raw range are all reported as such.

Usage:
    tools/capture_diff.py BEFORE.syx AFTER.syx
    tools/capture_diff.py BEFORE.syx AFTER.syx --markdown   # evidence rows
    tools/capture_diff.py --self-test
"""
from __future__ import annotations

import argparse
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))

import generate_patch_tables as tables  # noqa: E402
import syx_inspect as syx  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
MAP_DOC = ROOT / "docs" / "protocol" / "XP60_PATCH_PARAMETER_MAP.md"

# Documented Patch regions (docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md section 3).
PATCH_STRIDE = 1 << 14  # 00 01 00 00
# Performance-mode Part 10 is the Rhythm Setup, not a Patch.
RHYTHM_PART = 10

NOTE_NAMES = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")


@dataclass(frozen=True)
class Region:
    label: str      # "Temporary Patch (Patch mode)"
    base: int       # linear base address
    count: int      # how many patches follow at `stride`
    stride: int
    digits: int     # 0 when the label takes no index, otherwise its zero padding

    def name(self, part: int) -> str:
        return f"{self.label}{part:0{self.digits}d}" if self.digits else self.label


def regions() -> List[Region]:
    return [
        Region("Temporary Patch (Patch mode)", syx.linear((0x03, 0x00, 0x00, 0x00)), 1, PATCH_STRIDE, 0),
        Region("Temporary Patch, Performance Part ", syx.linear((0x02, 0x00, 0x00, 0x00)), 16, PATCH_STRIDE, 1),
        Region("User Patch USER:", syx.linear((0x11, 0x00, 0x00, 0x00)), 128, PATCH_STRIDE, 3),
    ]


# ---------------------------------------------------------------------------
# Parameter index, built from the transcribed Parameter Address Map
# ---------------------------------------------------------------------------


@dataclass
class ParameterIndex:
    """Byte offset -> parameter row, per block, plus the block geometry."""

    rows: Dict[str, List[tables.Row]] = field(default_factory=dict)
    by_offset: Dict[str, Dict[int, tables.Row]] = field(default_factory=dict)
    sizes: Dict[str, int] = field(default_factory=dict)
    tone_offsets: List[int] = field(default_factory=list)
    row_count: int = 0

    def block_at(self, patch_offset: int) -> Optional[Tuple[str, str, int]]:
        """(block key, display name, offset within the block) for a Patch offset."""
        if patch_offset < self.sizes["common"]:
            return ("common", "Patch Common", patch_offset)
        for i, base in enumerate(self.tone_offsets):
            if base <= patch_offset < base + self.sizes["tone"]:
                return ("tone", f"Tone {i + 1}", patch_offset - base)
        return None

    def span(self) -> int:
        return self.tone_offsets[-1] + self.sizes["tone"]


def build_index(doc_path: Path = MAP_DOC) -> ParameterIndex:
    rows, footnotes, sizes, tone_offsets = tables.parse_tables(doc_path.read_text(encoding="utf-8"))
    index = ParameterIndex(sizes=sizes, tone_offsets=tone_offsets, row_count=len(rows))
    for row in rows:
        tables.resolve_display(row, footnotes)
        row.category = tables.category_for(row)
        row.ident, row.enumerator = tables.ident_for(row)
        index.rows.setdefault(row.block, []).append(row)
        slot = index.by_offset.setdefault(row.block, {})
        for k in range(row.byte_count):
            if row.offset + k in slot:
                raise SystemExit(f"overlapping rows at {row.block} offset {row.offset + k}")
            slot[row.offset + k] = row
    return index


def decode_raw(row: tables.Row, byte_values: List[Optional[int]]) -> Optional[int]:
    """Roland raw value from the parameter's bytes; None if any byte is missing."""
    if any(b is None for b in byte_values):
        return None
    if row.encoding == "Nibble":
        value = 0
        for b in byte_values:
            if b > 0x0F:
                return None  # not a valid nibble byte; reported as unreadable
            value = (value << 4) | b
        return value
    return byte_values[0]


def format_display(row: tables.Row, raw: Optional[int]) -> str:
    """Mirror of ParameterDescriptor::formatDisplay so reports match the app."""
    if raw is None:
        return "-"
    if row.labels and row.raw_min <= raw <= row.raw_max:
        return row.labels[raw - row.raw_min]
    if row.encoding == "Ascii":
        ch = raw & 0x7F
        return repr(chr(ch)) if 0x20 <= ch <= 0x7E else f"0x{ch:02X}"
    display = raw * row.display_scale + row.display_offset
    if row.display_style == "Pan":
        if display < 0:
            return f"L{-display}"
        return f"{display}R" if display > 0 else "0"
    if row.display_style == "NoteName":
        note = max(0, display)
        return f"{NOTE_NAMES[note % 12]}{note // 12 - 1}"
    out = f"+{display}" if display > 0 and row.display_offset < 0 else str(display)
    return f"{out} {row.unit}".strip()


# ---------------------------------------------------------------------------
# Captures
# ---------------------------------------------------------------------------


@dataclass
class Capture:
    path: str
    image: Dict[int, int]
    dt1: int = 0
    invalid: List[str] = field(default_factory=list)
    payload_sizes: List[int] = field(default_factory=list)
    stray_bytes: int = 0
    aborted_sysex: int = 0


def read_capture_bytes(path: Path) -> bytes:
    """Accept either a binary .syx file or hex text.

    The Devices screen's Protocol activity panel yields copyable hex, so a
    hardware session can produce evidence without a separate SysEx utility.
    Text may contain `#` comments and any separators; only hex byte pairs are
    read. A file containing an F0 byte is treated as binary.
    """
    data = path.read_bytes()
    if b"\xF0" in data:
        return data
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        return data
    tokens: List[int] = []
    for line in text.splitlines():
        for token in line.split("#", 1)[0].replace(",", " ").split():
            token = token.removeprefix("0x").removeprefix("0X")
            if len(token) % 2 or not all(c in "0123456789abcdefABCDEF" for c in token):
                raise SystemExit(f"{path}: '{token}' is not a hex byte")
            tokens.extend(int(token[i:i + 2], 16) for i in range(0, len(token), 2))
    if not tokens:
        raise SystemExit(f"{path}: no SysEx bytes found")
    return bytes(tokens)


def load_capture(path: str, data: bytes, model: bytes) -> Capture:
    report = syx.split_stream(data)
    for message in report.messages:
        if message.kind == "sysex":
            syx.decode_roland(message, model)
    capture = Capture(
        path=path,
        image=syx.build_image(report.messages),
        dt1=sum(1 for m in report.messages if m.kind == "DT1"),
        payload_sizes=sorted({m.size for m in report.messages if m.kind == "DT1"}),
        stray_bytes=report.stray_bytes,
        aborted_sysex=report.aborted_sysex,
    )
    for message in report.messages:
        if message.kind in ("roland-invalid", "other-sysex"):
            capture.invalid.append(f"@{message.offset} {message.kind}: {message.detail}")
    return capture


# ---------------------------------------------------------------------------
# Address resolution
# ---------------------------------------------------------------------------


@dataclass
class Location:
    address: int
    region: str                  # human label, or "" when outside every region
    patch_base: Optional[int] = None
    block: str = ""              # "Patch Common" / "Tone 2"
    block_key: str = ""          # "common" / "tone"
    block_offset: int = -1
    row: Optional[tables.Row] = None
    note: str = ""               # why nothing more specific could be said

    @property
    def resolved(self) -> bool:
        return self.row is not None


def resolve(address: int, index: ParameterIndex) -> Location:
    for region in regions():
        offset_in_region = address - region.base
        if not (0 <= offset_in_region < region.count * region.stride):
            continue
        part = offset_in_region // region.stride + 1
        base = region.base + (part - 1) * region.stride
        offset = address - base
        label = region.name(part)
        if region.label.rstrip().endswith("Part") and part == RHYTHM_PART:
            return Location(address, label, base,
                            note="Performance Part 10 is the Rhythm Setup; the Rhythm Address Map "
                                 "is not transcribed (Phase 8)")
        if offset >= index.span():
            return Location(address, label, base,
                            note=f"offset {offset} is past Tone 4; the Parameter Address Map "
                                 f"defines {index.span()} bytes per Patch")
        placed = index.block_at(offset)
        if placed is None:
            return Location(address, label, base,
                            note=f"offset {offset} falls between documented blocks")
        block_key, block_name, block_offset = placed
        row = index.by_offset[block_key].get(block_offset)
        if row is None:
            return Location(address, label, base, block_name, block_key, block_offset,
                            note="no row in the Parameter Address Map covers this byte")
        return Location(address, label, base, block_name, block_key, block_offset, row)
    return Location(address, "", note="outside every documented Patch region")


# ---------------------------------------------------------------------------
# Diff
# ---------------------------------------------------------------------------


@dataclass
class Change:
    location: Location
    before_bytes: List[Optional[int]]
    after_bytes: List[Optional[int]]
    changed_offsets: List[int]

    @property
    def before_raw(self) -> Optional[int]:
        return decode_raw(self.location.row, self.before_bytes) if self.location.row else None

    @property
    def after_raw(self) -> Optional[int]:
        return decode_raw(self.location.row, self.after_bytes) if self.location.row else None

    def out_of_range(self) -> List[str]:
        row = self.location.row
        if row is None:
            return []
        out = []
        for tag, raw in (("before", self.before_raw), ("after", self.after_raw)):
            if raw is not None and not (row.raw_min <= raw <= row.raw_max):
                out.append(f"{tag} raw {raw} is outside the transcribed range {row.raw_min}..{row.raw_max}")
        return out


@dataclass
class Diff:
    changes: List[Change] = field(default_factory=list)
    only_before: List[int] = field(default_factory=list)
    only_after: List[int] = field(default_factory=list)
    shared: int = 0


def compare(before: Capture, after: Capture, index: ParameterIndex) -> Diff:
    diff = Diff()
    diff.only_before = sorted(set(before.image) - set(after.image))
    diff.only_after = sorted(set(after.image) - set(before.image))
    shared = sorted(set(before.image) & set(after.image))
    diff.shared = len(shared)

    claimed: set = set()
    for address in (a for a in shared if before.image[a] != after.image[a]):
        if address in claimed:
            continue
        location = resolve(address, index)
        row = location.row
        if row is None:
            diff.changes.append(Change(location, [before.image[address]], [after.image[address]], [address]))
            claimed.add(address)
            continue
        # A multi-byte parameter is reported once, with all of its bytes.
        first = address - (location.block_offset - row.offset)
        addresses = [first + k for k in range(row.byte_count)]
        claimed.update(addresses)
        diff.changes.append(Change(
            location=Location(first, location.region, location.patch_base, location.block,
                              location.block_key, row.offset, row),
            before_bytes=[before.image.get(a) for a in addresses],
            after_bytes=[after.image.get(a) for a in addresses],
            changed_offsets=[a for a in addresses if before.image.get(a) != after.image.get(a)],
        ))
    diff.changes.sort(key=lambda c: c.location.address)
    return diff


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------


def addr_text(address: int) -> str:
    return " ".join(f"{x:02X}" for x in syx.to_bytes(address))


def roland_offset_text(offset: int) -> str:
    return f"{offset // 128:02X} {offset % 128:02X}"


def byte_text(values: List[Optional[int]]) -> str:
    return " ".join(f"{b:02X}" if b is not None else "--" for b in values)


def summarise_ranges(addresses: List[int], limit: int = 4) -> str:
    if not addresses:
        return ""
    runs: List[Tuple[int, int]] = []
    start = previous = addresses[0]
    for address in addresses[1:]:
        if address == previous + 1:
            previous = address
            continue
        runs.append((start, previous))
        start = previous = address
    runs.append((start, previous))
    shown = ", ".join(f"{addr_text(a)}..{addr_text(b)}" if a != b else addr_text(a) for a, b in runs[:limit])
    return shown + (f", +{len(runs) - limit} more ranges" if len(runs) > limit else "")


def report_text(before: Capture, after: Capture, diff: Diff, limit: int) -> List[str]:
    out: List[str] = []
    for capture in (before, after):
        out.append(f"{capture.path}: {capture.dt1} DT1, {len(capture.image)} addressed bytes, "
                   f"payload sizes {capture.payload_sizes}")
        if capture.invalid:
            out.append(f"  {len(capture.invalid)} message(s) not usable as XP-60 DT1:")
            out.extend(f"    {line}" for line in capture.invalid[:limit])
        if capture.stray_bytes or capture.aborted_sysex:
            out.append(f"  stray bytes {capture.stray_bytes}, aborted SysEx {capture.aborted_sysex}")

    out.append("")
    out.append(f"{diff.shared} addresses in both captures, {len(diff.changes)} changed parameter(s)")
    if diff.only_before:
        out.append(f"  {len(diff.only_before)} address(es) only in the before capture: "
                   f"{summarise_ranges(diff.only_before)}")
    if diff.only_after:
        out.append(f"  {len(diff.only_after)} address(es) only in the after capture: "
                   f"{summarise_ranges(diff.only_after)}")
    if diff.only_before or diff.only_after:
        out.append("  Coverage differs, so those bytes are not a like-for-like comparison.")

    if not diff.changes:
        out.append("")
        out.append("No shared address changed value.")
        return out

    out.append("")
    for change in diff.changes[:limit]:
        location = change.location
        row = location.row
        head = f"{addr_text(location.address)}  {location.region or 'unknown region'}"
        if row is None:
            out.append(head)
            out.append(f"    raw {change.before_bytes[0]} -> {change.after_bytes[0]}  "
                       f"({location.note or 'unresolved'})")
            continue
        out.append(f"{head}  {location.block}")
        out.append(f"    {row.name}  [{row.category}]  block offset "
                   f"`{roland_offset_text(row.offset)}` ({row.offset}), {row.byte_count} byte(s), {row.encoding}")
        out.append(f"    raw {change.before_raw} -> {change.after_raw}    "
                   f"display {format_display(row, change.before_raw)} -> {format_display(row, change.after_raw)}")
        out.append(f"    bytes {byte_text(change.before_bytes)} -> {byte_text(change.after_bytes)}")
        out.append(f"    source: {row.source} ({row.status})")
        for warning in change.out_of_range():
            out.append(f"    ! {warning}")
    if len(diff.changes) > limit:
        out.append(f"... {len(diff.changes) - limit} further change(s); raise --limit to see them")
    return out


def report_markdown(before: Capture, after: Capture, diff: Diff, limit: int) -> List[str]:
    """Evidence rows in the shape PHASE_4_EFFECT_ROUTING.md asks for."""
    out = [
        f"Captures: `{before.path}` -> `{after.path}`",
        "",
        "| Address | Patch | Block | Parameter | Block offset | Raw before/after | Display before/after "
        "| Source | Status |",
        "|---|---|---|---|---|---|---|---|---|",
    ]
    for change in diff.changes[:limit]:
        location = change.location
        row = location.row
        region = location.region or "unknown region"
        if row is None:
            out.append(f"| `{addr_text(location.address)}` | {region} | Unresolved | "
                       f"{location.note or 'unknown'} | - | "
                       f"{change.before_bytes[0]} / {change.after_bytes[0]} | - / - | - | Unknown |")
            continue
        out.append(
            f"| `{addr_text(location.address)}` | {region} | {location.block} | {row.name} | "
            f"`{roland_offset_text(row.offset)}` | {change.before_raw} / {change.after_raw} | "
            f"{format_display(row, change.before_raw)} / {format_display(row, change.after_raw)} | "
            f"{row.source} | {row.status} |")
    if len(diff.changes) > limit:
        out.append(f"| ... | | | {len(diff.changes) - limit} further change(s) | | | | | |")
    if not diff.changes:
        out.append("| - | - | - | no shared address changed | - | - | - | - | - |")
    return out


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------


def dt1(address: Tuple[int, int, int, int], payload: bytes, device: int = 0x10, model: int = 0x6A) -> bytes:
    body = bytes(address) + payload
    return bytes([0xF0, syx.ROLAND_ID, device, model, syx.DT1]) + body + bytes([syx.checksum(body), 0xF7])


def self_test() -> int:
    index = build_index()
    common = {r.name: r for r in index.rows["common"]}
    tone = {r.name: r for r in index.rows["tone"]}
    failures: List[str] = []

    def check(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    span = index.span()

    def capture_of(image: bytearray, name: str) -> Capture:
        stream = b"".join(
            dt1((0x03, 0x00, (start // 128) & 0x7F, start % 128), bytes(image[start:start + 64]))
            for start in range(0, span, 64))
        return load_capture(name, stream, b"\x6a")

    before_image = bytearray(span)
    after_image = bytearray(before_image)

    # 1. A one-byte 7-bit Common parameter.
    efx5 = common["EFX Parameter 5"]
    after_image[efx5.offset] = 42
    # 2. A multi-byte nibble Tone parameter, inside Tone 2.
    wave = tone["Wave Number"]
    tone2 = index.tone_offsets[1] + wave.offset
    check(wave.byte_count >= 2, "expected Wave Number to be a nibble parameter")
    after_image[tone2] = 0x01
    after_image[tone2 + 1] = 0x0F
    # 3. A name character.
    after_image[common["Patch Name 3"].offset] = ord("Z")

    before = capture_of(before_image, "before")
    after = capture_of(after_image, "after")
    check(not before.invalid and not after.invalid, "synthetic captures should parse cleanly")

    diff = compare(before, after, index)
    check(len(diff.changes) == 3, f"expected 3 changed parameters, got {len(diff.changes)}")
    named = {c.location.row.name: c for c in diff.changes if c.location.row}
    check("EFX Parameter 5" in named, "EFX Parameter 5 was not resolved")
    check("Wave Number" in named, "Tone 2 Wave Number was not resolved")
    check("Patch Name 3" in named, "Patch Name 3 was not resolved")
    if "EFX Parameter 5" in named:
        change = named["EFX Parameter 5"]
        check(change.before_raw == 0 and change.after_raw == 42,
              f"EFX Parameter 5 raw {change.before_raw} -> {change.after_raw}, expected 0 -> 42")
        check(change.location.block == "Patch Common", "EFX Parameter 5 should sit in Patch Common")
    if "Wave Number" in named:
        change = named["Wave Number"]
        check(change.location.block == "Tone 2", f"Wave Number placed in {change.location.block}")
        check(change.after_raw == 0x1F, f"nibble decode gave {change.after_raw}, expected 31")
        check(len(change.changed_offsets) == 2, "both nibble bytes should be reported as changed")
    if "Patch Name 3" in named:
        check(format_display(named["Patch Name 3"].location.row, ord("Z")) == "'Z'",
              "ASCII display should show the character")

    # 4. Coverage differences are reported, never silently ignored.
    partial = load_capture("partial", dt1((0x03, 0x00, 0x00, 0x00), bytes(8)), b"\x6a")
    coverage = compare(before, partial, index)
    check(len(coverage.only_before) == span - 8,
          f"expected {span - 8} before-only addresses, got {len(coverage.only_before)}")
    check(not coverage.changes, "identical shared bytes must not be reported as changes")

    # 5. An address outside every documented Patch region stays unresolved.
    outside = resolve(syx.linear((0x00, 0x00, 0x00, 0x04)), index)
    check(not outside.resolved and bool(outside.note),
          "a System-area address should be unresolved with a stated reason")
    # 6. Performance Part 10 is the Rhythm Setup, not a Patch.
    rhythm = resolve(syx.linear((0x02, 0x09, 0x00, 0x00)), index)
    check(not rhythm.resolved and "Rhythm" in rhythm.note, "Part 10 should be reported as the Rhythm Setup")
    # 7. A gap between documented blocks is named as such.
    gap = resolve(syx.linear((0x03, 0x00, 0x00, 0x00)) + index.sizes["common"], index)
    check(not gap.resolved and "between documented blocks" in gap.note,
          "the byte after Patch Common should be reported as a gap")
    # 8. User Patch numbering follows the documented stride.
    user7 = resolve(syx.linear((0x11, 0x06, 0x00, 0x00)) + index.tone_offsets[3], index)
    check(user7.region == "User Patch USER:007" and user7.block == "Tone 4",
          f"USER:007 Tone 4 resolved as {user7.region!r} / {user7.block!r}")

    # 9. Out-of-range values are surfaced rather than clamped.
    structure = next(r for r in index.rows["common"] if r.name.startswith("Structure Type"))
    loud_image = bytearray(before_image)
    loud_image[structure.offset] = structure.raw_max + 1
    loud = compare(before, capture_of(loud_image, "loud"), index)
    check(any(c.out_of_range() for c in loud.changes), "an out-of-range raw value should be reported")

    # 10. Hex text pasted from the Protocol activity panel reads identically.
    message = dt1((0x03, 0x00, 0x00, 0x00), bytes([1, 2, 3, 4]))
    hex_text = "# copied from the Protocol activity panel\n" + " ".join(f"{b:02X}" for b in message) + "\n"
    with tempfile.TemporaryDirectory() as directory:
        text_path = Path(directory) / "capture.txt"
        text_path.write_text(hex_text, encoding="utf-8")
        binary_path = Path(directory) / "capture.syx"
        binary_path.write_bytes(message)
        check(read_capture_bytes(text_path) == message, "hex text should decode to the same bytes")
        check(read_capture_bytes(binary_path) == message, "a binary capture should be returned unchanged")

    # 11. A capture whose checksum is wrong is reported, not skipped.
    broken = bytearray(dt1((0x03, 0x00, 0x00, 0x00), bytes(4)))
    broken[-2] ^= 0x01
    check(bool(load_capture("broken", bytes(broken), b"\x6a").invalid), "a bad checksum must be reported")

    for line in failures:
        print(f"FAIL: {line}", file=sys.stderr)
    if failures:
        return 1
    print(f"capture_diff self-test passed ({index.row_count} parameter rows, {span} bytes per Patch)")
    return 0


# ---------------------------------------------------------------------------


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("before", nargs="?", help="capture taken before the change")
    parser.add_argument("after", nargs="?", help="capture taken after the change")
    parser.add_argument("--model", default="6A", help="model ID hex bytes, default 6A (XP-60/XP-80)")
    parser.add_argument("--markdown", action="store_true", help="print evidence rows instead of a text report")
    parser.add_argument("--limit", type=int, default=40, help="maximum changes to print, default 40")
    parser.add_argument("--self-test", action="store_true", help="verify the tool against synthetic captures")
    args = parser.parse_args(argv)

    if args.self_test:
        return self_test()
    if not args.before or not args.after:
        parser.error("BEFORE.syx and AFTER.syx are required unless --self-test is given")

    model = bytes.fromhex(args.model.replace(" ", ""))
    index = build_index()
    before = load_capture(args.before, read_capture_bytes(Path(args.before)), model)
    after = load_capture(args.after, read_capture_bytes(Path(args.after)), model)
    diff = compare(before, after, index)

    lines = (report_markdown(before, after, diff, args.limit) if args.markdown
             else report_text(before, after, diff, args.limit))
    print("\n".join(lines))
    # A capture that did not parse cleanly is not usable as evidence.
    return 1 if before.invalid or after.invalid else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
