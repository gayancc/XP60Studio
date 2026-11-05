//! Turning controller messages into what a hardware controller means: a plain Control Change, or an NRPN or RPN value
//! (a controller number selected by CC 99/98 or CC 101/100 and a value sent with Data Entry, CC 6 and 38). Interfaces and
//! some controllers send everything as NRPN, so a controller map that only knew plain CCs would hear nothing from them.

/// Where a controller value came from.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ControlSource {
    /// Control Change number 0..127.
    Cc(u8),
    /// Non-Registered Parameter Number, 14 bits.
    Nrpn(u16),
    /// Registered Parameter Number, 14 bits.
    Rpn(u16),
}

impl ControlSource {
    /// The largest value this kind of source can carry: 127 for a CC, 16383 for a parameter number.
    pub fn max_value(self) -> u16 {
        match self {
            ControlSource::Cc(_) => 127,
            ControlSource::Nrpn(_) | ControlSource::Rpn(_) => 16383,
        }
    }
}

/// A controller that moved, with its latest value.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ControlChange {
    pub channel: u8,
    pub source: ControlSource,
    pub value: u16,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Selected {
    None,
    Nrpn,
    Rpn,
}

/// What one channel has said so far about the parameter number it is addressing.
#[derive(Debug, Clone, Copy)]
struct Channel {
    selected: Selected,
    nrpn_msb: Option<u8>,
    nrpn_lsb: Option<u8>,
    rpn_msb: Option<u8>,
    rpn_lsb: Option<u8>,
    data_msb: Option<u8>,
    data_lsb: Option<u8>,
}

impl Channel {
    const NEW: Channel = Channel {
        selected: Selected::None,
        nrpn_msb: None,
        nrpn_lsb: None,
        rpn_msb: None,
        rpn_lsb: None,
        data_msb: None,
        data_lsb: None,
    };

    fn number(&self) -> Option<ControlSource> {
        let (msb, lsb, rpn) = match self.selected {
            Selected::Nrpn => (self.nrpn_msb?, self.nrpn_lsb?, false),
            Selected::Rpn => (self.rpn_msb?, self.rpn_lsb?, true),
            Selected::None => return None,
        };
        // RPN 127,127 is the "null" parameter: it ends addressing and selects nothing.
        if rpn && msb == 127 && lsb == 127 {
            return None;
        }
        let n = (u16::from(msb) << 7) | u16::from(lsb);
        Some(if rpn {
            ControlSource::Rpn(n)
        } else {
            ControlSource::Nrpn(n)
        })
    }
}

/// Feed it every Control Change; it returns the change to report, if any.
#[derive(Debug, Clone)]
pub struct ControlDecoder {
    channels: [Channel; 16],
}

impl Default for ControlDecoder {
    fn default() -> Self {
        ControlDecoder {
            channels: [Channel::NEW; 16],
        }
    }
}

impl ControlDecoder {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn reset(&mut self) {
        *self = Self::default();
    }

    pub fn feed(&mut self, channel: u8, controller: u8, value: u8) -> Option<ControlChange> {
        let ch = &mut self.channels[usize::from(channel & 0x0F)];
        match controller {
            // Selecting a parameter number starts a fresh data entry; nothing is reported for the selection itself.
            99 | 98 => {
                if ch.selected != Selected::Nrpn {
                    ch.selected = Selected::Nrpn;
                    ch.nrpn_msb = None;
                    ch.nrpn_lsb = None;
                }
                if controller == 99 {
                    ch.nrpn_msb = Some(value);
                    ch.nrpn_lsb = None;
                } else {
                    ch.nrpn_lsb = Some(value);
                }
                ch.data_msb = None;
                ch.data_lsb = None;
                None
            }
            101 | 100 => {
                if ch.selected != Selected::Rpn {
                    ch.selected = Selected::Rpn;
                    ch.rpn_msb = None;
                    ch.rpn_lsb = None;
                }
                if controller == 101 {
                    ch.rpn_msb = Some(value);
                    ch.rpn_lsb = None;
                } else {
                    ch.rpn_lsb = Some(value);
                }
                ch.data_msb = None;
                ch.data_lsb = None;
                None
            }
            // Data Entry: the value of the selected parameter number. With none selected it is an ordinary controller.
            6 | 38 => {
                let Some(source) = ch.number() else {
                    return Some(ControlChange {
                        channel,
                        source: ControlSource::Cc(controller),
                        value: u16::from(value),
                    });
                };
                if controller == 6 {
                    ch.data_msb = Some(value);
                } else {
                    ch.data_lsb = Some(value);
                }
                // A value with only its LSB heard so far is not reported: the MSB defines it.
                let msb = ch.data_msb?;
                Some(ControlChange {
                    channel,
                    source,
                    value: (u16::from(msb) << 7) | u16::from(ch.data_lsb.unwrap_or(0)),
                })
            }
            _ => Some(ControlChange {
                channel,
                source: ControlSource::Cc(controller),
                value: u16::from(value),
            }),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn nrpn(
        d: &mut ControlDecoder,
        ch: u8,
        msb: u8,
        lsb: u8,
        data_msb: u8,
        data_lsb: Option<u8>,
    ) -> Vec<ControlChange> {
        let mut out = Vec::new();
        out.extend(d.feed(ch, 99, msb));
        out.extend(d.feed(ch, 98, lsb));
        out.extend(d.feed(ch, 6, data_msb));
        if let Some(l) = data_lsb {
            out.extend(d.feed(ch, 38, l));
        }
        out
    }

    #[test]
    fn plain_controllers_pass_through() {
        let mut d = ControlDecoder::new();
        assert_eq!(
            d.feed(3, 74, 90),
            Some(ControlChange {
                channel: 3,
                source: ControlSource::Cc(74),
                value: 90
            })
        );
    }

    #[test]
    fn an_nrpn_is_one_14_bit_value_from_its_data_entry() {
        let mut d = ControlDecoder::new();
        let got = nrpn(&mut d, 0, 1, 2, 0x40, Some(0x10));
        // The MSB alone reports once (LSB taken as 0); the LSB then refines it.
        assert_eq!(got.len(), 2);
        assert_eq!(got[0].source, ControlSource::Nrpn((1 << 7) | 2));
        assert_eq!(got[0].value, 0x40 << 7);
        assert_eq!(got[1].value, (0x40 << 7) | 0x10);
        assert_eq!(ControlSource::Nrpn(0).max_value(), 16383);
    }

    #[test]
    fn selecting_a_number_reports_nothing_and_starts_afresh() {
        let mut d = ControlDecoder::new();
        assert_eq!(d.feed(0, 99, 5), None);
        assert_eq!(d.feed(0, 98, 6), None);
        // A second number must not inherit the previous value's LSB.
        nrpn(&mut d, 0, 5, 6, 0x7F, Some(0x7F));
        let got = nrpn(&mut d, 0, 5, 7, 0x01, None);
        assert_eq!(got[0].source, ControlSource::Nrpn((5 << 7) | 7));
        assert_eq!(got[0].value, 1 << 7);
    }

    #[test]
    fn data_entry_without_a_selected_number_is_an_ordinary_controller() {
        let mut d = ControlDecoder::new();
        assert_eq!(
            d.feed(0, 6, 33),
            Some(ControlChange {
                channel: 0,
                source: ControlSource::Cc(6),
                value: 33
            })
        );
    }

    #[test]
    fn rpn_is_told_apart_and_null_ends_addressing() {
        let mut d = ControlDecoder::new();
        d.feed(1, 101, 0);
        d.feed(1, 100, 2);
        let c = d.feed(1, 6, 12).unwrap();
        assert_eq!(c.source, ControlSource::Rpn(2));
        assert_eq!(c.value, 12 << 7);
        d.feed(1, 101, 127);
        d.feed(1, 100, 127);
        // After the null RPN, Data Entry is an ordinary controller again.
        assert_eq!(d.feed(1, 6, 9).unwrap().source, ControlSource::Cc(6));
    }

    #[test]
    fn channels_are_independent() {
        let mut d = ControlDecoder::new();
        d.feed(0, 99, 1);
        d.feed(0, 98, 1);
        d.feed(5, 99, 2);
        d.feed(5, 98, 2);
        assert_eq!(
            d.feed(0, 6, 1).unwrap().source,
            ControlSource::Nrpn((1 << 7) | 1)
        );
        assert_eq!(
            d.feed(5, 6, 1).unwrap().source,
            ControlSource::Nrpn((2 << 7) | 2)
        );
    }

    #[test]
    fn an_lsb_before_any_msb_is_not_reported() {
        let mut d = ControlDecoder::new();
        d.feed(0, 99, 1);
        d.feed(0, 98, 1);
        assert_eq!(d.feed(0, 38, 5), None);
    }
}
