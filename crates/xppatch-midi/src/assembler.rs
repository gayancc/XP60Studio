//! Turning incoming MIDI chunks into SysEx frames and the few channel events the application cares about.
//!
//! Backends may deliver a long SysEx in fragments, may interleave real-time bytes (Active Sensing `FE`, clock `F8`) and
//! will deliver unrelated channel messages (notes, controllers). The assembler yields complete SysEx frames and the
//! two kinds of channel event that tell us the instrument's temporary patch was replaced from the panel: Program
//! Change and Bank Select, plus every other controller (reported as `Controller`, for mapping a hardware controller to
//! parameters). Everything else is dropped. Memory is bounded.

use xppatch_sysex::message::{MAX_FRAME_LEN, SYSEX_END, SYSEX_START};

/// A channel event relevant to staleness of the temporary area.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MidiEvent {
    ProgramChange {
        channel: u8,
        program: u8,
    },
    /// Controller 0 (MSB) or 32 (LSB).
    BankSelect {
        channel: u8,
        controller: u8,
        value: u8,
    },
    /// Any other controller (not Bank Select): a mod wheel, a slider, a pedal.
    Controller {
        channel: u8,
        controller: u8,
        value: u8,
    },
}

/// One item produced by [`SysExAssembler::feed`].
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Incoming {
    /// A complete `F0 .. F7` frame (real-time bytes removed). It has not been validated; hand it to `Message::parse`.
    SysEx(Vec<u8>),
    /// A SysEx that was cut short (interrupted by another status byte) or grew past the frame limit.
    Truncated,
    Event(MidiEvent),
}

/// Stateful assembler; feed it every chunk in order.
#[derive(Debug, Default)]
pub struct SysExAssembler {
    sysex: Option<Vec<u8>>,
    /// Channel message being collected: (status, data bytes so far).
    channel: Option<(u8, Vec<u8>)>,
}

impl SysExAssembler {
    pub fn new() -> Self {
        Self::default()
    }

    /// Forget any partial message (call before a new request so stale fragments cannot be mistaken for a reply).
    pub fn reset(&mut self) {
        self.sysex = None;
        self.channel = None;
    }

    pub fn feed(&mut self, bytes: &[u8]) -> Vec<Incoming> {
        let mut out = Vec::new();
        for &b in bytes {
            self.byte(b, &mut out);
        }
        out
    }

    fn byte(&mut self, b: u8, out: &mut Vec<Incoming>) {
        // Real-time messages may appear anywhere, even inside a SysEx.
        if b >= 0xF8 {
            return;
        }
        if b == SYSEX_START {
            if self.sysex.take().is_some() {
                out.push(Incoming::Truncated);
            }
            self.channel = None;
            self.sysex = Some(vec![SYSEX_START]);
            return;
        }
        if let Some(buf) = self.sysex.as_mut() {
            if b == SYSEX_END {
                buf.push(b);
                if let Some(frame) = self.sysex.take() {
                    out.push(Incoming::SysEx(frame));
                }
            } else if b >= 0x80 {
                // Another status byte ends the SysEx without an F7.
                self.sysex = None;
                out.push(Incoming::Truncated);
                self.status(b);
            } else if buf.len() >= MAX_FRAME_LEN {
                // Runaway SysEx: drop it and wait for the next F0.
                self.sysex = None;
                out.push(Incoming::Truncated);
            } else {
                buf.push(b);
            }
            return;
        }
        if b >= 0x80 {
            self.status(b);
        } else {
            self.data(b, out);
        }
    }

    fn status(&mut self, b: u8) {
        // Only channel messages are tracked; system common (F1..F7) cancels any running status.
        self.channel = (0x80..0xF0)
            .contains(&b)
            .then(|| (b, Vec::with_capacity(2)));
    }

    fn data(&mut self, b: u8, out: &mut Vec<Incoming>) {
        let Some((status, data)) = self.channel.as_mut() else {
            return;
        };
        let need = match *status & 0xF0 {
            0xC0 | 0xD0 => 1,
            _ => 2,
        };
        data.push(b);
        if data.len() < need {
            return;
        }
        let channel = *status & 0x0F;
        let event = match (*status & 0xF0, data.as_slice()) {
            (0xC0, [program]) => Some(MidiEvent::ProgramChange {
                channel,
                program: *program,
            }),
            (0xB0, [controller @ (0 | 32), value]) => Some(MidiEvent::BankSelect {
                channel,
                controller: *controller,
                value: *value,
            }),
            (0xB0, [controller, value]) => Some(MidiEvent::Controller {
                channel,
                controller: *controller,
                value: *value,
            }),
            _ => None,
        };
        data.clear(); // keep the status for running status
        if let Some(e) = event {
            out.push(Incoming::Event(e));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const DT1: [u8; 12] = [
        0xF0, 0x41, 0x10, 0x6A, 0x12, 0x01, 0x00, 0x00, 0x28, 0x06, 0x51, 0xF7,
    ];

    fn frames(items: &[Incoming]) -> Vec<&Vec<u8>> {
        items
            .iter()
            .filter_map(|i| match i {
                Incoming::SysEx(f) => Some(f),
                _ => None,
            })
            .collect()
    }

    #[test]
    fn a_whole_frame_in_one_chunk() {
        let mut a = SysExAssembler::new();
        let items = a.feed(&DT1);
        assert_eq!(frames(&items), vec![&DT1.to_vec()]);
    }

    #[test]
    fn a_frame_split_across_chunks_is_reassembled() {
        let mut a = SysExAssembler::new();
        assert!(a.feed(&DT1[..5]).is_empty());
        assert!(a.feed(&DT1[5..9]).is_empty());
        let items = a.feed(&DT1[9..]);
        assert_eq!(frames(&items), vec![&DT1.to_vec()]);
    }

    #[test]
    fn real_time_bytes_inside_a_frame_are_dropped() {
        let mut a = SysExAssembler::new();
        let mut data = DT1.to_vec();
        data.insert(7, 0xFE);
        data.insert(2, 0xF8);
        let items = a.feed(&data);
        assert_eq!(frames(&items), vec![&DT1.to_vec()]);
    }

    #[test]
    fn an_interrupted_sysex_is_reported_and_the_next_frame_still_parses() {
        let mut a = SysExAssembler::new();
        let mut data = DT1[..6].to_vec();
        data.extend_from_slice(&DT1);
        let items = a.feed(&data);
        assert_eq!(items[0], Incoming::Truncated);
        assert_eq!(frames(&items), vec![&DT1.to_vec()]);

        // A channel status byte also ends an unterminated SysEx.
        let items = a.feed(&[0xF0, 0x41, 0xC0, 0x05]);
        assert_eq!(items[0], Incoming::Truncated);
        assert_eq!(
            items[1],
            Incoming::Event(MidiEvent::ProgramChange {
                channel: 0,
                program: 5
            })
        );
    }

    #[test]
    fn program_bank_and_controller_messages_are_events_notes_are_not() {
        let mut a = SysExAssembler::new();
        let items = a.feed(&[
            0x90, 0x3C, 0x40, // note on: ignored
            0xB2, 0x00, 0x7F, // bank select MSB on channel 3
            0xB2, 0x20, 0x01, // bank select LSB
            0xB2, 0x07, 0x64, // volume: a controller event
            0xC2, 0x0A, // program change
        ]);
        assert_eq!(
            items,
            vec![
                Incoming::Event(MidiEvent::BankSelect {
                    channel: 2,
                    controller: 0,
                    value: 0x7F
                }),
                Incoming::Event(MidiEvent::BankSelect {
                    channel: 2,
                    controller: 32,
                    value: 1
                }),
                Incoming::Event(MidiEvent::Controller {
                    channel: 2,
                    controller: 7,
                    value: 0x64
                }),
                Incoming::Event(MidiEvent::ProgramChange {
                    channel: 2,
                    program: 10
                }),
            ]
        );
    }

    #[test]
    fn running_status_is_honoured() {
        let mut a = SysExAssembler::new();
        let items = a.feed(&[0xC0, 0x01, 0x02, 0x03]);
        assert_eq!(items.len(), 3);
    }

    #[test]
    fn a_runaway_sysex_is_bounded() {
        let mut a = SysExAssembler::new();
        let mut data = vec![0xF0];
        data.extend(std::iter::repeat_n(0x01u8, MAX_FRAME_LEN * 4));
        let items = a.feed(&data);
        assert!(items.contains(&Incoming::Truncated));
        assert!(frames(&items).is_empty());
        // And the assembler recovers for the next real frame.
        assert_eq!(frames(&a.feed(&DT1)).len(), 1);
    }

    #[test]
    fn reset_discards_a_partial_frame() {
        let mut a = SysExAssembler::new();
        a.feed(&DT1[..6]);
        a.reset();
        assert!(a.feed(&DT1[6..]).is_empty());
    }

    #[test]
    fn other_controllers_are_reported_and_bank_select_stays_bank_select() {
        let mut a = SysExAssembler::new();
        let items = a.feed(&[0xB2, 0x01, 0x40, 0xB2, 0x00, 0x05]);
        assert_eq!(
            items,
            vec![
                Incoming::Event(MidiEvent::Controller {
                    channel: 2,
                    controller: 1,
                    value: 0x40
                }),
                Incoming::Event(MidiEvent::BankSelect {
                    channel: 2,
                    controller: 0,
                    value: 5
                }),
            ]
        );
    }

    #[test]
    fn controllers_follow_running_status_and_split_chunks() {
        let mut a = SysExAssembler::new();
        let mut items = a.feed(&[0xB0, 0x07, 0x10, 0x07, 0x20]);
        items.extend(a.feed(&[0x0B]));
        items.extend(a.feed(&[0x30]));
        let values: Vec<(u8, u8)> = items
            .iter()
            .filter_map(|i| match i {
                Incoming::Event(MidiEvent::Controller {
                    controller, value, ..
                }) => Some((*controller, *value)),
                _ => None,
            })
            .collect();
        assert_eq!(values, vec![(7, 0x10), (7, 0x20), (11, 0x30)]);
    }
}
