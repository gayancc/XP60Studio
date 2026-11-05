//! A Patch: one Common block and four Tone blocks, with conversion to and from DT1 pieces.

use crate::block::{BlockBuf, CodecError, Issue};
use crate::map::ParameterMap;
use crate::schema::BlockDef;
use thiserror::Error;
use xppatch_sysex::address::{Address, AddressError};
use xppatch_sysex::chunk::ChunkError;
use xppatch_sysex::memory::{MemoryError, PATCH_TONE_OFFSETS};
use xppatch_sysex::message::EncodeError;

#[derive(Debug, Error, PartialEq, Eq)]
#[non_exhaustive]
pub enum PatchError {
    #[error(transparent)]
    Address(#[from] AddressError),
    #[error(transparent)]
    Codec(#[from] CodecError),
    #[error(transparent)]
    Memory(#[from] MemoryError),
    #[error(transparent)]
    Chunk(#[from] ChunkError),
    #[error(transparent)]
    Encode(#[from] EncodeError),
    #[error("data at {address} does not fall inside one patch block")]
    OutsideBlock { address: Address },
    #[error("block {block} is incomplete: {missing} byte(s) missing")]
    Incomplete { block: String, missing: usize },
    #[error("conflicting data was written twice at {address}")]
    Overlap { address: Address },
    #[error("no such patch block")]
    UnknownBlock,
    #[error("patch has {count} out-of-range or malformed value(s); refusing to produce messages")]
    Invalid { count: usize },
}

/// Number of tones in a patch.
pub const TONES: usize = 4;

/// One of a patch's five blocks.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PatchBlock {
    Common,
    /// Tone 0 to 3 (shown to the musician as Tone 1 to 4).
    Tone(u8),
}

impl PatchBlock {
    /// "common", "tone1" to "tone4".
    pub fn parse(label: &str) -> Option<PatchBlock> {
        match label {
            "common" => Some(PatchBlock::Common),
            "tone1" => Some(PatchBlock::Tone(0)),
            "tone2" => Some(PatchBlock::Tone(1)),
            "tone3" => Some(PatchBlock::Tone(2)),
            "tone4" => Some(PatchBlock::Tone(3)),
            _ => None,
        }
    }

    pub fn label(self) -> String {
        match self {
            PatchBlock::Common => "common".to_owned(),
            PatchBlock::Tone(n) => format!("tone{}", u32::from(n) + 1),
        }
    }

    /// Position in transmission order (Common first).
    pub fn index(self) -> usize {
        match self {
            PatchBlock::Common => 0,
            PatchBlock::Tone(n) => usize::from(n) + 1,
        }
    }
}

/// A complete Patch.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Patch {
    pub common: BlockBuf,
    pub tones: [BlockBuf; TONES],
}

fn defs() -> (&'static BlockDef, &'static BlockDef) {
    let map = ParameterMap::embedded();
    // Both blocks are required by the embedded map and asserted by its tests.
    (
        map.block("patch_common")
            .expect("embedded map has patch_common"),
        map.block("patch_tone")
            .expect("embedded map has patch_tone"),
    )
}

/// Compact offsets of the five blocks inside a patch, in transmission order.
fn offsets() -> [u32; 5] {
    [
        0,
        PATCH_TONE_OFFSETS[0].compact(),
        PATCH_TONE_OFFSETS[1].compact(),
        PATCH_TONE_OFFSETS[2].compact(),
        PATCH_TONE_OFFSETS[3].compact(),
    ]
}

impl Patch {
    /// A patch with every parameter at its minimum.
    pub fn init() -> Patch {
        let (c, t) = defs();
        Patch {
            common: BlockBuf::init(c),
            tones: std::array::from_fn(|_| BlockBuf::init(t)),
        }
    }

    /// Bytes in the flat form: Common (73) then Tone 1 to 4 (129 each).
    pub const BLOCK_BYTES: usize = 73 + 4 * 129;

    /// All five blocks' data, concatenated (Common, then Tone 1 to 4). Used for storage and undo snapshots.
    pub fn to_blocks(&self) -> Vec<u8> {
        let mut out = self.common.bytes().to_vec();
        for t in &self.tones {
            out.extend_from_slice(t.bytes());
        }
        out
    }

    /// The inverse of [`Patch::to_blocks`]. Values are not range-checked (use [`Patch::validate`]).
    pub fn from_blocks(bytes: &[u8]) -> Result<Patch, PatchError> {
        if bytes.len() != Patch::BLOCK_BYTES {
            return Err(PatchError::Incomplete {
                block: "patch".to_owned(),
                missing: Patch::BLOCK_BYTES.saturating_sub(bytes.len()),
            });
        }
        let (cdef, tdef) = defs();
        let (c, rest) = bytes.split_at(cdef.size);
        let mut tones = rest.chunks(tdef.size);
        let mut next = || -> Result<BlockBuf, PatchError> {
            let chunk = tones.next().ok_or(PatchError::UnknownBlock)?;
            Ok(BlockBuf::from_bytes(tdef, chunk)?)
        };
        Ok(Patch {
            common: BlockBuf::from_bytes(cdef, c)?,
            tones: [next()?, next()?, next()?, next()?],
        })
    }

    pub fn name(&self) -> String {
        self.common.text()
    }

    pub fn set_name(&mut self, name: &str) -> Result<(), CodecError> {
        self.common.set_text(name)
    }

    pub fn block(&self, block: PatchBlock) -> Option<&BlockBuf> {
        match block {
            PatchBlock::Common => Some(&self.common),
            PatchBlock::Tone(n) => self.tones.get(usize::from(n)),
        }
    }

    pub fn block_mut(&mut self, block: PatchBlock) -> Option<&mut BlockBuf> {
        match block {
            PatchBlock::Common => Some(&mut self.common),
            PatchBlock::Tone(n) => self.tones.get_mut(usize::from(n)),
        }
    }

    /// The single DT1 that carries one parameter's current value, for a patch based at `base` (live editing sends
    /// just this, not the whole patch). Refuses a value that is out of range.
    pub fn param_dt1(
        &self,
        base: Address,
        block: PatchBlock,
        id: &str,
    ) -> Result<(Address, Vec<u8>), PatchError> {
        let buf = self.block(block).ok_or(PatchError::UnknownBlock)?;
        let def = buf
            .def()
            .param(id)
            .ok_or_else(|| CodecError::UnknownParam(id.to_owned()))?;
        let value = buf.get_param(def)?;
        if value < def.min || value > def.max {
            return Err(PatchError::Invalid { count: 1 });
        }
        let start = offsets()[block.index()]
            .checked_add(u32::try_from(def.offset).map_err(|_| AddressError::Overflow)?)
            .ok_or(AddressError::Overflow)?;
        let data = buf.bytes()[def.offset..def.offset + def.bytes].to_vec();
        Ok((base.checked_add(Address::from_compact(start)?)?, data))
    }

    /// Range problems across all five blocks, as `(block label, issue)`.
    pub fn validate(&self) -> Vec<(String, Issue)> {
        let mut out: Vec<(String, Issue)> = self
            .common
            .validate()
            .into_iter()
            .map(|i| ("common".to_owned(), i))
            .collect();
        for (n, tone) in self.tones.iter().enumerate() {
            out.extend(
                tone.validate()
                    .into_iter()
                    .map(|i| (format!("tone{}", n + 1), i)),
            );
        }
        out
    }

    /// The five `(address, data)` pieces (Common, then Tone 1 to 4) for a patch based at `base`.
    ///
    /// Refuses a patch that holds out-of-range or malformed values, so imported bad data can never reach a send or
    /// export path by accident.
    pub fn to_dt1(&self, base: Address) -> Result<Vec<(Address, Vec<u8>)>, PatchError> {
        let issues = self.validate().len();
        if issues > 0 {
            return Err(PatchError::Invalid { count: issues });
        }
        let blocks = std::iter::once(&self.common).chain(self.tones.iter());
        let mut out = Vec::with_capacity(5);
        for (block, off) in blocks.zip(offsets()) {
            out.push((
                base.checked_add(Address::from_compact(off)?)?,
                block.bytes().to_vec(),
            ));
        }
        Ok(out)
    }

    /// Rebuild a patch from DT1 pieces in any order and any chunking. Every byte of every block must be present.
    /// Pieces may overlap only if the overlapping bytes are identical; conflicting data is an error.
    ///
    /// Values are **not** range-checked here (so a bad file can be loaded and inspected with [`Patch::validate`]).
    pub fn from_dt1(base: Address, pieces: &[(Address, Vec<u8>)]) -> Result<Patch, PatchError> {
        let (cdef, tdef) = defs();
        let off = offsets();
        let layout: [(u32, &'static BlockDef); 5] = [
            (off[0], cdef),
            (off[1], tdef),
            (off[2], tdef),
            (off[3], tdef),
            (off[4], tdef),
        ];
        let blocks = crate::doc::assemble(base, &layout, pieces)?;
        let Ok([common, t1, t2, t3, t4]) = <[BlockBuf; 5]>::try_from(blocks) else {
            // Five blocks are always pushed above; reaching this would be a logic error, reported rather than panicking.
            return Err(PatchError::Incomplete {
                block: "patch".to_owned(),
                missing: 0,
            });
        };
        Ok(Patch {
            common,
            tones: [t1, t2, t3, t4],
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use xppatch_sysex::chunk::{split_dt1, MAX_DT1_DATA_TO_DEVICE};
    use xppatch_sysex::memory::user_patch;
    use xppatch_sysex::message::Message;

    fn as_pieces(msgs: Vec<Message>) -> Vec<(Address, Vec<u8>)> {
        msgs.into_iter()
            .map(|m| match m {
                Message::Dt1 { address, data } => (address, data),
                Message::Rq1 { .. } => unreachable!(),
            })
            .collect()
    }

    #[test]
    fn five_pieces_at_the_documented_offsets() {
        let base = user_patch(7).unwrap();
        let pieces = Patch::init().to_dt1(base).unwrap();
        let addrs: Vec<String> = pieces.iter().map(|(a, _)| a.to_string()).collect();
        assert_eq!(
            addrs,
            [
                "11 07 00 00",
                "11 07 10 00",
                "11 07 12 00",
                "11 07 14 00",
                "11 07 16 00"
            ]
        );
        assert_eq!(
            pieces.iter().map(|(_, d)| d.len()).collect::<Vec<_>>(),
            [73, 129, 129, 129, 129]
        );
    }

    #[test]
    fn roundtrip_through_pieces() {
        let mut p = Patch::init();
        p.set_name("Round Trip").unwrap();
        p.tones[2].set("tone_switch", 1).unwrap();
        let base = user_patch(0).unwrap();
        assert_eq!(Patch::from_dt1(base, &p.to_dt1(base).unwrap()).unwrap(), p);
    }

    #[test]
    fn reassembles_128_plus_1_chunks_in_any_order() {
        let mut p = Patch::init();
        p.set_name("Chunked").unwrap();
        let base = user_patch(3).unwrap();
        let mut pieces = Vec::new();
        for (addr, data) in p.to_dt1(base).unwrap() {
            pieces.extend(as_pieces(
                split_dt1(addr, &data, MAX_DT1_DATA_TO_DEVICE).unwrap(),
            ));
        }
        assert_eq!(
            pieces.len(),
            1 + 4 * 2,
            "common is 73 bytes; each 129-byte tone splits in two"
        );
        pieces.reverse();
        assert_eq!(Patch::from_dt1(base, &pieces).unwrap(), p);
    }

    #[test]
    fn reports_missing_and_misplaced_data() {
        let base = user_patch(0).unwrap();
        let mut pieces = Patch::init().to_dt1(base).unwrap();
        pieces[3].1.truncate(100);
        assert!(matches!(
            Patch::from_dt1(base, &pieces),
            Err(PatchError::Incomplete { missing: 29, .. })
        ));

        let stray = vec![(base.checked_add_int(0x80).unwrap(), vec![0u8; 4])];
        assert!(matches!(
            Patch::from_dt1(base, &stray),
            Err(PatchError::OutsideBlock { .. })
        ));
        let below = vec![(Address::ZERO, vec![0u8; 4])];
        assert!(matches!(
            Patch::from_dt1(base, &below),
            Err(PatchError::OutsideBlock { .. })
        ));
    }

    #[test]
    fn identical_duplicates_are_fine_but_conflicting_overlaps_are_errors() {
        let base = user_patch(0).unwrap();
        let mut pieces = Patch::init().to_dt1(base).unwrap();

        // An identical second copy of Tone 1 is harmless.
        let mut same = pieces.clone();
        same.push(pieces[1].clone());
        assert!(Patch::from_dt1(base, &same).is_ok());

        // A partial copy that disagrees must not be merged in, whatever the order.
        let mut conflict = (pieces[1].0, pieces[1].1[..10].to_vec());
        conflict.1[5] ^= 0x01;
        let mut later = pieces.clone();
        later.push(conflict.clone());
        assert!(matches!(
            Patch::from_dt1(base, &later),
            Err(PatchError::Overlap { .. })
        ));
        pieces.insert(0, conflict);
        assert!(matches!(
            Patch::from_dt1(base, &pieces),
            Err(PatchError::Overlap { .. })
        ));
    }

    #[test]
    fn an_invalid_patch_is_loadable_for_inspection_but_refuses_to_produce_messages() {
        let base = user_patch(0).unwrap();
        let mut pieces = Patch::init().to_dt1(base).unwrap();
        let tempo = defs().0.param("patch_tempo").unwrap().offset;
        pieces[0].1[tempo] = 0x00; // high nibble 0, low nibble 7: tempo 7, below the minimum of 20
        pieces[0].1[tempo + 1] = 0x07;
        let loaded = Patch::from_dt1(base, &pieces).expect("loads");
        assert!(!loaded.validate().is_empty());
        assert!(matches!(
            loaded.to_dt1(base),
            Err(PatchError::Invalid { count: 1.. })
        ));
    }

    #[test]
    fn a_single_parameter_has_its_own_address_and_bytes() {
        use super::PatchBlock;
        let mut p = Patch::init();
        p.tones[1].set("cutoff_frequency", 99).unwrap();
        let base = user_patch(7).unwrap();
        let (addr, data) = p
            .param_dt1(base, PatchBlock::Tone(1), "cutoff_frequency")
            .unwrap();
        // User patch 8 is 11 07 00 00; Tone 2 is 12 00; Cutoff is offset 0x51.
        assert_eq!(addr.bytes(), [0x11, 0x07, 0x12, 0x51]);
        assert_eq!(data, vec![99]);
    }

    #[test]
    fn a_multi_byte_parameter_sends_all_its_bytes() {
        use super::PatchBlock;
        let mut p = Patch::init();
        p.common.set("patch_tempo", 120).unwrap();
        let (addr, data) = p
            .param_dt1(
                Address::from_lanes([3, 0, 0, 0]),
                PatchBlock::Common,
                "patch_tempo",
            )
            .unwrap();
        assert_eq!(addr.bytes(), [3, 0, 0, 44]);
        assert_eq!(data.len(), 2);
    }

    #[test]
    fn param_messages_refuse_unknown_blocks_params_and_bad_values() {
        use super::PatchBlock;
        let mut p = Patch::init();
        let base = Address::from_lanes([3, 0, 0, 0]);
        assert!(p
            .param_dt1(base, PatchBlock::Tone(9), "tone_level")
            .is_err());
        assert!(p.param_dt1(base, PatchBlock::Tone(0), "nope").is_err());
        // Force an out-of-range byte past the checked setter.
        p.tones[0] = BlockBuf::from_bytes(p.tones[0].def(), &{
            let mut raw = p.tones[0].bytes().to_vec();
            raw[101] = 0x7F; // tone_level is 0..=127, so use a narrower one
            raw[0] = 5; // tone_switch is 0..=1
            raw
        })
        .unwrap();
        assert!(p
            .param_dt1(base, PatchBlock::Tone(0), "tone_switch")
            .is_err());
    }

    #[test]
    fn block_labels_round_trip() {
        use super::PatchBlock;
        for label in ["common", "tone1", "tone2", "tone3", "tone4"] {
            assert_eq!(PatchBlock::parse(label).unwrap().label(), label);
        }
        assert!(PatchBlock::parse("tone5").is_none());
        assert_eq!(PatchBlock::Tone(3).index(), 4);
    }

    #[test]
    fn flat_blocks_round_trip_and_reject_wrong_lengths() {
        let mut p = Patch::init();
        p.set_name("Flat").unwrap();
        p.tones[2].set("resonance", 77).unwrap();
        let bytes = p.to_blocks();
        assert_eq!(bytes.len(), Patch::BLOCK_BYTES);
        assert_eq!(Patch::from_blocks(&bytes).unwrap(), p);
        assert!(Patch::from_blocks(&bytes[1..]).is_err());
        assert!(Patch::from_blocks(&[]).is_err());
        let mut long = bytes;
        long.push(0);
        assert!(Patch::from_blocks(&long).is_err());
    }
}
