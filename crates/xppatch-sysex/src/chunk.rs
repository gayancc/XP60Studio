//! Splitting a data block into DT1 messages for the XP-60.
//!
//! The XP-60 transmits whole blocks (up to 129 data bytes) but accepts at most 128 data bytes per DT1, so a 129-byte
//! tone is written as 128 + 1. Splitting delivers identical bytes to identical addresses; each piece starts
//! `previous length` address units later, using base-128 arithmetic.
//!
//! Evidence: manual (128-byte rule) and the legacy hardware log (read-back equal, 4 of 4).

use crate::address::{Address, AddressError};
use crate::message::{EncodeError, Message, ParseError};
use crate::stream::frames;
use thiserror::Error;

/// Maximum data bytes per DT1 sent **to** the XP-60.
pub const MAX_DT1_DATA_TO_DEVICE: usize = 128;

#[derive(Debug, Error, PartialEq, Eq)]
#[non_exhaustive]
pub enum ChunkError {
    #[error("chunk size must be at least 1")]
    ZeroChunk,
    #[error("no data to send")]
    Empty,
    #[error(transparent)]
    Address(#[from] AddressError),
}

/// Split `data` at `address` into DT1 messages of at most `max_data` bytes each, with advancing addresses. The whole
/// span, including its last byte, must stay inside the 28-bit address space. Use [`MAX_DT1_DATA_TO_DEVICE`] when the
/// target is the instrument.
pub fn split_dt1(
    address: Address,
    data: &[u8],
    max_data: usize,
) -> Result<Vec<Message>, ChunkError> {
    if max_data == 0 {
        return Err(ChunkError::ZeroChunk);
    }
    if data.is_empty() {
        return Err(ChunkError::Empty);
    }
    let overflow = |_| AddressError::Overflow;
    // The last byte must fit; checking it up front also covers a piece that ends exactly on the top address.
    let total = u32::try_from(data.len() - 1).map_err(overflow)?;
    address.checked_add_int(total)?;

    let mut out = Vec::with_capacity(data.len().div_ceil(max_data));
    let mut at = address;
    for piece in data.chunks(max_data) {
        out.push(Message::Dt1 {
            address: at,
            data: piece.to_vec(),
        });
        // Advancing past the final piece is unnecessary and could spuriously overflow.
        at = at
            .checked_add_int(u32::try_from(piece.len()).map_err(overflow)?)
            .unwrap_or(at);
    }
    Ok(out)
}

/// Why a stream of DT1 frames could not be cut into smaller ones.
#[derive(Debug, Error, PartialEq, Eq)]
#[non_exhaustive]
pub enum ResplitError {
    #[error("frame {index}: {source}")]
    Parse { index: usize, source: ParseError },
    #[error("frame {index} is not a DT1 data message")]
    NotDt1 { index: usize },
    #[error(transparent)]
    Chunk(#[from] ChunkError),
    #[error(transparent)]
    Encode(#[from] EncodeError),
}

/// Cut every DT1 frame in `bytes` into frames of at most `max_data` data bytes (never more than the instrument's 128),
/// keeping the device ID, the order and the bytes at every address. Frames already small enough are kept as they were.
/// Smaller frames make a whole-patch send gentler on slow interfaces; the data written is identical.
pub fn resplit_dt1(bytes: &[u8], max_data: usize) -> Result<Vec<u8>, ResplitError> {
    let limit = max_data.min(MAX_DT1_DATA_TO_DEVICE);
    if limit == 0 {
        return Err(ChunkError::ZeroChunk.into());
    }
    let mut out = Vec::with_capacity(bytes.len());
    for (index, frame) in frames(bytes).enumerate() {
        let parsed =
            Message::parse(&frame).map_err(|source| ResplitError::Parse { index, source })?;
        let Message::Dt1 { address, data } = parsed.message else {
            return Err(ResplitError::NotDt1 { index });
        };
        for piece in split_dt1(address, &data, limit)? {
            out.extend(piece.encode(parsed.device)?);
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::message::DeviceId;

    #[test]
    fn a_129_byte_tone_splits_128_plus_1_with_carry() {
        let data: Vec<u8> = (0..129u32).map(|i| (i % 128) as u8).collect();
        let base = Address::parse("11 00 10 00").unwrap();
        let parts = split_dt1(base, &data, MAX_DT1_DATA_TO_DEVICE).unwrap();
        assert_eq!(parts.len(), 2);
        match (&parts[0], &parts[1]) {
            (
                Message::Dt1 {
                    address: a0,
                    data: d0,
                },
                Message::Dt1 {
                    address: a1,
                    data: d1,
                },
            ) => {
                assert_eq!(*a0, base);
                assert_eq!(d0.len(), 128);
                assert_eq!(*a1, Address::parse("11 00 11 00").unwrap());
                assert_eq!(d1, &vec![data[128]]);
            }
            other => panic!("unexpected {other:?}"),
        }
    }

    #[test]
    fn small_blocks_are_a_single_message() {
        let parts = split_dt1(Address::ZERO, &[1, 2, 3], 128).unwrap();
        assert_eq!(parts.len(), 1);
    }

    #[test]
    fn rejects_bad_arguments_and_address_overflow() {
        assert_eq!(
            split_dt1(Address::ZERO, &[1], 0),
            Err(ChunkError::ZeroChunk)
        );
        assert_eq!(split_dt1(Address::ZERO, &[], 8), Err(ChunkError::Empty));
        let top = Address::parse("7F 7F 7F 7F").unwrap();
        assert!(matches!(
            split_dt1(top, &[1, 2], 1),
            Err(ChunkError::Address(AddressError::Overflow))
        ));
    }

    #[test]
    fn data_ending_exactly_on_the_top_address_is_fine() {
        let top = Address::parse("7F 7F 7F 7F").unwrap();
        let parts = split_dt1(top, &[1], 1).unwrap();
        assert_eq!(parts.len(), 1);
        let one_before = Address::parse("7F 7F 7F 7E").unwrap();
        let parts = split_dt1(one_before, &[1, 2], 1).unwrap();
        assert_eq!(parts.len(), 2);
    }

    fn dt1_frame(device: DeviceId, address: Address, data: &[u8]) -> Vec<u8> {
        Message::Dt1 {
            address,
            data: data.to_vec(),
        }
        .encode(device)
        .unwrap()
    }

    /// The (address, data) of every DT1 in a stream, for comparing what two streams write.
    fn writes(bytes: &[u8]) -> Vec<(Address, Vec<u8>)> {
        frames(bytes)
            .map(|f| match Message::parse(&f).unwrap().message {
                Message::Dt1 { address, data } => (address, data),
                other => panic!("unexpected {other:?}"),
            })
            .collect()
    }

    /// Flatten writes into byte -> address so that a different cut of the same data compares equal.
    fn memory(bytes: &[u8]) -> std::collections::BTreeMap<u32, u8> {
        let mut m = std::collections::BTreeMap::new();
        for (a, d) in writes(bytes) {
            for (i, b) in d.iter().enumerate() {
                m.insert(a.checked_add_int(i as u32).unwrap().compact(), *b);
            }
        }
        m
    }

    #[test]
    fn resplitting_cuts_big_frames_and_writes_the_same_bytes_to_the_same_places() {
        let device = DeviceId::new(0x11).unwrap();
        let data: Vec<u8> = (0..129u32).map(|i| (i * 7 % 128) as u8).collect();
        let base = Address::parse("03 00 10 00").unwrap();
        let original = dt1_frame(device, base, &data);
        let small = resplit_dt1(&original, 32).unwrap();
        let pieces = writes(&small);
        assert_eq!(
            pieces.iter().map(|(_, d)| d.len()).collect::<Vec<_>>(),
            vec![32, 32, 32, 32, 1]
        );
        assert_eq!(memory(&small), memory(&original));
        // The device ID travels with every piece.
        assert!(frames(&small).all(|f| Message::parse(&f).unwrap().device == device));
    }

    #[test]
    fn a_chunk_at_or_above_the_instruments_limit_changes_nothing_else() {
        let device = DeviceId::DEFAULT;
        let original = dt1_frame(device, Address::ZERO, &[1; 100]);
        assert_eq!(resplit_dt1(&original, 128).unwrap(), original);
        // Asking for more than the instrument accepts is held to 128, not passed on.
        let big = dt1_frame(device, Address::ZERO, &[2; 128]);
        assert_eq!(writes(&resplit_dt1(&big, 4096).unwrap())[0].1.len(), 128);
    }

    #[test]
    fn several_frames_keep_their_order() {
        let device = DeviceId::DEFAULT;
        let a = dt1_frame(device, Address::parse("03 00 00 00").unwrap(), &[1; 70]);
        let b = dt1_frame(device, Address::parse("03 00 10 00").unwrap(), &[2; 70]);
        let both: Vec<u8> = [a.clone(), b.clone()].concat();
        let cut = writes(&resplit_dt1(&both, 50).unwrap());
        assert_eq!(cut.len(), 4);
        assert_eq!(cut[0].0, Address::parse("03 00 00 00").unwrap());
        assert_eq!(cut[2].0, Address::parse("03 00 10 00").unwrap());
        assert_eq!(memory(&resplit_dt1(&both, 50).unwrap()), memory(&both));
    }

    #[test]
    fn refuses_a_zero_chunk_a_data_request_and_a_broken_frame() {
        let device = DeviceId::DEFAULT;
        let ok = dt1_frame(device, Address::ZERO, &[1, 2, 3]);
        assert_eq!(
            resplit_dt1(&ok, 0),
            Err(ResplitError::Chunk(ChunkError::ZeroChunk))
        );
        let rq1 = Message::Rq1 {
            address: Address::ZERO,
            size: Address::parse("00 00 00 10").unwrap(),
        }
        .encode(device)
        .unwrap();
        assert_eq!(
            resplit_dt1(&rq1, 32),
            Err(ResplitError::NotDt1 { index: 0 })
        );
        let mut bad = ok.clone();
        let n = bad.len();
        bad[n - 2] ^= 0x01; // checksum
        assert!(matches!(
            resplit_dt1(&bad, 32),
            Err(ResplitError::Parse { index: 0, .. })
        ));
    }
}
