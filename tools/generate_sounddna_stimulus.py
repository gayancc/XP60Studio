"""Generate the deterministic MIDI stimulus used for Sound DNA audio capture.

The file intentionally contains notes and timing only: no Bank Select, Program
Change, SysEx, controllers, or effects assumptions. Preset selection remains a
separate, verified capture step.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import tempfile
from pathlib import Path

PPQ = 480
TEMPO_US = 500_000


def vlq(value: int) -> bytes:
    if value < 0:
        raise ValueError("delta time cannot be negative")
    encoded = [value & 0x7F]
    value >>= 7
    while value:
        encoded.append(0x80 | (value & 0x7F))
        value >>= 7
    return bytes(reversed(encoded))


def meta(delta: int, kind: int, payload: bytes) -> bytes:
    return vlq(delta) + bytes((0xFF, kind)) + vlq(len(payload)) + payload


def note_message(delta: int, status: int, note: int, velocity: int) -> bytes:
    return vlq(delta) + bytes((status, note, velocity))


def render(channel: int = 1) -> tuple[bytes, list[dict]]:
    if not 1 <= channel <= 16:
        raise ValueError("MIDI channel must be 1..16")
    on = 0x90 | (channel - 1)
    off = 0x80 | (channel - 1)
    track = bytearray()
    track += meta(0, 0x03, b"XP60Studio Sound DNA stimulus v1")
    track += meta(0, 0x51, TEMPO_US.to_bytes(3, "big"))
    manifest = []

    def marker(label: str):
        nonlocal track
        track += meta(0, 0x06, label.encode("ascii"))

    def single(label: str, note: int, velocity: int, duration: int = PPQ, release: int = PPQ):
        nonlocal track
        marker(label)
        track += note_message(0, on, note, velocity)
        track += note_message(duration, off, note, 0)
        # Delay is attached to the next marker, preserving an audible release.
        track += meta(release, 0x01, b"")
        manifest.append({"id": label, "notes": [note], "velocity": velocity,
                         "duration_ticks": duration, "release_ticks": release})

    for note, register in ((36, "low"), (60, "mid"), (84, "high")):
        for velocity in (32, 80, 120):
            single(f"{register}-v{velocity}", note, velocity)

    def chord(label: str, notes: tuple[int, ...], velocity: int, duration: int, release: int):
        nonlocal track
        marker(label)
        for index, note in enumerate(notes):
            track += note_message(0, on, note, velocity)
        for index, note in enumerate(notes):
            track += note_message(duration if index == 0 else 0, off, note, 0)
        track += meta(release, 0x01, b"")
        manifest.append({"id": label, "notes": list(notes), "velocity": velocity,
                         "duration_ticks": duration, "release_ticks": release})

    chord("fifth", (60, 67), 96, 2 * PPQ, PPQ)
    chord("open-chord", (48, 55, 60, 64), 96, 2 * PPQ, 2 * PPQ)
    single("sustain-tail", 60, 96, 4 * PPQ, 4 * PPQ)
    track += meta(0, 0x2F, b"")

    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ)
    body = b"MTrk" + struct.pack(">I", len(track)) + bytes(track)
    return header + body, manifest


def write_stimulus(output: Path, manifest_path: Path | None, channel: int = 1):
    midi, events = render(channel)
    output.write_bytes(midi)
    manifest = {
        "format": "xp60studio.sounddna-stimulus/1",
        "midi_sha256": hashlib.sha256(midi).hexdigest(),
        "midi_channel": channel,
        "ticks_per_quarter": PPQ,
        "tempo_us_per_quarter": TEMPO_US,
        "contains_bank_or_program_selection": False,
        "events": events,
    }
    if manifest_path:
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def self_test():
    first, events = render()
    second, _ = render()
    assert first == second and first[:4] == b"MThd" and b"MTrk" in first
    # Status bytes for Bank Select CC and Program Change must be absent. Notes
    # use only 0x8n/0x9n and meta events use 0xFF.
    assert first.count(bytes((0xB0,))) == 0 and first.count(bytes((0xC0,))) == 0
    assert len(events) == 12
    with tempfile.TemporaryDirectory() as directory:
        target = Path(directory) / "stimulus.mid"
        card = write_stimulus(target, Path(directory) / "manifest.json")
        assert card["midi_sha256"] == hashlib.sha256(target.read_bytes()).hexdigest()
    print("generate_sounddna_stimulus self-test passed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path, nargs="?")
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--channel", type=int, default=1)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.output:
        parser.error("output is required unless --self-test is used")
    card = write_stimulus(args.output, args.manifest, args.channel)
    print(f"wrote {args.output} ({card['midi_sha256']})")


if __name__ == "__main__":
    main()
