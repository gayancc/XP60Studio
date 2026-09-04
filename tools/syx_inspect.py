#!/usr/bin/env python3
"""Inspect a Roland .syx file for XP60Studio research work.

Research utility only (see AGENTS.md): it is not part of the production
runtime. It lists every message in the stream, validates Roland RQ1/DT1
framing and checksums, assembles the DT1 payloads into an address-indexed
image and prints the Patch names it finds at the documented XP-60 bases.

Usage:
    tools/syx_inspect.py FILE.syx [--model 6A] [--dump-image OUT.bin] [--verbose]

The Roland facts used here mirror src/xp60/Xp60Device.cpp and
docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md.
"""
from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

ROLAND_ID = 0x41
RQ1, DT1 = 0x11, 0x12
TEMP_PATCH = (0x03, 0x00, 0x00, 0x00)
USER_PATCH_BASE = (0x11, 0x00, 0x00, 0x00)
USER_PATCH_STRIDE = 1 << 14  # 00 01 00 00
NAME_LENGTH = 12


def linear(addr: Tuple[int, int, int, int]) -> int:
    return (addr[0] << 21) | (addr[1] << 14) | (addr[2] << 7) | addr[3]


def to_bytes(value: int) -> Tuple[int, int, int, int]:
    return ((value >> 21) & 0x7F, (value >> 14) & 0x7F, (value >> 7) & 0x7F, value & 0x7F)


def hexs(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)


def checksum(body: bytes) -> int:
    return (128 - (sum(body) % 128)) % 128


@dataclass
class Message:
    offset: int
    raw: bytes
    kind: str  # RQ1 / DT1 / roland-invalid / other-sysex / midi
    device: Optional[int] = None
    address: Optional[int] = None
    size: Optional[int] = None
    data: bytes = b""
    detail: str = ""


@dataclass
class Report:
    messages: List[Message] = field(default_factory=list)
    stray_bytes: int = 0
    aborted_sysex: int = 0


def split_stream(data: bytes) -> Report:
    report = Report()
    i, n = 0, len(data)
    while i < n:
        b = data[i]
        if b == 0xF0:
            end = data.find(b"\xF7", i + 1)
            # A status byte (other than realtime) before F7 aborts the SysEx.
            j = i + 1
            while j < n and (data[j] < 0x80 or data[j] >= 0xF8) and data[j] != 0xF7:
                j += 1
            if j >= n or data[j] != 0xF7:
                report.aborted_sysex += 1
                i = j if j < n else n
                continue
            raw = bytes(x for x in data[i : j + 1] if x < 0xF8 or x in (0xF7,))
            report.messages.append(Message(i, raw, "sysex"))
            i = j + 1
        elif b >= 0xF8:
            report.messages.append(Message(i, bytes([b]), "midi"))
            i += 1
        elif b >= 0x80:
            length = {0xC0: 2, 0xD0: 2}.get(b & 0xF0, 3) if b < 0xF0 else {0xF1: 2, 0xF3: 2, 0xF2: 3, 0xF6: 1}.get(b, 0)
            if length == 0 or i + length > n:
                report.stray_bytes += 1
                i += 1
                continue
            report.messages.append(Message(i, data[i : i + length], "midi"))
            i += length
        else:
            report.stray_bytes += 1
            i += 1
    return report


def decode_roland(msg: Message, model: bytes) -> None:
    raw = msg.raw
    if len(raw) < 2 or raw[1] != ROLAND_ID:
        msg.kind = "other-sysex"
        msg.detail = f"manufacturer {raw[1]:02X}" if len(raw) > 1 else "empty"
        return
    if len(raw) < 10:
        msg.kind, msg.detail = "roland-invalid", "truncated"
        return
    if not (0x10 <= raw[2] <= 0x1F):
        msg.kind, msg.detail = "roland-invalid", f"device id byte {raw[2]:02X} outside 10..1F"
        return
    msg.device = raw[2] + 1
    if raw[3 : 3 + len(model)] != model:
        msg.kind, msg.detail = "roland-invalid", f"model bytes {hexs(raw[3:5])} != {hexs(model)}"
        return
    cmd_at = 3 + len(model)
    cmd = raw[cmd_at]
    addr = raw[cmd_at + 1 : cmd_at + 5]
    body = raw[cmd_at + 5 : -2]
    csum = raw[-2]
    if any(x & 0x80 for x in addr) or any(x & 0x80 for x in body):
        msg.kind, msg.detail = "roland-invalid", "data byte with bit 7 set"
        return
    expected = checksum(addr + body)
    if csum != expected:
        msg.kind, msg.detail = "roland-invalid", f"checksum {csum:02X}, expected {expected:02X}"
        return
    msg.address = linear(tuple(addr))
    if cmd == RQ1:
        if len(body) != 4:
            msg.kind, msg.detail = "roland-invalid", "RQ1 size must be 4 bytes"
            return
        msg.kind, msg.size = "RQ1", linear(tuple(body))
    elif cmd == DT1:
        if not body:
            msg.kind, msg.detail = "roland-invalid", "DT1 without data"
            return
        msg.kind, msg.data, msg.size = "DT1", body, len(body)
    else:
        msg.kind, msg.detail = "roland-invalid", f"command {cmd:02X} unsupported"


def build_image(messages: List[Message]) -> Dict[int, int]:
    image: Dict[int, int] = {}
    for m in messages:
        if m.kind == "DT1" and m.address is not None:
            for k, byte in enumerate(m.data):
                image[m.address + k] = byte
    return image


def read_name(image: Dict[int, int], base: int) -> Optional[str]:
    chars = []
    for k in range(NAME_LENGTH):
        b = image.get(base + k)
        if b is None or not (0x20 <= b <= 0x7E):
            return None
        chars.append(chr(b))
    return "".join(chars).rstrip()


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("file")
    parser.add_argument("--model", default="6A", help="model ID hex bytes, default 6A (XP-60/XP-80)")
    parser.add_argument("--dump-image", help="write the assembled DT1 payload bytes (address-ordered) to a file")
    parser.add_argument("--verbose", action="store_true", help="print every message")
    args = parser.parse_args(argv)

    model = bytes.fromhex(args.model.replace(" ", ""))
    data = open(args.file, "rb").read()
    report = split_stream(data)
    for m in report.messages:
        if m.kind == "sysex":
            decode_roland(m, model)

    counts: Dict[str, int] = {}
    for m in report.messages:
        counts[m.kind] = counts.get(m.kind, 0) + 1
    print(f"{args.file}: {len(data)} bytes, {len(report.messages)} messages")
    for kind in ("DT1", "RQ1", "roland-invalid", "other-sysex", "midi"):
        if counts.get(kind):
            print(f"  {kind:15s} {counts[kind]}")
    if report.stray_bytes or report.aborted_sysex:
        print(f"  stray bytes {report.stray_bytes}, aborted SysEx {report.aborted_sysex}")

    if args.verbose:
        for m in report.messages:
            if m.kind in ("DT1", "RQ1"):
                a = " ".join(f"{x:02X}" for x in to_bytes(m.address))
                print(f"  @{m.offset:7d} {m.kind} dev={m.device} addr={a} size={m.size}")
            else:
                print(f"  @{m.offset:7d} {m.kind} {hexs(m.raw[:12])}{'...' if len(m.raw) > 12 else ''} {m.detail}")

    image = build_image(report.messages)
    if image:
        lo, hi = min(image), max(image)
        print(f"  image: {len(image)} bytes between {' '.join(f'{x:02X}' for x in to_bytes(lo))} and "
              f"{' '.join(f'{x:02X}' for x in to_bytes(hi))}")
        # DT1 packet size statistics (relevant to the 128-byte rule).
        sizes = sorted({m.size for m in report.messages if m.kind == "DT1"})
        print(f"  DT1 payload sizes seen: {sizes}")
        name = read_name(image, linear(TEMP_PATCH))
        if name is not None:
            print(f"  temporary patch name: '{name}'")
        found = []
        for n in range(1, 129):
            base = linear(USER_PATCH_BASE) + (n - 1) * USER_PATCH_STRIDE
            nm = read_name(image, base)
            if nm is not None:
                found.append((n, nm))
        if found:
            print(f"  user patch names ({len(found)}):")
            for n, nm in found:
                print(f"    USER:{n:03d} '{nm}'")
        if args.dump_image:
            with open(args.dump_image, "wb") as out:
                out.write(bytes(image[k] for k in sorted(image)))
            print(f"  image bytes written to {args.dump_image}")
    return 0 if not any(m.kind == "roland-invalid" for m in report.messages) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
