//! `.syx` files: a sequence of DT1 messages, as produced by the XP-60 or a librarian.

use crate::doc::{DocKind, Document};
use crate::patch::{Patch, PatchError};
use thiserror::Error;
use xppatch_sysex::address::Address;
use xppatch_sysex::chunk::{split_dt1, ChunkError, MAX_DT1_DATA_TO_DEVICE};
use xppatch_sysex::memory::{
    user_patch, user_performance, user_rhythm, MemoryError, SYSTEM, TEMP_PATCH, USER_PATCH_COUNT,
    USER_PERFORMANCE_COUNT, USER_RHYTHM_COUNT,
};
use xppatch_sysex::message::{DeviceId, EncodeError, Message, ParseError};
use xppatch_sysex::stream::frames;

/// Largest `.syx` file accepted. A full User Patch bank is about 112 KB; this leaves generous room for whole-synth
/// backups while bounding memory on hostile input.
pub const MAX_FILE_BYTES: usize = 16 * 1024 * 1024;

#[derive(Debug, Error, PartialEq, Eq)]
#[non_exhaustive]
pub enum SyxError {
    #[error("message {index}: {source}")]
    Parse { index: usize, source: ParseError },
    #[error("message {index} is a data request; a file of stored data should contain only DT1")]
    UnexpectedRequest { index: usize },
    #[error("the file contains no SysEx messages")]
    Empty,
    #[error("file is {size} bytes, above the {limit} byte limit")]
    TooLarge { size: usize, limit: usize },
    #[error("user patch slot {0} is out of range")]
    SlotOutOfRange(u8),
    #[error("split size {0} exceeds the instrument's {MAX_DT1_DATA_TO_DEVICE}-byte limit")]
    SplitTooLarge(usize),
    #[error(transparent)]
    Patch(#[from] PatchError),
    #[error(transparent)]
    Memory(#[from] MemoryError),
    #[error(transparent)]
    Chunk(#[from] ChunkError),
    #[error(transparent)]
    Encode(#[from] EncodeError),
}

/// One DT1 message from a file.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Dt1Record {
    pub device: DeviceId,
    pub address: Address,
    pub data: Vec<u8>,
}

/// The messages of a `.syx` file, in order.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct SyxImage {
    pub records: Vec<Dt1Record>,
}

impl SyxImage {
    /// Parse a file. Strict about the messages (a malformed frame or any RQ1 is an error naming its position, and an
    /// empty file is an error) and tolerant only of non-SysEx bytes *between* frames. Parsing stops at the first error.
    pub fn parse(bytes: &[u8]) -> Result<SyxImage, SyxError> {
        if bytes.len() > MAX_FILE_BYTES {
            return Err(SyxError::TooLarge {
                size: bytes.len(),
                limit: MAX_FILE_BYTES,
            });
        }
        let mut records = Vec::new();
        for (index, frame) in frames(bytes).enumerate() {
            let parsed =
                Message::parse(&frame).map_err(|source| SyxError::Parse { index, source })?;
            match parsed.message {
                Message::Dt1 { address, data } => records.push(Dt1Record {
                    device: parsed.device,
                    address,
                    data,
                }),
                Message::Rq1 { .. } => return Err(SyxError::UnexpectedRequest { index }),
            }
        }
        if records.is_empty() {
            return Err(SyxError::Empty);
        }
        Ok(SyxImage { records })
    }

    /// Serialise back to the exact message bytes.
    pub fn to_bytes(&self) -> Result<Vec<u8>, EncodeError> {
        let mut out = Vec::new();
        for r in &self.records {
            out.extend(
                Message::Dt1 {
                    address: r.address,
                    data: r.data.clone(),
                }
                .encode(r.device)?,
            );
        }
        Ok(out)
    }

    /// The patch addressed to the temporary patch area (`03 00 00 00`), if the image holds one. A file saved from a
    /// single patch dump usually looks like this. Incomplete or conflicting data is an error.
    pub fn temp_patch(&self) -> Result<Option<Patch>, SyxError> {
        let pieces: Vec<(Address, Vec<u8>)> = self
            .records
            .iter()
            .filter(|r| {
                let l = r.address.bytes();
                l[0] == 0x03 && l[1] == 0x00
            })
            .map(|r| (r.address, r.data.clone()))
            .collect();
        if pieces.is_empty() {
            return Ok(None);
        }
        Ok(Some(Patch::from_dt1(TEMP_PATCH, &pieces)?))
    }

    /// The System settings in the image (messages addressed to `00 00 xx xx`), if it holds any. Incomplete or conflicting
    /// data is an error, as for patches.
    pub fn system_document(&self) -> Result<Option<Document>, SyxError> {
        let pieces: Vec<(Address, Vec<u8>)> = self
            .records
            .iter()
            .filter(|r| {
                let l = r.address.bytes();
                l[0] == 0x00 && l[1] == 0x00
            })
            .map(|r| (r.address, r.data.clone()))
            .collect();
        if pieces.is_empty() {
            return Ok(None);
        }
        Ok(Some(Document::from_dt1(DocKind::System, SYSTEM, &pieces)?))
    }

    /// Every complete User Patch in the image, as `(slot 0..=127, patch)`, ordered by slot. Messages for other
    /// regions are ignored. A User Patch with missing data, or a slot whose messages conflict with each other, is an
    /// error rather than being silently dropped or merged.
    pub fn user_patches(&self) -> Result<Vec<(u8, Patch)>, SyxError> {
        let mut slots: std::collections::BTreeMap<u8, Vec<(Address, Vec<u8>)>> = Default::default();
        for r in &self.records {
            let lanes = r.address.bytes();
            if lanes[0] == 0x11 && lanes[1] < USER_PATCH_COUNT {
                slots
                    .entry(lanes[1])
                    .or_default()
                    .push((r.address, r.data.clone()));
            }
        }
        let mut out = Vec::with_capacity(slots.len());
        for (slot, pieces) in slots {
            let base = user_patch(slot)?;
            out.push((slot, Patch::from_dt1(base, &pieces)?));
        }
        Ok(out)
    }

    /// Every complete User Performance (kind Performance, slots 0 to 31) or User Rhythm Set (kind Rhythm, slots 0 and one)
    /// in the image, as `(slot, document)` ordered by slot. Other regions are ignored; incomplete or conflicting data is an
    /// error, as for patches. System settings have no User slots and give an empty list.
    pub fn user_documents(&self, kind: DocKind) -> Result<Vec<(u8, Document)>, SyxError> {
        let slot_of = |lanes: [u8; 4]| -> Option<u8> {
            match kind {
                DocKind::Performance if lanes[0] == 0x10 && lanes[1] < USER_PERFORMANCE_COUNT => {
                    Some(lanes[1])
                }
                DocKind::Rhythm
                    if lanes[0] == 0x10 && (0x40..0x40 + USER_RHYTHM_COUNT).contains(&lanes[1]) =>
                {
                    Some(lanes[1] - 0x40)
                }
                _ => None,
            }
        };
        let mut slots: std::collections::BTreeMap<u8, Vec<(Address, Vec<u8>)>> = Default::default();
        for r in &self.records {
            if let Some(slot) = slot_of(r.address.bytes()) {
                slots
                    .entry(slot)
                    .or_default()
                    .push((r.address, r.data.clone()));
            }
        }
        let mut out = Vec::with_capacity(slots.len());
        for (slot, pieces) in slots {
            let base = match kind {
                DocKind::Performance => user_performance(slot)?,
                _ => user_rhythm(slot)?,
            };
            out.push((slot, Document::from_dt1(kind, base, &pieces)?));
        }
        Ok(out)
    }
}

/// Serialise User Performances or User Rhythm Sets as `.syx` messages, laid out like [`user_patch_bank_bytes`] lays out
/// patches (one message per block, or pieces of at most `split` data bytes, never above 128). Kinds without User slots
/// are refused.
pub fn user_document_bytes(
    kind: DocKind,
    documents: &[(u8, Document)],
    device: DeviceId,
    split: Option<usize>,
) -> Result<Vec<u8>, SyxError> {
    if let Some(n) = split {
        if n > MAX_DT1_DATA_TO_DEVICE {
            return Err(SyxError::SplitTooLarge(n));
        }
    }
    let mut out = Vec::new();
    for (slot, doc) in documents {
        if doc.kind() != kind {
            return Err(SyxError::SlotOutOfRange(*slot));
        }
        let base = match kind {
            DocKind::Performance => user_performance(*slot),
            DocKind::Rhythm => user_rhythm(*slot),
            DocKind::System => return Err(SyxError::SlotOutOfRange(*slot)),
        }
        .map_err(|_| SyxError::SlotOutOfRange(*slot))?;
        for (address, data) in doc.to_dt1(base)? {
            let messages = match split {
                Some(n) => split_dt1(address, &data, n)?,
                None => vec![Message::Dt1 { address, data }],
            };
            for m in messages {
                out.extend(m.encode(device)?);
            }
        }
    }
    Ok(out)
}

/// Serialise the System settings as `.syx` messages (pieces of at most `split` data bytes, never above 128). Other kinds
/// are refused.
pub fn system_document_bytes(
    doc: &Document,
    device: DeviceId,
    split: Option<usize>,
) -> Result<Vec<u8>, SyxError> {
    if let Some(n) = split {
        if n > MAX_DT1_DATA_TO_DEVICE {
            return Err(SyxError::SplitTooLarge(n));
        }
    }
    if doc.kind() != DocKind::System {
        return Err(SyxError::SlotOutOfRange(0));
    }
    let mut out = Vec::new();
    for (address, data) in doc.to_dt1(SYSTEM)? {
        let messages = match split {
            Some(n) => split_dt1(address, &data, n)?,
            None => vec![Message::Dt1 { address, data }],
        };
        for m in messages {
            out.extend(m.encode(device)?);
        }
    }
    Ok(out)
}

/// Serialise patches as a User Patch bank `.syx`. With `split = None` each block is one message (the way the XP-60
/// transmits them); with `Some(n)` blocks are split into pieces of at most `n` data bytes (use 128 to send to the
/// instrument; more than 128 is refused). Patches holding invalid values are refused.
pub fn user_patch_bank_bytes(
    patches: &[(u8, Patch)],
    device: DeviceId,
    split: Option<usize>,
) -> Result<Vec<u8>, SyxError> {
    if let Some(n) = split {
        if n > MAX_DT1_DATA_TO_DEVICE {
            return Err(SyxError::SplitTooLarge(n));
        }
    }
    let mut out = Vec::new();
    for (slot, patch) in patches {
        let base = user_patch(*slot).map_err(|_| SyxError::SlotOutOfRange(*slot))?;
        for (address, data) in patch.to_dt1(base)? {
            let messages = match split {
                Some(n) => split_dt1(address, &data, n)?,
                None => vec![Message::Dt1 { address, data }],
            };
            for m in messages {
                out.extend(m.encode(device)?);
            }
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn system_settings_roundtrip_through_a_file() {
        let mut doc = Document::init(DocKind::System);
        doc.block_mut(0).unwrap().set("master_tune", 37).unwrap();
        let bytes = system_document_bytes(&doc, DeviceId::DEFAULT, Some(128)).unwrap();
        let back = SyxImage::parse(&bytes).unwrap().system_document().unwrap();
        assert_eq!(back, Some(doc));
        // a patch bank holds no System settings, and other kinds are refused
        let bank = user_patch_bank_bytes(&[(0, Patch::init())], DeviceId::DEFAULT, None).unwrap();
        assert_eq!(
            SyxImage::parse(&bank).unwrap().system_document().unwrap(),
            None
        );
        let perf = Document::init(DocKind::Performance);
        assert!(system_document_bytes(&perf, DeviceId::DEFAULT, None).is_err());
    }

    #[test]
    fn bank_bytes_roundtrip_through_the_image() {
        let mut a = Patch::init();
        a.set_name("Alpha").unwrap();
        let mut b = Patch::init();
        b.set_name("Beta").unwrap();
        let bytes =
            user_patch_bank_bytes(&[(0, a.clone()), (5, b.clone())], DeviceId::DEFAULT, None)
                .unwrap();
        let image = SyxImage::parse(&bytes).unwrap();
        assert_eq!(image.records.len(), 10);
        assert_eq!(image.to_bytes().unwrap(), bytes);
        let patches = image.user_patches().unwrap();
        assert_eq!(patches.len(), 2);
        assert_eq!((patches[0].0, &patches[0].1), (0, &a));
        assert_eq!((patches[1].0, &patches[1].1), (5, &b));
    }

    #[test]
    fn split_export_is_128_byte_pieces_and_reads_back_the_same() {
        let mut p = Patch::init();
        p.set_name("Split").unwrap();
        let bytes = user_patch_bank_bytes(&[(9, p.clone())], DeviceId::DEFAULT, Some(128)).unwrap();
        let image = SyxImage::parse(&bytes).unwrap();
        assert_eq!(image.records.len(), 1 + 4 * 2);
        assert!(image.records.iter().all(|r| r.data.len() <= 128));
        assert_eq!(image.user_patches().unwrap()[0].1, p);
    }

    #[test]
    fn malformed_and_request_messages_are_errors_with_positions() {
        let good = user_patch_bank_bytes(&[(0, Patch::init())], DeviceId::DEFAULT, None).unwrap();
        let mut broken = good.clone();
        let flip = broken.len() - 3;
        broken[flip] ^= 0x01;
        assert!(matches!(
            SyxImage::parse(&broken),
            Err(SyxError::Parse { index: 4, .. })
        ));

        let rq = Message::Rq1 {
            address: Address::ZERO,
            size: Address::from_compact(1).unwrap(),
        }
        .encode(DeviceId::DEFAULT)
        .unwrap();
        assert_eq!(
            SyxImage::parse(&rq),
            Err(SyxError::UnexpectedRequest { index: 0 })
        );
    }

    #[test]
    fn empty_and_oversized_inputs_are_rejected() {
        assert_eq!(SyxImage::parse(&[]), Err(SyxError::Empty));
        assert_eq!(SyxImage::parse(&[0x90, 0x3C, 0x40]), Err(SyxError::Empty));
        let huge = vec![0u8; MAX_FILE_BYTES + 1];
        assert!(matches!(
            SyxImage::parse(&huge),
            Err(SyxError::TooLarge { .. })
        ));
    }

    #[test]
    fn a_pathological_run_of_frame_starts_fails_fast() {
        // N bytes of F0 would be N truncated frames; parsing must stop at the first one.
        let hostile = vec![0xF0u8; 1_000_000];
        assert!(matches!(
            SyxImage::parse(&hostile),
            Err(SyxError::Parse { index: 0, .. })
        ));
    }

    #[test]
    fn an_incomplete_patch_is_an_error_not_a_silent_drop() {
        let bytes = user_patch_bank_bytes(&[(0, Patch::init())], DeviceId::DEFAULT, None).unwrap();
        let mut image = SyxImage::parse(&bytes).unwrap();
        image.records.pop();
        assert!(matches!(
            image.user_patches(),
            Err(SyxError::Patch(PatchError::Incomplete { .. }))
        ));
    }

    #[test]
    fn a_slot_sent_twice_with_different_data_is_a_conflict() {
        let mut a = Patch::init();
        a.set_name("First").unwrap();
        let mut b = a.clone();
        b.set_name("Second").unwrap();
        let mut bytes = user_patch_bank_bytes(&[(4, a)], DeviceId::DEFAULT, None).unwrap();
        bytes.extend(user_patch_bank_bytes(&[(4, b)], DeviceId::DEFAULT, None).unwrap());
        let image = SyxImage::parse(&bytes).unwrap();
        assert!(matches!(
            image.user_patches(),
            Err(SyxError::Patch(PatchError::Overlap { .. }))
        ));
    }

    #[test]
    fn slot_and_split_limits_are_enforced() {
        assert_eq!(
            user_patch_bank_bytes(&[(128, Patch::init())], DeviceId::DEFAULT, None),
            Err(SyxError::SlotOutOfRange(128))
        );
        assert_eq!(
            user_patch_bank_bytes(&[(0, Patch::init())], DeviceId::DEFAULT, Some(129)),
            Err(SyxError::SplitTooLarge(129))
        );
    }

    const BANK_FILE: &[u8] = include_bytes!("../../../tests/golden/xp60-user-bank.syx");

    #[test]
    fn the_golden_file_holds_whole_user_performances_and_rhythm_sets() {
        let image = SyxImage::parse(BANK_FILE).unwrap();
        let perfs = image.user_documents(DocKind::Performance).unwrap();
        let rhythm = image.user_documents(DocKind::Rhythm).unwrap();
        assert_eq!(
            perfs.len(),
            usize::from(USER_PERFORMANCE_COUNT),
            "32 User Performances"
        );
        assert_eq!(
            rhythm.len(),
            usize::from(USER_RHYTHM_COUNT),
            "2 User Rhythm Sets"
        );
        assert!(perfs.windows(2).all(|w| w[0].0 < w[1].0), "ordered by slot");
        assert!(perfs.iter().all(|(_, d)| d.kind() == DocKind::Performance));
        assert!(image.user_documents(DocKind::System).unwrap().is_empty());
    }

    #[test]
    fn user_documents_survive_being_written_and_read_again_in_any_cut() {
        let image = SyxImage::parse(BANK_FILE).unwrap();
        for kind in [DocKind::Performance, DocKind::Rhythm] {
            let docs = image.user_documents(kind).unwrap();
            for split in [None, Some(128), Some(32)] {
                let bytes = user_document_bytes(kind, &docs, DeviceId::DEFAULT, split).unwrap();
                let again = SyxImage::parse(&bytes)
                    .unwrap()
                    .user_documents(kind)
                    .unwrap();
                assert_eq!(again.len(), docs.len(), "{kind:?} split {split:?}");
                for ((sa, a), (sb, b)) in docs.iter().zip(&again) {
                    assert_eq!(sa, sb);
                    assert_eq!(a.blocks().len(), b.blocks().len());
                    assert!(a
                        .blocks()
                        .iter()
                        .zip(b.blocks())
                        .all(|(x, y)| x.bytes() == y.bytes()));
                }
            }
        }
    }

    #[test]
    fn writing_user_documents_refuses_bad_slots_kinds_and_chunks() {
        let doc = Document::init(DocKind::Performance);
        let one = [(32u8, doc.clone())];
        assert_eq!(
            user_document_bytes(DocKind::Performance, &one, DeviceId::DEFAULT, None),
            Err(SyxError::SlotOutOfRange(32))
        );
        assert_eq!(
            user_document_bytes(
                DocKind::Rhythm,
                &[(0, doc.clone())],
                DeviceId::DEFAULT,
                None
            ),
            Err(SyxError::SlotOutOfRange(0))
        );
        assert_eq!(
            user_document_bytes(
                DocKind::System,
                &[(0, Document::init(DocKind::System))],
                DeviceId::DEFAULT,
                None
            ),
            Err(SyxError::SlotOutOfRange(0))
        );
        assert_eq!(
            user_document_bytes(
                DocKind::Performance,
                &[(0, doc)],
                DeviceId::DEFAULT,
                Some(129)
            ),
            Err(SyxError::SplitTooLarge(129))
        );
    }

    #[test]
    fn an_incomplete_user_performance_is_an_error_not_a_short_list() {
        let image = SyxImage::parse(BANK_FILE).unwrap();
        let mut broken = image.clone();
        // Drop one record of User Performance 1: the slot is then incomplete.
        let at = broken
            .records
            .iter()
            .position(|r| r.address.bytes()[0] == 0x10 && r.address.bytes()[1] == 0)
            .unwrap();
        broken.records.remove(at);
        assert!(matches!(
            broken.user_documents(DocKind::Performance),
            Err(SyxError::Patch(_))
        ));
    }
}
