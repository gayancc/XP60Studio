//! The XP-60 memory map: base addresses and the temporary / permanent distinction.
//!
//! The synth sounds the **temporary area**; panel edits change only that area, and power-off or selecting another
//! sound discards it. A DT1 to `03 00 00 00` is therefore harmless, while a DT1 to `11 nn 00 00` (User Patch) or
//! `10 ..` (User Performance / Rhythm) or the System area overwrites stored data. The boundary is one address byte, so
//! [`classify`] is the single place the application asks "is this write destructive?".
//!
//! Evidence: manual (memory architecture, address map), legacy hardware log, bundle samples.

use crate::address::Address;
use thiserror::Error;

pub const SYSTEM: Address = Address::from_lanes([0x00, 0x00, 0x00, 0x00]);
pub const TEMP_PERFORMANCE: Address = Address::from_lanes([0x01, 0x00, 0x00, 0x00]);
/// Temporary patch used in Patch mode.
pub const TEMP_PATCH: Address = Address::from_lanes([0x03, 0x00, 0x00, 0x00]);

/// Offsets of Patch Tone 1 to 4 within a patch (`10 00`, `12 00`, `14 00`, `16 00`).
pub const PATCH_TONE_OFFSETS: [Address; 4] = [
    Address::from_lanes([0x00, 0x00, 0x10, 0x00]),
    Address::from_lanes([0x00, 0x00, 0x12, 0x00]),
    Address::from_lanes([0x00, 0x00, 0x14, 0x00]),
    Address::from_lanes([0x00, 0x00, 0x16, 0x00]),
];

/// Number of USER slots.
pub const USER_PATCH_COUNT: u8 = 128;
pub const USER_PERFORMANCE_COUNT: u8 = 32;
pub const USER_RHYTHM_COUNT: u8 = 2;
pub const PERFORMANCE_PARTS: u8 = 16;

#[derive(Debug, Error, PartialEq, Eq)]
#[non_exhaustive]
pub enum MemoryError {
    #[error("{what} index {index} is out of range (0..{count})")]
    OutOfRange {
        what: &'static str,
        index: u8,
        count: u8,
    },
}

fn check(what: &'static str, index: u8, count: u8) -> Result<(), MemoryError> {
    if index < count {
        Ok(())
    } else {
        Err(MemoryError::OutOfRange { what, index, count })
    }
}

/// Temporary patch for performance part `part` (0 to 15): `02 pp 00 00`.
pub fn temp_patch_part(part: u8) -> Result<Address, MemoryError> {
    check("part", part, PERFORMANCE_PARTS)?;
    Ok(Address::from_lanes([0x02, part, 0x00, 0x00]))
}

/// User patch `slot` (0 to 127, shown as USER:001 to 128): `11 ss 00 00`.
pub fn user_patch(slot: u8) -> Result<Address, MemoryError> {
    check("user patch", slot, USER_PATCH_COUNT)?;
    Ok(Address::from_lanes([0x11, slot, 0x00, 0x00]))
}

/// User performance `slot` (0 to 31): `10 ss 00 00`.
pub fn user_performance(slot: u8) -> Result<Address, MemoryError> {
    check("user performance", slot, USER_PERFORMANCE_COUNT)?;
    Ok(Address::from_lanes([0x10, slot, 0x00, 0x00]))
}

/// User rhythm setup `slot` (0 or 1): `10 40 00 00`, `10 41 00 00`.
pub fn user_rhythm(slot: u8) -> Result<Address, MemoryError> {
    check("user rhythm", slot, USER_RHYTHM_COUNT)?;
    Ok(Address::from_lanes([0x10, 0x40 + slot, 0x00, 0x00]))
}

/// Offset of performance part `part` (0 to 15) within a performance: `10 00` to `1F 00`.
pub fn performance_part_offset(part: u8) -> Result<Address, MemoryError> {
    check("part", part, PERFORMANCE_PARTS)?;
    Ok(Address::from_lanes([0x00, 0x00, 0x10 + part, 0x00]))
}

/// What a write to an address does to the instrument.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum WriteKind {
    /// Temporary memory: audible now, lost on the next sound change. Safe.
    Temporary,
    /// System or User memory: persistent. Destructive; needs the explicit write ceremony.
    Permanent,
    /// Not a region this application knows. Never written.
    Unknown,
}

/// Classify the region an address falls in, by its first lane.
pub fn classify(address: Address) -> WriteKind {
    match address.bytes()[0] {
        0x01..=0x03 => WriteKind::Temporary,
        0x00 | 0x10 | 0x11 => WriteKind::Permanent,
        _ => WriteKind::Unknown,
    }
}

/// Classify a write of `len` bytes starting at `address`: the most destructive result of its first and last byte.
/// A span that is empty or runs past the address space is [`WriteKind::Unknown`]. Use this, not [`classify`], whenever
/// the length is known, because a write can start in temporary memory and carry into the next region.
pub fn classify_range(address: Address, len: usize) -> WriteKind {
    let last = len
        .checked_sub(1)
        .and_then(|n| u32::try_from(n).ok())
        .and_then(|n| address.checked_add_int(n).ok());
    let Some(last) = last else {
        return WriteKind::Unknown;
    };
    match (classify(address), classify(last)) {
        (WriteKind::Temporary, WriteKind::Temporary) => WriteKind::Temporary,
        (WriteKind::Permanent, _) | (_, WriteKind::Permanent) => WriteKind::Permanent,
        _ => WriteKind::Unknown,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn a(s: &str) -> Address {
        Address::parse(s).unwrap()
    }

    #[test]
    fn slot_addresses() {
        assert_eq!(user_patch(0).unwrap(), a("11 00 00 00"));
        assert_eq!(user_patch(127).unwrap(), a("11 7F 00 00"));
        assert_eq!(user_performance(31).unwrap(), a("10 1F 00 00"));
        assert_eq!(user_rhythm(0).unwrap(), a("10 40 00 00"));
        assert_eq!(user_rhythm(1).unwrap(), a("10 41 00 00"));
        assert_eq!(temp_patch_part(9).unwrap(), a("02 09 00 00"));
        assert_eq!(performance_part_offset(15).unwrap(), a("00 00 1F 00"));
    }

    #[test]
    fn out_of_range_slots_are_errors() {
        assert!(user_patch(128).is_err());
        assert!(user_performance(32).is_err());
        assert!(user_rhythm(2).is_err());
        assert!(temp_patch_part(16).is_err());
    }

    #[test]
    fn the_destructive_boundary_is_one_byte() {
        assert_eq!(classify(TEMP_PATCH), WriteKind::Temporary);
        assert_eq!(classify(TEMP_PERFORMANCE), WriteKind::Temporary);
        assert_eq!(classify(temp_patch_part(3).unwrap()), WriteKind::Temporary);
        assert_eq!(classify(user_patch(5).unwrap()), WriteKind::Permanent);
        assert_eq!(classify(user_performance(5).unwrap()), WriteKind::Permanent);
        assert_eq!(classify(user_rhythm(1).unwrap()), WriteKind::Permanent);
        assert_eq!(classify(SYSTEM), WriteKind::Permanent);
        assert_eq!(classify(a("40 00 00 00")), WriteKind::Unknown);
    }

    #[test]
    fn patch_tone_offsets_are_0x200_apart() {
        for pair in PATCH_TONE_OFFSETS.windows(2) {
            assert_eq!(pair[1].checked_sub(pair[0]).unwrap().compact(), 0x100);
        }
    }

    #[test]
    fn a_write_is_classified_by_where_it_ends_too() {
        assert_eq!(classify_range(TEMP_PATCH, 129), WriteKind::Temporary);
        // Starts in temporary memory but carries into the unknown region 04.
        assert_eq!(classify_range(a("03 7F 7F 70"), 128), WriteKind::Unknown);
        // Starts in System (permanent) and carries into temporary memory: still permanent.
        assert_eq!(classify_range(a("00 7F 7F 70"), 128), WriteKind::Permanent);
        // 02 carries into 03: both temporary.
        assert_eq!(classify_range(a("02 7F 7F 70"), 128), WriteKind::Temporary);
        assert_eq!(classify_range(TEMP_PATCH, 0), WriteKind::Unknown);
        assert_eq!(classify_range(a("7F 7F 7F 7F"), 2), WriteKind::Unknown);
    }
}
