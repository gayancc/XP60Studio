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
use xppatch_sysex::message::{DeviceId, EncodeError, Message, ParseError, Parsed};
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
    #[error("none of the {skipped} messages in this file is XP-60 data ({reason})")]
    NoXp60Data { skipped: usize, reason: String },
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

/// A message the lenient reader left out of a file, and why.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SkippedMessage {
    pub index: usize,
    pub reason: String,
}

/// A part of a patch whose messages were missing or damaged in a file, so the part is at its initial values.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FilledPart {
    /// User patch slot, 0 to 127.
    pub slot: u8,
    /// "common" or "tone 1" to "tone 4".
    pub part: &'static str,
}

/// The patches of a file with the parts that had to be filled in.
pub type LenientPatches = (Vec<(u8, Patch)>, Vec<FilledPart>);

const PATCH_PARTS: [&str; 5] = ["common", "tone 1", "tone 2", "tone 3", "tone 4"];
/// The model ID byte of the XP-60/80 (the first thing a stray file for another instrument gets wrong).
const XP60_MODEL: u8 = 0x6A;

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

    /// Parse a file the way the instrument reads one: a message that is damaged or is not XP-60 data is skipped (and
    /// reported) instead of failing the whole file. Only a file with no usable message at all is an error. Use this for
    /// files a person brings in; [`SyxImage::parse`] stays strict for anything written back to the instrument.
    pub fn parse_lenient(bytes: &[u8]) -> Result<(SyxImage, Vec<SkippedMessage>), SyxError> {
        if bytes.len() > MAX_FILE_BYTES {
            return Err(SyxError::TooLarge {
                size: bytes.len(),
                limit: MAX_FILE_BYTES,
            });
        }
        let mut records = Vec::new();
        let mut skipped: Vec<SkippedMessage> = Vec::new();
        let mut first_reason = String::new();
        for (index, frame) in frames(bytes).enumerate() {
            let reason = match Message::parse(&frame) {
                Ok(Parsed {
                    device,
                    message: Message::Dt1 { address, data },
                }) => {
                    records.push(Dt1Record {
                        device,
                        address,
                        data,
                    });
                    continue;
                }
                Ok(_) => "a data request, not stored data".to_owned(),
                Err(e) => match (frame.get(1), frame.get(3)) {
                    (Some(&0x41), Some(&m)) if m != XP60_MODEL => {
                        format!("for Roland model {m:#04x}, not the XP-60 ({XP60_MODEL:#04x})")
                    }
                    _ => e.to_string(),
                },
            };
            if skipped.is_empty() {
                first_reason.clone_from(&reason);
            }
            skipped.push(SkippedMessage { index, reason });
        }
        if records.is_empty() {
            return if skipped.is_empty() {
                Err(SyxError::Empty)
            } else {
                Err(SyxError::NoXp60Data {
                    skipped: skipped.len(),
                    reason: first_reason,
                })
            };
        }
        Ok((SyxImage { records }, skipped))
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

    /// Like [`SyxImage::user_patches`], for a file that may have lost messages: a patch whose common block or a Tone has
    /// missing data gets that part at its initial values (and is listed), instead of the whole file failing. A part that
    /// arrived complete is never touched. `damaged` is how many messages the reader skipped: a part is only filled in
    /// when a skipped message can account for it, so a file that was simply cut short is still an error, as before.
    pub fn user_patches_lenient(&self, damaged: usize) -> Result<LenientPatches, SyxError> {
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
        let mut filled = Vec::new();
        for (slot, mut pieces) in slots {
            let base = user_patch(slot)?;
            for (i, (addr, data)) in Patch::init().to_dt1(base)?.into_iter().enumerate() {
                let lo = addr.compact();
                let hi = lo + u32::try_from(data.len()).unwrap_or(u32::MAX);
                let inside = |a: &Address| (lo..hi).contains(&a.compact());
                let got: usize = pieces
                    .iter()
                    .filter(|(a, _)| inside(a))
                    .map(|(_, d)| d.len())
                    .sum();
                if got < data.len() {
                    pieces.retain(|(a, _)| !inside(a));
                    pieces.push((addr, data));
                    filled.push(FilledPart {
                        slot,
                        part: PATCH_PARTS[i],
                    });
                }
            }
            out.push((slot, Patch::from_dt1(base, &pieces)?));
        }
        if filled.len() > damaged {
            // More parts are missing than damaged messages explain: the file is incomplete, not damaged. Say so.
            return self.user_patches().map(|p| (p, Vec::new()));
        }
        Ok((out, filled))
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

    /// A copy of the golden bank with one patch-common message's manufacturer byte and one Tone message's command byte
    /// damaged, as a transfer error can leave them.
    fn damaged_bank() -> Vec<u8> {
        let mut b = BANK_FILE.to_vec();
        let starts: Vec<usize> = b
            .iter()
            .enumerate()
            .filter(|(_, &x)| x == 0xF0)
            .map(|(i, _)| i)
            .collect();
        let at = |slot: u8, block: u8| {
            *starts
                .iter()
                .find(|&&i| b[i + 5] == 0x11 && b[i + 6] == slot && b[i + 7] == block)
                .expect("block present")
        };
        let common = at(3, 0x00);
        let tone = at(9, 0x10);
        b[common + 1] = 0x01; // manufacturer 0x41 -> 0x01
        b[tone + 4] = 0x02; // command 0x12 -> 0x02
        b
    }

    #[test]
    fn lenient_reading_skips_damaged_messages_where_strict_reading_fails() {
        let bytes = damaged_bank();
        assert!(SyxImage::parse(&bytes).is_err());
        let (image, skipped) = SyxImage::parse_lenient(&bytes).unwrap();
        assert_eq!(skipped.len(), 2);
        let (patches, filled) = image.user_patches_lenient(skipped.len()).unwrap();
        assert_eq!(patches.len(), 128, "every patch is still loaded");
        assert_eq!(
            filled,
            vec![
                FilledPart {
                    slot: 3,
                    part: "common"
                },
                FilledPart {
                    slot: 9,
                    part: "tone 1"
                },
            ]
        );
        let good = SyxImage::parse(BANK_FILE).unwrap().user_patches().unwrap();
        for (slot, p) in &patches {
            if *slot != 3 && *slot != 9 {
                assert_eq!(p, &good.iter().find(|(s, _)| s == slot).unwrap().1);
            }
        }
        let b = good.iter().find(|(s, _)| *s == 9).unwrap();
        let l = patches.iter().find(|(s, _)| *s == 9).unwrap();
        assert_eq!(
            l.1.common, b.1.common,
            "the common block of a patch with a damaged Tone is untouched"
        );
    }

    #[test]
    fn a_file_that_was_cut_short_is_still_an_error() {
        // cut cleanly between messages: slot 5 has its common block and Tone 1 but not Tones 2 to 4, and nothing was damaged
        let cut = BANK_FILE
            .iter()
            .enumerate()
            .filter(|(_, &x)| x == 0xF0)
            .map(|(i, _)| i)
            .find(|&i| {
                BANK_FILE[i + 5] == 0x11 && BANK_FILE[i + 6] == 5 && BANK_FILE[i + 7] == 0x12
            })
            .expect("slot 5 tone 2 present");
        let (image, skipped) = SyxImage::parse_lenient(&BANK_FILE[..cut]).unwrap();
        assert!(skipped.is_empty());
        assert!(image.user_patches_lenient(skipped.len()).is_err());
    }

    #[test]
    fn a_file_for_another_model_is_named_as_such() {
        // a DT1-shaped message for model 0x00 with device 0x09, as an older Roland instrument sends it
        let mut f = vec![0xF0, 0x41, 0x09, 0x00, 0x0D, 0x01, 0x02, 0x03, 0x04];
        f.push(0x76); // Roland checksum of the four data bytes: 128 - (1 + 2 + 3 + 4)
        f.push(0xF7);
        match SyxImage::parse_lenient(&f) {
            Err(SyxError::NoXp60Data { skipped: 1, reason }) => {
                assert!(reason.contains("not the XP-60"), "{reason}");
            }
            other => panic!("{other:?}"),
        }
        assert_eq!(SyxImage::parse_lenient(&[]), Err(SyxError::Empty));
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
