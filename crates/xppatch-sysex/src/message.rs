//! Roland RQ1 / DT1 messages for the XP-60.
//!
//! ```text
//! RQ1: F0 41 dd 6A 11  a0 a1 a2 a3  s0 s1 s2 s3  cc F7
//! DT1: F0 41 dd 6A 12  a0 a1 a2 a3  d0 .. dn     cc F7
//! ```
//!
//! Parsing is strict: every body byte must be 7-bit, the checksum must match, the model ID must be the XP-60's
//! single byte, the frame must be a sane length, and the addressed span must stay inside the 28-bit address space.
//! Anything else is a typed error instead of a guess.

use crate::address::{Address, AddressError, Size};
use crate::checksum::checksum;
use thiserror::Error;

pub const SYSEX_START: u8 = 0xF0;
pub const SYSEX_END: u8 = 0xF7;
pub const MANUFACTURER_ROLAND: u8 = 0x41;
/// XP-60 / XP-80 model ID (one byte; later Roland models use two).
pub const MODEL_ID: u8 = 0x6A;
pub const CMD_RQ1: u8 = 0x11;
pub const CMD_DT1: u8 = 0x12;

/// Shortest valid frame: a DT1 with one data byte.
const MIN_FRAME: usize = 12;
/// Longest frame accepted. The largest legitimate message is a 129-byte Patch Tone (140 bytes framed); this leaves
/// room while bounding what hostile input can make the rest of the program hold.
pub const MAX_FRAME_LEN: usize = 512;

/// SysEx device ID `0x10..=0x1F`, shown to users as 17 to 32.
#[derive(Clone, Copy, PartialEq, Eq, Debug, Hash)]
pub struct DeviceId(u8);

impl DeviceId {
    /// Factory default (`0x10`, displayed 17).
    pub const DEFAULT: DeviceId = DeviceId(0x10);

    pub fn new(byte: u8) -> Result<Self, ParseError> {
        if (0x10..=0x1F).contains(&byte) {
            Ok(DeviceId(byte))
        } else {
            Err(ParseError::InvalidDeviceId(byte))
        }
    }

    /// From the number shown in the synth's menu (17 to 32).
    pub fn from_display_number(number: u8) -> Result<Self, ParseError> {
        DeviceId::new(number.wrapping_sub(1))
    }

    pub const fn byte(self) -> u8 {
        self.0
    }

    pub const fn display_number(self) -> u8 {
        self.0 + 1
    }
}

impl Default for DeviceId {
    fn default() -> Self {
        DeviceId::DEFAULT
    }
}

/// A decoded Roland message.
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum Message {
    /// Data request: read `size` address units starting at `address`.
    Rq1 { address: Address, size: Size },
    /// Data set: write `data` at `address`. Data bytes are 7-bit.
    Dt1 { address: Address, data: Vec<u8> },
}

/// A parsed frame with the device ID it was addressed to (or sent from).
#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Parsed {
    pub device: DeviceId,
    pub message: Message,
}

#[derive(Debug, Error, PartialEq, Eq, Clone)]
#[non_exhaustive]
pub enum EncodeError {
    #[error("DT1 data must not be empty")]
    EmptyData,
    #[error("DT1 data byte {byte:#04x} at index {index} exceeds 0x7F")]
    DataByteOutOfRange { index: usize, byte: u8 },
    #[error("RQ1 size must be at least 1")]
    ZeroSize,
    #[error("the addressed span runs past the 28-bit address space")]
    AddressOverflow,
}

#[derive(Debug, Error, PartialEq, Eq, Clone)]
#[non_exhaustive]
pub enum ParseError {
    #[error("frame is too short ({len} bytes)")]
    TooShort { len: usize },
    #[error("frame is too long ({len} bytes, limit {MAX_FRAME_LEN})")]
    TooLong { len: usize },
    #[error("frame does not start with F0")]
    MissingStart,
    #[error("frame does not end with F7")]
    MissingEnd,
    #[error("body byte {byte:#04x} at index {index} exceeds 0x7F")]
    ByteOutOfRange { index: usize, byte: u8 },
    #[error("manufacturer {0:#04x} is not Roland (0x41)")]
    NotRoland(u8),
    #[error("device ID {0:#04x} is outside 0x10..=0x1F")]
    InvalidDeviceId(u8),
    #[error("unsupported model ID {0:#04x} (expected 0x6A)")]
    UnsupportedModel(u8),
    #[error("unknown command {0:#04x}")]
    UnknownCommand(u8),
    #[error("bad checksum: expected {expected:#04x}, found {actual:#04x}")]
    BadChecksum { expected: u8, actual: u8 },
    #[error("RQ1 body must be 8 bytes, found {0}")]
    Rq1Length(usize),
    #[error("RQ1 size must be at least 1")]
    Rq1ZeroSize,
    #[error("DT1 carries no data bytes")]
    Dt1NoData,
    #[error("invalid address or size: {0}")]
    Address(#[from] AddressError),
}

impl Message {
    /// Serialise to a complete frame (`F0 ... F7`).
    pub fn encode(&self, device: DeviceId) -> Result<Vec<u8>, EncodeError> {
        let (command, body) = match self {
            Message::Rq1 { address, size } => {
                let Some(span) = size.compact().checked_sub(1) else {
                    return Err(EncodeError::ZeroSize);
                };
                address
                    .checked_add_int(span)
                    .map_err(|_| EncodeError::AddressOverflow)?;
                let mut body = Vec::with_capacity(8);
                body.extend_from_slice(&address.bytes());
                body.extend_from_slice(&size.bytes());
                (CMD_RQ1, body)
            }
            Message::Dt1 { address, data } => {
                if data.is_empty() {
                    return Err(EncodeError::EmptyData);
                }
                if let Some((index, &byte)) = data.iter().enumerate().find(|(_, &b)| b > 0x7F) {
                    return Err(EncodeError::DataByteOutOfRange { index, byte });
                }
                let last =
                    u32::try_from(data.len() - 1).map_err(|_| EncodeError::AddressOverflow)?;
                address
                    .checked_add_int(last)
                    .map_err(|_| EncodeError::AddressOverflow)?;
                let mut body = Vec::with_capacity(4 + data.len());
                body.extend_from_slice(&address.bytes());
                body.extend_from_slice(data);
                (CMD_DT1, body)
            }
        };
        let mut frame = Vec::with_capacity(body.len() + 7);
        frame.extend_from_slice(&[
            SYSEX_START,
            MANUFACTURER_ROLAND,
            device.byte(),
            MODEL_ID,
            command,
        ]);
        let cks = checksum(&body);
        frame.extend_from_slice(&body);
        frame.push(cks);
        frame.push(SYSEX_END);
        Ok(frame)
    }

    /// Strictly parse one complete frame.
    pub fn parse(frame: &[u8]) -> Result<Parsed, ParseError> {
        // Framing first: a frame cut off mid-transfer should say "missing F7", not "too short".
        let (Some(&first), Some(&last)) = (frame.first(), frame.last()) else {
            return Err(ParseError::TooShort { len: 0 });
        };
        if first != SYSEX_START {
            return Err(ParseError::MissingStart);
        }
        if last != SYSEX_END || frame.len() == 1 {
            return Err(ParseError::MissingEnd);
        }
        if frame.len() < MIN_FRAME {
            return Err(ParseError::TooShort { len: frame.len() });
        }
        if frame.len() > MAX_FRAME_LEN {
            return Err(ParseError::TooLong { len: frame.len() });
        }
        if let Some((index, &byte)) = frame[1..frame.len() - 1]
            .iter()
            .enumerate()
            .find(|(_, &b)| b > 0x7F)
        {
            return Err(ParseError::ByteOutOfRange {
                index: index + 1,
                byte,
            });
        }
        if frame[1] != MANUFACTURER_ROLAND {
            return Err(ParseError::NotRoland(frame[1]));
        }
        let device = DeviceId::new(frame[2])?;
        if frame[3] != MODEL_ID {
            return Err(ParseError::UnsupportedModel(frame[3]));
        }
        let command = frame[4];
        let body = &frame[5..frame.len() - 2];
        let actual = frame[frame.len() - 2];
        let expected = checksum(body);
        if expected != actual {
            return Err(ParseError::BadChecksum { expected, actual });
        }
        let address = |bytes: &[u8]| Address::from_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
        let message = match command {
            CMD_RQ1 => {
                if body.len() != 8 {
                    return Err(ParseError::Rq1Length(body.len()));
                }
                let (address, size) = (address(&body[0..4])?, address(&body[4..8])?);
                let Some(span) = size.compact().checked_sub(1) else {
                    return Err(ParseError::Rq1ZeroSize);
                };
                address.checked_add_int(span)?;
                Message::Rq1 { address, size }
            }
            CMD_DT1 => {
                if body.len() < 5 {
                    return Err(ParseError::Dt1NoData);
                }
                let address = address(&body[0..4])?;
                // body.len() is at most MAX_FRAME_LEN, so this conversion cannot fail.
                let last = u32::try_from(body.len() - 5).map_err(|_| AddressError::Overflow)?;
                address.checked_add_int(last)?;
                Message::Dt1 {
                    address,
                    data: body[4..].to_vec(),
                }
            }
            other => return Err(ParseError::UnknownCommand(other)),
        };
        Ok(Parsed { device, message })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn a(s: &str) -> Address {
        Address::parse(s).unwrap()
    }

    // Roland's published examples (XP-60/XP-80 MIDI Implementation).
    const RQ1_EXAMPLE: [u8; 15] = [
        0xF0, 0x41, 0x10, 0x6A, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x19, 0x47, 0xF7,
    ];
    const DT1_EXAMPLE: [u8; 12] = [
        0xF0, 0x41, 0x10, 0x6A, 0x12, 0x01, 0x00, 0x00, 0x28, 0x06, 0x51, 0xF7,
    ];

    #[test]
    fn encodes_the_published_rq1_example() {
        let m = Message::Rq1 {
            address: a("01 00 00 00"),
            size: a("00 00 1F 19"),
        };
        assert_eq!(m.encode(DeviceId::DEFAULT).unwrap(), RQ1_EXAMPLE);
    }

    #[test]
    fn encodes_the_published_dt1_example() {
        let m = Message::Dt1 {
            address: a("01 00 00 28"),
            data: vec![0x06],
        };
        assert_eq!(m.encode(DeviceId::DEFAULT).unwrap(), DT1_EXAMPLE);
    }

    #[test]
    fn parses_the_published_examples_back() {
        let p = Message::parse(&RQ1_EXAMPLE).unwrap();
        assert_eq!(p.device, DeviceId::DEFAULT);
        assert_eq!(
            p.message,
            Message::Rq1 {
                address: a("01 00 00 00"),
                size: a("00 00 1F 19")
            }
        );
        let p = Message::parse(&DT1_EXAMPLE).unwrap();
        assert_eq!(
            p.message,
            Message::Dt1 {
                address: a("01 00 00 28"),
                data: vec![6]
            }
        );
    }

    #[test]
    fn device_id_display_numbers() {
        assert_eq!(DeviceId::DEFAULT.display_number(), 17);
        assert_eq!(DeviceId::from_display_number(32).unwrap().byte(), 0x1F);
        assert!(DeviceId::from_display_number(33).is_err());
        assert!(DeviceId::from_display_number(16).is_err());
    }

    #[test]
    fn rejects_corrupt_frames_with_specific_errors() {
        let mut bad = DT1_EXAMPLE;
        bad[10] ^= 0x01;
        assert!(matches!(
            Message::parse(&bad),
            Err(ParseError::BadChecksum { .. })
        ));

        let mut other_model = DT1_EXAMPLE;
        other_model[3] = 0x6B;
        assert_eq!(
            Message::parse(&other_model),
            Err(ParseError::UnsupportedModel(0x6B))
        );

        let mut other_maker = DT1_EXAMPLE;
        other_maker[1] = 0x43;
        assert_eq!(
            Message::parse(&other_maker),
            Err(ParseError::NotRoland(0x43))
        );

        let mut high_bit = DT1_EXAMPLE;
        high_bit[9] = 0x86;
        assert!(matches!(
            Message::parse(&high_bit),
            Err(ParseError::ByteOutOfRange { index: 9, .. })
        ));

        assert!(matches!(
            Message::parse(&DT1_EXAMPLE[..11]),
            Err(ParseError::MissingEnd)
        ));
        assert_eq!(
            Message::parse(&[0xF0, 0x41, 0x10, 0xF7]),
            Err(ParseError::TooShort { len: 4 })
        );
        assert_eq!(Message::parse(&[]), Err(ParseError::TooShort { len: 0 }));
        assert_eq!(
            Message::parse(&[0x41, 0x10, 0x6A]),
            Err(ParseError::MissingStart)
        );

        let mut wrong_device = DT1_EXAMPLE;
        wrong_device[2] = 0x20;
        assert_eq!(
            Message::parse(&wrong_device),
            Err(ParseError::InvalidDeviceId(0x20))
        );
    }

    #[test]
    fn unknown_command_and_rq1_length_are_reported() {
        let body = [0x01, 0x00, 0x00, 0x28, 0x06];
        let mut f = vec![0xF0, 0x41, 0x10, 0x6A, 0x20];
        f.extend_from_slice(&body);
        f.push(checksum(&body));
        f.push(0xF7);
        assert_eq!(Message::parse(&f), Err(ParseError::UnknownCommand(0x20)));

        let mut r = vec![0xF0, 0x41, 0x10, 0x6A, 0x11];
        r.extend_from_slice(&body);
        r.push(checksum(&body));
        r.push(0xF7);
        assert_eq!(Message::parse(&r), Err(ParseError::Rq1Length(5)));
    }

    #[test]
    fn dt1_encoding_validates_data() {
        let empty = Message::Dt1 {
            address: Address::ZERO,
            data: vec![],
        };
        assert_eq!(empty.encode(DeviceId::DEFAULT), Err(EncodeError::EmptyData));
        let high = Message::Dt1 {
            address: Address::ZERO,
            data: vec![1, 0x80],
        };
        assert_eq!(
            high.encode(DeviceId::DEFAULT),
            Err(EncodeError::DataByteOutOfRange {
                index: 1,
                byte: 0x80
            })
        );
    }

    #[test]
    fn a_span_that_leaves_the_address_space_is_an_error_both_ways() {
        // Two bytes starting on the very last address would run past 7F 7F 7F 7F.
        let m = Message::Dt1 {
            address: a("7F 7F 7F 7F"),
            data: vec![1, 2],
        };
        assert_eq!(
            m.encode(DeviceId::DEFAULT),
            Err(EncodeError::AddressOverflow)
        );
        // ...while a single byte on the last address is fine.
        let ok = Message::Dt1 {
            address: a("7F 7F 7F 7F"),
            data: vec![1],
        };
        assert!(ok.encode(DeviceId::DEFAULT).is_ok());

        // Hand-build the same overflowing frame with a valid checksum; parse must refuse it too.
        let body = [0x7F, 0x7F, 0x7F, 0x7F, 1, 2];
        let mut f = vec![0xF0, 0x41, 0x10, 0x6A, 0x12];
        f.extend_from_slice(&body);
        f.push(checksum(&body));
        f.push(0xF7);
        assert_eq!(
            Message::parse(&f),
            Err(ParseError::Address(AddressError::Overflow))
        );
    }

    #[test]
    fn rq1_size_zero_and_overflowing_requests_are_rejected() {
        let zero = Message::Rq1 {
            address: a("01 00 00 00"),
            size: Address::ZERO,
        };
        assert_eq!(zero.encode(DeviceId::DEFAULT), Err(EncodeError::ZeroSize));
        let over = Message::Rq1 {
            address: a("7F 7F 7F 7F"),
            size: a("00 00 00 02"),
        };
        assert_eq!(
            over.encode(DeviceId::DEFAULT),
            Err(EncodeError::AddressOverflow)
        );

        let body = [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00];
        let mut f = vec![0xF0, 0x41, 0x10, 0x6A, 0x11];
        f.extend_from_slice(&body);
        f.push(checksum(&body));
        f.push(0xF7);
        assert_eq!(Message::parse(&f), Err(ParseError::Rq1ZeroSize));
    }

    #[test]
    fn an_oversized_frame_is_rejected_before_it_is_decoded() {
        let mut f = vec![0xF0, 0x41, 0x10, 0x6A, 0x12];
        f.extend(std::iter::repeat_n(0x01, MAX_FRAME_LEN));
        f.push(0x00);
        f.push(0xF7);
        assert!(matches!(
            Message::parse(&f),
            Err(ParseError::TooLong { .. })
        ));
    }

    #[test]
    fn encode_parse_roundtrip_with_other_device_id() {
        let device = DeviceId::from_display_number(20).unwrap();
        let m = Message::Dt1 {
            address: a("11 05 10 00"),
            data: (0..129u8).map(|i| i % 128).collect(),
        };
        let bytes = m.encode(device).unwrap();
        assert_eq!(
            Message::parse(&bytes).unwrap(),
            Parsed { device, message: m }
        );
    }
}
