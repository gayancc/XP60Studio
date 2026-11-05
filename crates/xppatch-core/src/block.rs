//! A block of data bytes with typed access to its parameters, driven entirely by the parameter map.

use crate::schema::{BlockDef, Encoding, ParamDef};
use thiserror::Error;

#[derive(Debug, Error, PartialEq, Eq, Clone)]
#[non_exhaustive]
pub enum CodecError {
    #[error("unknown parameter {0:?}")]
    UnknownParam(String),
    #[error("{param}: value {value} is outside {min}..={max}")]
    OutOfRange {
        param: String,
        value: i32,
        min: i32,
        max: i32,
    },
    #[error("block {block}: expected {expected} bytes, found {actual}")]
    WrongLength {
        block: String,
        expected: usize,
        actual: usize,
    },
    #[error("{param}: byte {byte:#04x} at offset {index} is not a valid low nibble")]
    BadNibble {
        param: String,
        index: usize,
        byte: u8,
    },
    #[error("block {block}: data byte {byte:#04x} at offset {index} exceeds 0x7F")]
    ByteOutOfRange {
        block: String,
        index: usize,
        byte: u8,
    },
    #[error("text is {len} characters but the block holds {max}")]
    TextTooLong { len: usize, max: usize },
}

/// What is wrong with one parameter's stored value.
#[derive(Debug, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum IssueKind {
    OutOfRange {
        value: i32,
        min: i32,
        max: i32,
    },
    BadNibble {
        index: usize,
        byte: u8,
    },
    /// Any other decoding failure, described in words.
    Malformed(String),
}

/// One problem found by [`BlockBuf::validate`].
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Issue {
    pub param: String,
    pub kind: IssueKind,
}

/// The data bytes of one block, with access by parameter id.
#[derive(Debug, Clone)]
pub struct BlockBuf {
    def: &'static BlockDef,
    data: Vec<u8>,
}

/// Two blocks are equal when they describe the same block type and hold the same bytes.
impl PartialEq for BlockBuf {
    fn eq(&self, other: &Self) -> bool {
        self.def.id == other.def.id && self.data == other.data
    }
}

impl Eq for BlockBuf {}

impl BlockBuf {
    /// A block with every parameter at its minimum (names become spaces).
    pub fn init(def: &'static BlockDef) -> Self {
        let mut block = BlockBuf {
            def,
            data: vec![0; def.size],
        };
        for p in &def.params {
            // A definition that failed validation leaves that parameter zeroed instead of panicking.
            let _ = block.set_param(p, p.min);
        }
        block
    }

    /// Wrap received bytes. The length must equal the block size and every byte must be 7-bit.
    ///
    /// This does **not** check parameter ranges: imported data may be out of range and should be inspected with
    /// [`BlockBuf::validate`]. Producing messages from invalid data is refused one level up (`Patch::to_dt1`).
    pub fn from_bytes(def: &'static BlockDef, bytes: &[u8]) -> Result<Self, CodecError> {
        if bytes.len() != def.size {
            return Err(CodecError::WrongLength {
                block: def.id.clone(),
                expected: def.size,
                actual: bytes.len(),
            });
        }
        if let Some((index, &byte)) = bytes.iter().enumerate().find(|(_, &b)| b > 0x7F) {
            return Err(CodecError::ByteOutOfRange {
                block: def.id.clone(),
                index,
                byte,
            });
        }
        Ok(BlockBuf {
            def,
            data: bytes.to_vec(),
        })
    }

    pub fn def(&self) -> &'static BlockDef {
        self.def
    }

    pub fn bytes(&self) -> &[u8] {
        &self.data
    }

    /// Only parameters that belong to this block's definition (and fit inside it) may be used.
    fn check(&self, p: &ParamDef) -> Result<(), CodecError> {
        let owns = self
            .def
            .param(&p.id)
            .is_some_and(|own| std::ptr::eq(own, p));
        let fits = p
            .offset
            .checked_add(p.bytes)
            .is_some_and(|end| end <= self.data.len());
        if owns && fits {
            Ok(())
        } else {
            Err(CodecError::UnknownParam(p.id.clone()))
        }
    }

    /// Read a parameter's stored value (not range-checked; see [`BlockBuf::validate`]).
    pub fn get(&self, id: &str) -> Result<i32, CodecError> {
        let p = self
            .def
            .param(id)
            .ok_or_else(|| CodecError::UnknownParam(id.to_owned()))?;
        self.get_param(p)
    }

    pub fn get_param(&self, p: &ParamDef) -> Result<i32, CodecError> {
        self.check(p)?;
        match p.encoding {
            Encoding::Seven | Encoding::Ascii => Ok(i32::from(self.data[p.offset])),
            Encoding::Nibble => {
                let mut value = 0i32;
                for i in 0..p.bytes {
                    let byte = self.data[p.offset + i];
                    if byte > 0x0F {
                        return Err(CodecError::BadNibble {
                            param: p.id.clone(),
                            index: p.offset + i,
                            byte,
                        });
                    }
                    value = (value << 4) | i32::from(byte);
                }
                Ok(value)
            }
        }
    }

    /// Store a value, rejecting anything outside the parameter's raw range.
    pub fn set(&mut self, id: &str, value: i32) -> Result<(), CodecError> {
        let def = self.def;
        let p = def
            .param(id)
            .ok_or_else(|| CodecError::UnknownParam(id.to_owned()))?;
        self.set_param(p, value)
    }

    pub fn set_param(&mut self, p: &ParamDef, value: i32) -> Result<(), CodecError> {
        self.check(p)?;
        if value < p.min || value > p.max {
            return Err(CodecError::OutOfRange {
                param: p.id.clone(),
                value,
                min: p.min,
                max: p.max,
            });
        }
        self.write(p, value);
        Ok(())
    }

    /// Every parameter whose stored value is outside its range (or malformed).
    pub fn validate(&self) -> Vec<Issue> {
        self.def
            .params
            .iter()
            .filter_map(|p| {
                let kind = match self.get_param(p) {
                    Ok(v) if v < p.min || v > p.max => IssueKind::OutOfRange {
                        value: v,
                        min: p.min,
                        max: p.max,
                    },
                    Ok(_) => return None,
                    Err(CodecError::BadNibble { index, byte, .. }) => {
                        IssueKind::BadNibble { index, byte }
                    }
                    Err(other) => IssueKind::Malformed(other.to_string()),
                };
                Some(Issue {
                    param: p.id.clone(),
                    kind,
                })
            })
            .collect()
    }

    /// The block's ASCII characters joined in order (a patch name), trailing spaces removed. Bytes that are not
    /// printable ASCII are shown as `?`, so a hostile name cannot smuggle control characters into logs or the UI.
    pub fn text(&self) -> String {
        let s: String = self
            .def
            .params
            .iter()
            .filter(|p| p.encoding == Encoding::Ascii)
            .filter_map(|p| self.data.get(p.offset).copied())
            .map(|b| {
                if (0x20..0x7F).contains(&b) {
                    char::from(b)
                } else {
                    '?'
                }
            })
            .collect();
        s.trim_end().to_owned()
    }

    /// Set the block's ASCII characters from `text`, padding with spaces. Each character must lie inside its
    /// parameter's raw range.
    pub fn set_text(&mut self, text: &str) -> Result<(), CodecError> {
        let def = self.def;
        let slots: Vec<&ParamDef> = def
            .params
            .iter()
            .filter(|p| p.encoding == Encoding::Ascii)
            .collect();
        let len = text.chars().count();
        if len > slots.len() {
            return Err(CodecError::TextTooLong {
                len,
                max: slots.len(),
            });
        }
        let mut chars = text.chars();
        let mut staged = Vec::with_capacity(slots.len());
        for p in &slots {
            let v = chars.next().unwrap_or(' ') as i32;
            if v < p.min || v > p.max {
                return Err(CodecError::OutOfRange {
                    param: p.id.clone(),
                    value: v,
                    min: p.min,
                    max: p.max,
                });
            }
            staged.push((*p, v));
        }
        for (p, v) in staged {
            self.set_param(p, v)?;
        }
        Ok(())
    }

    fn write(&mut self, p: &ParamDef, value: i32) {
        match p.encoding {
            Encoding::Seven | Encoding::Ascii => self.data[p.offset] = (value & 0x7F) as u8,
            Encoding::Nibble => {
                for i in 0..p.bytes {
                    let shift = 4 * (p.bytes - 1 - i);
                    self.data[p.offset + i] = ((value >> shift) & 0x0F) as u8;
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::map::ParameterMap;

    fn common() -> &'static BlockDef {
        ParameterMap::embedded()
            .block("patch_common")
            .expect("block")
    }

    fn tone() -> &'static BlockDef {
        ParameterMap::embedded().block("patch_tone").expect("block")
    }

    #[test]
    fn init_sets_every_parameter_to_its_minimum() {
        let b = BlockBuf::init(common());
        assert!(b.validate().is_empty());
        assert_eq!(b.get("patch_tempo").unwrap(), 20);
        assert_eq!(b.text(), "");
    }

    #[test]
    fn nibble_values_are_two_bytes_high_first() {
        let mut b = BlockBuf::init(common());
        b.set("patch_tempo", 0xA7).unwrap();
        let p = common().param("patch_tempo").unwrap();
        assert_eq!(&b.bytes()[p.offset..p.offset + 2], &[0x0A, 0x07]);
        assert_eq!(b.get("patch_tempo").unwrap(), 0xA7);
    }

    #[test]
    fn set_rejects_out_of_range_and_unknown() {
        let mut b = BlockBuf::init(common());
        assert!(matches!(
            b.set("patch_tempo", 251),
            Err(CodecError::OutOfRange { .. })
        ));
        assert!(matches!(
            b.set("patch_tempo", 19),
            Err(CodecError::OutOfRange { .. })
        ));
        assert!(matches!(
            b.set("no_such", 0),
            Err(CodecError::UnknownParam(_))
        ));
        assert_eq!(
            b.get("patch_tempo").unwrap(),
            20,
            "a rejected set must not change the block"
        );
    }

    #[test]
    fn a_parameter_from_another_block_is_rejected_not_a_panic() {
        // The tone block is 129 bytes; its parameters index far past the 73-byte common block.
        let foreign = tone()
            .params
            .iter()
            .max_by_key(|p| p.offset)
            .expect("tone has params");
        assert!(foreign.offset >= common().size);
        let mut b = BlockBuf::init(common());
        assert!(matches!(
            b.get_param(foreign),
            Err(CodecError::UnknownParam(_))
        ));
        assert!(matches!(
            b.set_param(foreign, foreign.min),
            Err(CodecError::UnknownParam(_))
        ));
        // A clone of a real parameter is not the block's own definition either.
        let own_clone = common().param("patch_tempo").unwrap().clone();
        assert!(b.get_param(&own_clone).is_err());
    }

    #[test]
    fn text_roundtrips_and_validates() {
        let mut b = BlockBuf::init(common());
        b.set_text("Warm Pad").unwrap();
        assert_eq!(b.text(), "Warm Pad");
        assert!(matches!(
            b.set_text("thirteen chars"),
            Err(CodecError::TextTooLong { len: 14, max: 12 })
        ));
        assert!(b.set_text("caf\u{e9}").is_err());
        assert_eq!(
            b.text(),
            "Warm Pad",
            "a rejected name must not change the block"
        );
    }

    #[test]
    fn hostile_name_bytes_are_shown_as_question_marks() {
        let mut bytes = BlockBuf::init(common()).bytes().to_vec();
        bytes[0] = 0x1B; // ESC
        bytes[1] = b'A';
        bytes[2] = 0x7F; // DEL
        bytes[3] = 0x00;
        let b = BlockBuf::from_bytes(common(), &bytes).unwrap();
        assert_eq!(b.text(), "?A??");
    }

    #[test]
    fn from_bytes_checks_length_and_7_bit() {
        assert!(matches!(
            BlockBuf::from_bytes(common(), &[0; 5]),
            Err(CodecError::WrongLength { .. })
        ));
        let mut bad = vec![0u8; 73];
        bad[3] = 0x80;
        assert!(matches!(
            BlockBuf::from_bytes(common(), &bad),
            Err(CodecError::ByteOutOfRange { index: 3, .. })
        ));
    }

    #[test]
    fn validate_reports_bad_values_and_bad_nibbles() {
        let mut bytes = BlockBuf::init(common()).bytes().to_vec();
        let tempo = common().param("patch_tempo").unwrap().offset;
        bytes[tempo] = 0x1F;
        let p = common().param("patch_level").unwrap();
        bytes[p.offset] = 0x7F;
        let b = BlockBuf::from_bytes(common(), &bytes).unwrap();
        let issues = b.validate();
        assert!(issues
            .iter()
            .any(|i| i.param == "patch_tempo" && matches!(i.kind, IssueKind::BadNibble { .. })));
        assert!(b.get("patch_tempo").is_err());
    }
}
