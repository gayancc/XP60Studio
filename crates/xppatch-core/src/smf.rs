//! Standard MIDI Files (`.mid`) that carry SysEx. A librarian-friendly container for the same messages a `.syx` holds:
//! one track, one SysEx event per message, spaced so that playing the file back through a sequencer does not outrun the
//! instrument (parity row D7). Only SysEx events are read; notes, controllers and meta events are skipped.

use thiserror::Error;
use xppatch_sysex::stream::frames;

use crate::syx::MAX_FILE_BYTES;

/// Ticks per quarter note written to the header.
const DIVISION: u16 = 480;
/// Gap between messages, in ticks: 24 ticks is 25 ms at the default 120 BPM, above the instrument's 20 ms minimum.
const GAP_TICKS: u32 = 24;
const SYSEX_START: u8 = 0xF0;
const SYSEX_END: u8 = 0xF7;

#[derive(Debug, Error, PartialEq, Eq)]
#[non_exhaustive]
pub enum SmfError {
    #[error("no SysEx messages to write")]
    NothingToWrite,
    #[error("not a Standard MIDI File (missing MThd header)")]
    NotMidi,
    #[error("the MIDI file ends in the middle of {0}")]
    Truncated(&'static str),
    #[error("the MIDI file contains no SysEx messages")]
    NoSysex,
    #[error("a SysEx event in the MIDI file never ends with F7")]
    UnterminatedSysex,
    #[error("file is {size} bytes, above the {limit} byte limit")]
    TooLarge { size: usize, limit: usize },
}

/// Append a MIDI variable-length quantity (7 bits per byte, high bit set on all but the last).
fn push_varlen(out: &mut Vec<u8>, mut value: u32) {
    let mut stack = [0u8; 5];
    let mut n = 0;
    loop {
        stack[n] = (value & 0x7F) as u8;
        n += 1;
        value >>= 7;
        if value == 0 {
            break;
        }
    }
    for i in (0..n).rev() {
        out.push(stack[i] | if i == 0 { 0 } else { 0x80 });
    }
}

/// Wrap the SysEx messages of a `.syx` file in a type 0 Standard MIDI File.
pub fn syx_to_smf(syx: &[u8]) -> Result<Vec<u8>, SmfError> {
    let mut track = Vec::new();
    let mut count = 0usize;
    for frame in frames(syx) {
        // Only complete frames are carried; a truncated one would play back as a broken message.
        if frame.len() < 3 || frame.last() != Some(&SYSEX_END) {
            continue;
        }
        push_varlen(&mut track, if count == 0 { 0 } else { GAP_TICKS });
        track.push(SYSEX_START);
        // The event length counts the bytes after F0, up to and including F7.
        push_varlen(&mut track, (frame.len() - 1) as u32);
        track.extend_from_slice(&frame[1..]);
        count += 1;
    }
    if count == 0 {
        return Err(SmfError::NothingToWrite);
    }
    track.extend_from_slice(&[0x00, 0xFF, 0x2F, 0x00]); // end of track

    let mut out = Vec::with_capacity(22 + track.len());
    out.extend_from_slice(b"MThd");
    out.extend_from_slice(&6u32.to_be_bytes());
    out.extend_from_slice(&0u16.to_be_bytes()); // format 0
    out.extend_from_slice(&1u16.to_be_bytes()); // one track
    out.extend_from_slice(&DIVISION.to_be_bytes());
    out.extend_from_slice(b"MTrk");
    out.extend_from_slice(&(track.len() as u32).to_be_bytes());
    out.extend_from_slice(&track);
    Ok(out)
}

struct Cursor<'a> {
    bytes: &'a [u8],
    pos: usize,
}

impl<'a> Cursor<'a> {
    fn u8(&mut self, what: &'static str) -> Result<u8, SmfError> {
        let b = *self.bytes.get(self.pos).ok_or(SmfError::Truncated(what))?;
        self.pos += 1;
        Ok(b)
    }

    fn take(&mut self, n: usize, what: &'static str) -> Result<&'a [u8], SmfError> {
        let end = self.pos.checked_add(n).ok_or(SmfError::Truncated(what))?;
        let s = self
            .bytes
            .get(self.pos..end)
            .ok_or(SmfError::Truncated(what))?;
        self.pos = end;
        Ok(s)
    }

    fn varlen(&mut self, what: &'static str) -> Result<u32, SmfError> {
        let mut value: u32 = 0;
        for _ in 0..4 {
            let b = self.u8(what)?;
            value = (value << 7) | u32::from(b & 0x7F);
            if b & 0x80 == 0 {
                return Ok(value);
            }
        }
        Err(SmfError::Truncated(what))
    }

    fn u32(&mut self, what: &'static str) -> Result<u32, SmfError> {
        let s = self.take(4, what)?;
        Ok(u32::from_be_bytes([s[0], s[1], s[2], s[3]]))
    }
}

/// Collect every SysEx message of a Standard MIDI File (any format, any number of tracks) as `.syx` bytes, each message
/// complete with its F0 and F7. Escape (`F7`) continuations of a split message are joined.
pub fn smf_to_syx(smf: &[u8]) -> Result<Vec<u8>, SmfError> {
    if smf.len() > MAX_FILE_BYTES {
        return Err(SmfError::TooLarge {
            size: smf.len(),
            limit: MAX_FILE_BYTES,
        });
    }
    let mut c = Cursor { bytes: smf, pos: 0 };
    if c.take(4, "the header").ok() != Some(b"MThd") {
        return Err(SmfError::NotMidi);
    }
    let header_len = c.u32("the header")? as usize;
    c.take(header_len.max(6), "the header")?; // format, tracks, division: not needed to find the SysEx events

    let mut out = Vec::new();
    while c.pos < smf.len() {
        let id = c.take(4, "a chunk header")?;
        let len = c.u32("a chunk header")? as usize;
        let body = c.take(len, "a track")?;
        if id != b"MTrk" {
            continue; // unknown chunk types are to be skipped
        }
        read_track(body, &mut out)?;
    }
    if out.is_empty() {
        return Err(SmfError::NoSysex);
    }
    Ok(out)
}

fn read_track(body: &[u8], out: &mut Vec<u8>) -> Result<(), SmfError> {
    let mut c = Cursor {
        bytes: body,
        pos: 0,
    };
    let mut running: u8 = 0;
    // A message that was split over several events and is still waiting for its F7.
    let mut pending: Option<Vec<u8>> = None;
    while c.pos < body.len() {
        c.varlen("an event time")?;
        let first = c.u8("an event")?;
        let status = if first & 0x80 != 0 { first } else { running };
        match status {
            0xFF => {
                c.u8("a meta event")?; // type
                let n = c.varlen("a meta event")? as usize;
                c.take(n, "a meta event")?;
            }
            SYSEX_START | SYSEX_END => {
                let n = c.varlen("a SysEx event")? as usize;
                let data = c.take(n, "a SysEx event")?;
                let message = match (status, pending.take()) {
                    (SYSEX_START, None) => {
                        let mut m = vec![SYSEX_START];
                        m.extend_from_slice(data);
                        m
                    }
                    (SYSEX_START, Some(_)) => return Err(SmfError::UnterminatedSysex),
                    (_, Some(mut m)) => {
                        m.extend_from_slice(data);
                        m
                    }
                    // A lone F7 event is an "escape" carrying arbitrary bytes, not part of a message: skip it.
                    (_, None) => continue,
                };
                if message.last() == Some(&SYSEX_END) {
                    out.extend_from_slice(&message);
                } else {
                    pending = Some(message);
                }
            }
            0x80..=0xEF => {
                running = status;
                // The first data byte was already read when it was not a status byte.
                let data_bytes = if matches!(status & 0xF0, 0xC0 | 0xD0) {
                    1
                } else {
                    2
                };
                let already = usize::from(first & 0x80 == 0);
                c.take(data_bytes - already, "a channel event")?;
            }
            _ => return Err(SmfError::Truncated("an unknown event")),
        }
    }
    if pending.is_some() {
        return Err(SmfError::UnterminatedSysex);
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::syx::SyxImage;

    const BANK: &[u8] = include_bytes!("../../../tests/golden/xp60-user-bank.syx");

    #[test]
    fn varlen_matches_the_midi_examples() {
        for (value, bytes) in [
            (0u32, vec![0x00]),
            (0x7F, vec![0x7F]),
            (0x80, vec![0x81, 0x00]),
            (0x3FFF, vec![0xFF, 0x7F]),
            (0x4000, vec![0x81, 0x80, 0x00]),
        ] {
            let mut v = Vec::new();
            push_varlen(&mut v, value);
            assert_eq!(v, bytes, "value {value:#x}");
            let mut c = Cursor {
                bytes: &bytes,
                pos: 0,
            };
            assert_eq!(c.varlen("x").unwrap(), value);
        }
    }

    #[test]
    fn writes_a_valid_header_and_end_of_track() {
        let smf = syx_to_smf(&[0xF0, 0x41, 0x10, 0x6A, 0x12, 0x01, 0xF7]).unwrap();
        assert_eq!(&smf[0..4], b"MThd");
        assert_eq!(&smf[4..8], &6u32.to_be_bytes());
        assert_eq!(&smf[8..10], &0u16.to_be_bytes());
        assert_eq!(&smf[10..12], &1u16.to_be_bytes());
        assert_eq!(&smf[12..14], &DIVISION.to_be_bytes());
        assert_eq!(&smf[14..18], b"MTrk");
        let len = u32::from_be_bytes([smf[18], smf[19], smf[20], smf[21]]) as usize;
        assert_eq!(smf.len(), 22 + len);
        assert_eq!(&smf[smf.len() - 4..], &[0x00, 0xFF, 0x2F, 0x00]);
    }

    #[test]
    fn round_trips_the_golden_bank_byte_for_byte() {
        let smf = syx_to_smf(BANK).unwrap();
        let back = smf_to_syx(&smf).unwrap();
        assert_eq!(back, BANK);
        // And it is still the same 128 patches.
        assert_eq!(
            SyxImage::parse(&back).unwrap(),
            SyxImage::parse(BANK).unwrap()
        );
    }

    #[test]
    fn spaces_messages_apart() {
        let two = [
            0xF0, 0x41, 0x10, 0x6A, 0x12, 0xF7, 0xF0, 0x41, 0x10, 0x6A, 0x12, 0x02, 0xF7,
        ];
        let smf = syx_to_smf(&two).unwrap();
        // First event at time 0, second after GAP_TICKS (24 = 0x18).
        let track = &smf[22..];
        assert_eq!(track[0], 0x00);
        let second = 2 + 1 + 5; // time + F0 + length + 5 data bytes (41 10 6A 12 F7)
        assert_eq!(track[second], GAP_TICKS as u8);
    }

    #[test]
    fn skips_incomplete_frames_and_refuses_an_empty_input() {
        assert_eq!(syx_to_smf(&[]), Err(SmfError::NothingToWrite));
        assert_eq!(
            syx_to_smf(&[0xF0, 0x41, 0x10]),
            Err(SmfError::NothingToWrite)
        );
        let mixed = [0xF0, 0x41, 0x10, 0xF0, 0x41, 0x10, 0x6A, 0xF7];
        let back = smf_to_syx(&syx_to_smf(&mixed).unwrap()).unwrap();
        assert_eq!(back, vec![0xF0, 0x41, 0x10, 0x6A, 0xF7]);
    }

    /// A hand-built file with a note, running status, a meta event and a SysEx split over an F0 and an F7 event.
    #[test]
    fn reads_foreign_files_skipping_everything_but_sysex() {
        let mut track = vec![
            0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20, // tempo
            0x00, 0x90, 0x3C, 0x40, // note on
            0x10, 0x3E, 0x40, // running status note on
            0x00, 0xC0, 0x05, // program change (one data byte)
            0x00, 0xF0, 0x04, 0x41, 0x10, 0x6A, 0x12, // SysEx, first part, no F7
            0x05, 0xF7, 0x02, 0x33, 0xF7, // continuation ending the message
            0x00, 0xF7, 0x02, 0x01, 0x02, // lone escape: ignored
            0x00, 0xFF, 0x2F, 0x00,
        ];
        let mut smf = Vec::new();
        smf.extend_from_slice(b"MThd");
        smf.extend_from_slice(&6u32.to_be_bytes());
        smf.extend_from_slice(&[0, 1, 0, 1, 0x01, 0xE0]);
        smf.extend_from_slice(b"MTrk");
        smf.extend_from_slice(&(track.len() as u32).to_be_bytes());
        smf.append(&mut track);
        assert_eq!(
            smf_to_syx(&smf).unwrap(),
            vec![0xF0, 0x41, 0x10, 0x6A, 0x12, 0x33, 0xF7]
        );
    }

    #[test]
    fn rejects_bad_input_with_a_clear_error() {
        assert_eq!(smf_to_syx(b"RIFFxxxx"), Err(SmfError::NotMidi));
        assert_eq!(smf_to_syx(&[]), Err(SmfError::NotMidi));
        let smf = syx_to_smf(&[0xF0, 0x41, 0x10, 0x6A, 0x12, 0x01, 0xF7]).unwrap();
        assert!(matches!(
            smf_to_syx(&smf[..smf.len() - 6]),
            Err(SmfError::Truncated(_))
        ));
        // A file with only notes has nothing to import.
        let mut notes = Vec::new();
        notes.extend_from_slice(b"MThd");
        notes.extend_from_slice(&6u32.to_be_bytes());
        notes.extend_from_slice(&[0, 0, 0, 1, 0x01, 0xE0]);
        notes.extend_from_slice(b"MTrk");
        notes.extend_from_slice(&8u32.to_be_bytes());
        notes.extend_from_slice(&[0x00, 0x90, 0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00]);
        assert_eq!(smf_to_syx(&notes), Err(SmfError::NoSysex));
    }

    #[test]
    fn never_panics_on_garbage() {
        let mut seed = 0x1234_5678u32;
        for len in [0usize, 1, 5, 14, 22, 64, 300] {
            let mut data: Vec<u8> = (0..len)
                .map(|_| {
                    seed = seed.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
                    (seed >> 24) as u8
                })
                .collect();
            if len >= 4 {
                data[..4].copy_from_slice(b"MThd");
            }
            let _ = smf_to_syx(&data);
        }
    }
}
