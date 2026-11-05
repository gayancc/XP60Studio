//! Roland 4-byte addresses and sizes.
//!
//! The four bytes each carry 7 bits and are written most significant first, so a value is a **base-128 number**
//! (28 bits in total). Arithmetic converts to that compact integer, operates, and converts back, so carries flow
//! across the 7-bit lanes: `00 00 00 7F + 1 = 00 00 01 00`.
//!
//! Evidence: the official manual (7-bit carry) and the Patch Base code (`artifact/docs/xppatchstudio/05-behaviour-spec.md`, B-03).

use std::fmt;
use thiserror::Error;

/// Largest value representable in four 7-bit lanes.
pub const MAX_COMPACT: u32 = 0x0FFF_FFFF;

/// Errors from address construction and arithmetic.
#[derive(Debug, Error, PartialEq, Eq, Clone)]
#[non_exhaustive]
pub enum AddressError {
    /// A lane byte was above `0x7F`.
    #[error("address byte {byte:#04x} at position {index} exceeds 0x7F")]
    ByteOutOfRange { index: usize, byte: u8 },
    /// The result does not fit in 28 bits (or went below zero).
    #[error("address arithmetic left the 28-bit range")]
    Overflow,
    /// Text could not be read as four hex bytes.
    #[error("invalid address text: {0:?}")]
    Parse(String),
}

/// A 28-bit value stored as four 7-bit lanes. Used for both addresses and RQ1 sizes.
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Default)]
pub struct Address(u32);

/// An RQ1 size uses the same lane encoding as an address.
pub type Size = Address;

impl Address {
    /// All four lanes zero.
    pub const ZERO: Address = Address(0);

    /// Build from four lane bytes (most significant first). Every byte must be at most `0x7F`.
    pub fn from_bytes(bytes: [u8; 4]) -> Result<Self, AddressError> {
        let mut v: u32 = 0;
        for (index, &byte) in bytes.iter().enumerate() {
            if byte > 0x7F {
                return Err(AddressError::ByteOutOfRange { index, byte });
            }
            v = (v << 7) | u32::from(byte);
        }
        Ok(Address(v))
    }

    /// Const constructor for fixed addresses in source. Panics at compile time if a lane exceeds `0x7F`.
    pub const fn from_lanes(b: [u8; 4]) -> Address {
        assert!(b[0] <= 0x7F && b[1] <= 0x7F && b[2] <= 0x7F && b[3] <= 0x7F);
        Address(
            ((b[0] as u32) << 21) | ((b[1] as u32) << 14) | ((b[2] as u32) << 7) | (b[3] as u32),
        )
    }

    /// Build from the compact 28-bit integer.
    pub fn from_compact(value: u32) -> Result<Self, AddressError> {
        if value > MAX_COMPACT {
            return Err(AddressError::Overflow);
        }
        Ok(Address(value))
    }

    /// The compact 28-bit integer (`b0<<21 | b1<<14 | b2<<7 | b3`).
    pub const fn compact(self) -> u32 {
        self.0
    }

    /// The four lane bytes, most significant first.
    pub const fn bytes(self) -> [u8; 4] {
        [
            ((self.0 >> 21) & 0x7F) as u8,
            ((self.0 >> 14) & 0x7F) as u8,
            ((self.0 >> 7) & 0x7F) as u8,
            (self.0 & 0x7F) as u8,
        ]
    }

    /// `self + rhs` with carry across lanes.
    pub fn checked_add(self, rhs: Address) -> Result<Address, AddressError> {
        Address::from_compact(self.0.checked_add(rhs.0).ok_or(AddressError::Overflow)?)
    }

    /// `self + n` where `n` is an ordinary integer (for example a byte count).
    pub fn checked_add_int(self, n: u32) -> Result<Address, AddressError> {
        Address::from_compact(self.0.checked_add(n).ok_or(AddressError::Overflow)?)
    }

    /// `self - rhs`; fails if the result would be negative.
    pub fn checked_sub(self, rhs: Address) -> Result<Address, AddressError> {
        Ok(Address(
            self.0.checked_sub(rhs.0).ok_or(AddressError::Overflow)?,
        ))
    }

    /// Parse text such as `"03 00 10 00"` (hex bytes separated by spaces).
    pub fn parse(text: &str) -> Result<Self, AddressError> {
        let parts: Vec<&str> = text.split_whitespace().collect();
        if parts.len() != 4 {
            return Err(AddressError::Parse(text.to_owned()));
        }
        let mut bytes = [0u8; 4];
        for (slot, part) in bytes.iter_mut().zip(parts) {
            *slot = u8::from_str_radix(part, 16)
                .ok()
                .filter(|_| part.chars().all(|c| c.is_ascii_hexdigit()))
                .ok_or(())
                .map_err(|_| AddressError::Parse(text.to_owned()))?;
        }
        Address::from_bytes(bytes)
    }
}

impl fmt::Display for Address {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let b = self.bytes();
        write!(f, "{:02X} {:02X} {:02X} {:02X}", b[0], b[1], b[2], b[3])
    }
}

impl fmt::Debug for Address {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Address({self})")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use proptest::prelude::*;

    fn a(s: &str) -> Address {
        Address::parse(s).unwrap()
    }

    #[test]
    fn carries_across_7_bit_lanes() {
        assert_eq!(
            a("00 00 00 7F").checked_add_int(1).unwrap(),
            a("00 00 01 00")
        );
        assert_eq!(
            a("03 00 10 00").checked_add(a("00 00 02 00")).unwrap(),
            a("03 00 12 00")
        );
        assert_eq!(
            a("00 00 7F 7F").checked_add_int(1).unwrap(),
            a("00 01 00 00")
        );
        // a 129-byte tone split 128 + 1: the second piece starts 128 units later.
        assert_eq!(
            a("11 00 10 00").checked_add_int(128).unwrap(),
            a("11 00 11 00")
        );
    }

    #[test]
    fn maximum_is_7f_7f_7f_7f_and_overflows_past_it() {
        let max = a("7F 7F 7F 7F");
        assert_eq!(max.compact(), MAX_COMPACT);
        assert_eq!(max.checked_add_int(1), Err(AddressError::Overflow));
        assert_eq!(
            Address::ZERO.checked_sub(a("00 00 00 01")),
            Err(AddressError::Overflow)
        );
    }

    #[test]
    fn rejects_bytes_above_7f_and_bad_text() {
        assert_eq!(
            Address::from_bytes([0, 0x80, 0, 0]),
            Err(AddressError::ByteOutOfRange {
                index: 1,
                byte: 0x80
            })
        );
        assert!(Address::parse("01 02 03").is_err());
        assert!(Address::parse("01 02 03 zz").is_err());
        assert!(Address::parse("01 02 03 80").is_err());
        assert!(
            Address::parse("01 02 03 +7").is_err(),
            "a leading plus sign is not hex"
        );
    }

    #[test]
    fn displays_as_spaced_uppercase_hex() {
        assert_eq!(a("03 00 10 0a").to_string(), "03 00 10 0A");
    }

    proptest! {
        #[test]
        fn bytes_roundtrip(v in 0u32..=MAX_COMPACT) {
            let addr = Address::from_compact(v).unwrap();
            prop_assert_eq!(Address::from_bytes(addr.bytes()).unwrap(), addr);
        }

        #[test]
        fn add_then_sub_is_identity(x in 0u32..=MAX_COMPACT / 2, y in 0u32..=MAX_COMPACT / 2) {
            let (x, y) = (Address::from_compact(x).unwrap(), Address::from_compact(y).unwrap());
            prop_assert_eq!(x.checked_add(y).unwrap().checked_sub(y).unwrap(), x);
        }

        #[test]
        fn lane_bytes_never_exceed_7f(v in 0u32..=MAX_COMPACT) {
            prop_assert!(Address::from_compact(v).unwrap().bytes().iter().all(|&b| b <= 0x7F));
        }
    }
}
