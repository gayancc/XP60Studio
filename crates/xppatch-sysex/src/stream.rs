//! Splitting a byte stream (a `.syx` file or a MIDI input buffer) into SysEx frames and parsing them.

use crate::message::{Message, ParseError, Parsed, SYSEX_END, SYSEX_START};
use std::borrow::Cow;

/// MIDI real-time messages (`F8..=FF`) may legally appear *inside* a SysEx frame (for example Active Sensing `FE`).
const REALTIME_MIN: u8 = 0xF8;

/// Lazily splits a buffer into candidate SysEx frames. Bytes outside `F0 .. F7` are ignored. Real-time bytes inside a
/// frame are removed. A frame that is interrupted by another `F0` (or runs to the end of the buffer) is returned
/// without its terminating `F7`, so parsing reports it as truncated rather than silently dropping it.
pub struct Frames<'a> {
    bytes: &'a [u8],
    pos: usize,
}

/// Iterate the frames in `bytes`, one at a time (nothing is collected, so callers can stop at the first error).
pub fn frames(bytes: &[u8]) -> Frames<'_> {
    Frames { bytes, pos: 0 }
}

impl<'a> Iterator for Frames<'a> {
    type Item = Cow<'a, [u8]>;

    fn next(&mut self) -> Option<Self::Item> {
        let bytes = self.bytes;
        while self.pos < bytes.len() && bytes[self.pos] != SYSEX_START {
            self.pos += 1;
        }
        if self.pos >= bytes.len() {
            return None;
        }
        let start = self.pos;
        self.pos += 1;
        let mut has_realtime = false;
        while self.pos < bytes.len()
            && bytes[self.pos] != SYSEX_END
            && bytes[self.pos] != SYSEX_START
        {
            has_realtime |= bytes[self.pos] >= REALTIME_MIN;
            self.pos += 1;
        }
        if self.pos < bytes.len() && bytes[self.pos] == SYSEX_END {
            self.pos += 1;
        }
        let raw = &bytes[start..self.pos];
        Some(if has_realtime {
            Cow::Owned(raw.iter().copied().filter(|&b| b < REALTIME_MIN).collect())
        } else {
            Cow::Borrowed(raw)
        })
    }
}

/// Parse every frame in `bytes`, keeping each result so callers can report exactly which message failed. Intended for
/// bounded buffers (a MIDI input read); for files use [`frames`] and stop at the first error.
pub fn parse_stream(bytes: &[u8]) -> Vec<Result<Parsed, ParseError>> {
    frames(bytes).map(|f| Message::parse(&f)).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    const DT1: [u8; 12] = [
        0xF0, 0x41, 0x10, 0x6A, 0x12, 0x01, 0x00, 0x00, 0x28, 0x06, 0x51, 0xF7,
    ];

    #[test]
    fn splits_back_to_back_frames_and_skips_noise() {
        let mut data = vec![0x90, 0x3C, 0x40];
        data.extend_from_slice(&DT1);
        data.extend_from_slice(&[0xFE]);
        data.extend_from_slice(&DT1);
        let f: Vec<_> = frames(&data).collect();
        assert_eq!(f.len(), 2);
        assert!(f.iter().all(|x| x.as_ref() == DT1));
        assert!(parse_stream(&data).iter().all(Result::is_ok));
    }

    #[test]
    fn real_time_bytes_inside_a_frame_are_removed() {
        let mut data = DT1.to_vec();
        data.insert(6, 0xFE); // Active Sensing in the middle of the address
        data.insert(3, 0xF8); // timing clock earlier
        let f: Vec<_> = frames(&data).collect();
        assert_eq!(f.len(), 1);
        assert_eq!(
            f[0].as_ref(),
            DT1,
            "the frame is reconstructed without the real-time bytes"
        );
        assert!(Message::parse(&f[0]).is_ok());
        // Frames with no real-time bytes are borrowed, not copied.
        assert!(matches!(frames(&DT1).next(), Some(Cow::Borrowed(_))));
    }

    #[test]
    fn an_interrupted_frame_is_reported_not_dropped() {
        let mut data = DT1[..8].to_vec();
        data.extend_from_slice(&DT1);
        let results = parse_stream(&data);
        assert_eq!(results.len(), 2);
        assert!(matches!(
            results[0],
            Err(ParseError::TooShort { .. }) | Err(ParseError::MissingEnd)
        ));
        assert!(results[1].is_ok());
    }

    #[test]
    fn a_frame_missing_its_terminator_at_the_end_is_reported() {
        let results = parse_stream(&DT1[..11]);
        assert_eq!(results.len(), 1);
        assert_eq!(results[0], Err(ParseError::MissingEnd));
    }

    #[test]
    fn empty_input_has_no_frames() {
        assert!(frames(&[]).next().is_none());
        assert!(frames(&[0x90, 0x3C]).next().is_none());
    }

    #[test]
    fn iteration_is_lazy_so_a_caller_can_stop_early() {
        let hostile = vec![0xF0u8; 1_000_000];
        // Taking just the first frame must not walk (or allocate for) the whole buffer.
        assert_eq!(frames(&hostile).next().map(|f| f.len()), Some(1));
    }
}
