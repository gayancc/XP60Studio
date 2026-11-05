//! Performance and Rhythm Set documents: a Common block plus a run of repeated blocks (16 Parts, or 64 Notes).
//!
//! Like [`crate::Patch`], a document converts to and from DT1 pieces. The layout of each kind comes from the memory map:
//! Performance Parts sit at `10 00` to `1F 00` inside the performance; Rhythm Notes at `23 00` to `62 00` inside the
//! set, where the third address byte is the MIDI note number (35 to 98).

use crate::block::{BlockBuf, CodecError, Issue};
use crate::map::ParameterMap;
use crate::patch::PatchError;
use crate::schema::BlockDef;
use xppatch_sysex::address::Address;
use xppatch_sysex::memory::{SYSTEM, TEMP_PERFORMANCE, USER_PERFORMANCE_COUNT, USER_RHYTHM_COUNT};

/// Which kind of document.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DocKind {
    Performance,
    Rhythm,
    /// System Common plus 17 Scale Tune blocks (16 Parts and Patch mode). Permanent memory: read and edit locally; it is
    /// only ever written through the write ceremony.
    System,
}

/// Parts in a Performance.
pub const PARTS: usize = 16;
/// First key (MIDI note) a Rhythm Set defines, and how many keys it has.
pub const RHYTHM_FIRST_NOTE: u8 = 0x23;
pub const RHYTHM_NOTES: usize = 64;
/// Scale Tune blocks in the System area: one per Part plus one for Patch mode.
pub const SCALE_TUNES: usize = 17;

impl DocKind {
    fn common_id(self) -> &'static str {
        match self {
            DocKind::Performance => "performance_common",
            DocKind::Rhythm => "rhythm_common",
            DocKind::System => "system_common",
        }
    }

    fn item_id(self) -> &'static str {
        match self {
            DocKind::Performance => "performance_part",
            DocKind::Rhythm => "rhythm_note",
            DocKind::System => "scale_tune",
        }
    }

    /// Number of repeated blocks after Common.
    pub fn items(self) -> usize {
        match self {
            DocKind::Performance => PARTS,
            DocKind::Rhythm => RHYTHM_NOTES,
            DocKind::System => SCALE_TUNES,
        }
    }

    /// Compact offset of repeated block `i` (0-based) inside the document.
    fn item_offset(self, i: usize) -> u32 {
        let first = match self {
            DocKind::Performance => 0x10,
            DocKind::Rhythm => u32::from(RHYTHM_FIRST_NOTE),
            DocKind::System => 0x10,
        };
        // The third address lane steps by one per block, so the compact offset steps by 128.
        (first + i as u32) << 7
    }

    /// The label used for repeated block `i`: `part1` to `part16`, or `note35` to `note98` (the key number).
    pub fn item_label(self, i: usize) -> String {
        match self {
            DocKind::Performance => format!("part{}", i + 1),
            DocKind::Rhythm => format!("note{}", usize::from(RHYTHM_FIRST_NOTE) + i),
            DocKind::System => format!("tune{}", i + 1),
        }
    }

    /// Parse `common`, `partN` or `noteK` into a block index (0 is Common).
    pub fn parse_label(self, label: &str) -> Option<usize> {
        if label == "common" {
            return Some(0);
        }
        let index = self.parse_label_loose(label)?;
        // Only the canonical spelling counts: no `part+1`, no `part01`.
        (self.item_label(index - 1) == label).then_some(index)
    }

    fn parse_label_loose(self, label: &str) -> Option<usize> {
        match self {
            DocKind::Performance => {
                let n: usize = label.strip_prefix("part")?.parse().ok()?;
                (1..=PARTS).contains(&n).then_some(n)
            }
            DocKind::Rhythm => {
                let k: usize = label.strip_prefix("note")?.parse().ok()?;
                let first = usize::from(RHYTHM_FIRST_NOTE);
                (first..first + RHYTHM_NOTES)
                    .contains(&k)
                    .then(|| k - first + 1)
            }
            DocKind::System => {
                let n: usize = label.strip_prefix("tune")?.parse().ok()?;
                (1..=SCALE_TUNES).contains(&n).then_some(n)
            }
        }
    }

    /// Where the temporary copy of this kind lives (what live edits write to).
    pub fn temp_base(self) -> Address {
        match self {
            DocKind::Performance => TEMP_PERFORMANCE,
            // The Rhythm Setup is Part 10's temporary area.
            DocKind::Rhythm => Address::from_lanes([0x02, 0x09, 0x00, 0x00]),
            // There is no temporary System area: this is where it lives. Nothing here is ever live-edited.
            DocKind::System => SYSTEM,
        }
    }

    /// True for kinds whose temporary copy can be edited live. System settings live in permanent memory.
    pub fn is_live_editable(self) -> bool {
        !matches!(self, DocKind::System)
    }

    /// Number of User slots of this kind.
    pub fn user_slots(self) -> u8 {
        match self {
            DocKind::Performance => USER_PERFORMANCE_COUNT,
            DocKind::Rhythm => USER_RHYTHM_COUNT,
            DocKind::System => 0,
        }
    }

    fn defs(self) -> (&'static BlockDef, &'static BlockDef) {
        let map = ParameterMap::embedded();
        (
            map.block(self.common_id())
                .expect("embedded map has the common block"),
            map.block(self.item_id())
                .expect("embedded map has the repeated block"),
        )
    }

    /// `(compact offset, size in bytes)` of every block, in transmission order.
    pub fn block_spans(self) -> Vec<(u32, usize)> {
        self.layout()
            .into_iter()
            .map(|(o, d)| (o, d.size))
            .collect()
    }

    /// `(compact offset, definition)` of every block in transmission order.
    fn layout(self) -> Vec<(u32, &'static BlockDef)> {
        let (c, item) = self.defs();
        let mut out = vec![(0, c)];
        out.extend((0..self.items()).map(|i| (self.item_offset(i), item)));
        out
    }
}

/// A Performance or Rhythm Set.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Document {
    kind: DocKind,
    /// Common first, then the repeated blocks.
    blocks: Vec<BlockBuf>,
}

impl Document {
    /// Every parameter at its minimum.
    pub fn init(kind: DocKind) -> Document {
        Document {
            kind,
            blocks: kind
                .layout()
                .into_iter()
                .map(|(_, d)| BlockBuf::init(d))
                .collect(),
        }
    }

    pub fn kind(&self) -> DocKind {
        self.kind
    }

    pub fn blocks(&self) -> &[BlockBuf] {
        &self.blocks
    }

    pub fn block(&self, index: usize) -> Option<&BlockBuf> {
        self.blocks.get(index)
    }

    pub fn block_mut(&mut self, index: usize) -> Option<&mut BlockBuf> {
        self.blocks.get_mut(index)
    }

    pub fn name(&self) -> String {
        self.blocks[0].text()
    }

    pub fn set_name(&mut self, name: &str) -> Result<(), CodecError> {
        self.blocks[0].set_text(name)
    }

    /// Range problems across all blocks, as `(block label, issue)`.
    pub fn validate(&self) -> Vec<(String, Issue)> {
        let mut out = Vec::new();
        for (i, b) in self.blocks.iter().enumerate() {
            let label = if i == 0 {
                "common".to_owned()
            } else {
                self.kind.item_label(i - 1)
            };
            out.extend(b.validate().into_iter().map(|issue| (label.clone(), issue)));
        }
        out
    }

    /// One `(address, data)` piece per block for a document based at `base`. Refuses invalid values.
    pub fn to_dt1(&self, base: Address) -> Result<Vec<(Address, Vec<u8>)>, PatchError> {
        let issues = self.validate().len();
        if issues > 0 {
            return Err(PatchError::Invalid { count: issues });
        }
        let mut out = Vec::with_capacity(self.blocks.len());
        for (block, (off, _)) in self.blocks.iter().zip(self.kind.layout()) {
            out.push((
                base.checked_add(Address::from_compact(off)?)?,
                block.bytes().to_vec(),
            ));
        }
        Ok(out)
    }

    /// The single DT1 for one parameter of block `index` (0 is Common), for a document based at `base`.
    pub fn param_dt1(
        &self,
        base: Address,
        index: usize,
        id: &str,
    ) -> Result<(Address, Vec<u8>), PatchError> {
        let buf = self.block(index).ok_or(PatchError::UnknownBlock)?;
        let def = buf
            .def()
            .param(id)
            .ok_or_else(|| CodecError::UnknownParam(id.to_owned()))?;
        let value = buf.get_param(def)?;
        if value < def.min || value > def.max {
            return Err(PatchError::Invalid { count: 1 });
        }
        let (off, _) = *self
            .kind
            .layout()
            .get(index)
            .ok_or(PatchError::UnknownBlock)?;
        let start = off
            .checked_add(
                u32::try_from(def.offset)
                    .map_err(|_| xppatch_sysex::address::AddressError::Overflow)?,
            )
            .ok_or(xppatch_sysex::address::AddressError::Overflow)?;
        let data = buf.bytes()[def.offset..def.offset + def.bytes].to_vec();
        Ok((base.checked_add(Address::from_compact(start)?)?, data))
    }

    /// Rebuild a document from DT1 pieces in any order and chunking. Every byte of every block must be present;
    /// overlaps are allowed only if identical. Values are not range-checked here (use [`Document::validate`]).
    pub fn from_dt1(
        kind: DocKind,
        base: Address,
        pieces: &[(Address, Vec<u8>)],
    ) -> Result<Document, PatchError> {
        let blocks = assemble(base, &kind.layout(), pieces)?;
        Ok(Document { kind, blocks })
    }
}

/// Reassemble blocks from DT1 pieces according to `layout` (compact offset and definition per block).
pub(crate) fn assemble(
    base: Address,
    layout: &[(u32, &'static BlockDef)],
    pieces: &[(Address, Vec<u8>)],
) -> Result<Vec<BlockBuf>, PatchError> {
    let mut data: Vec<Vec<u8>> = layout.iter().map(|(_, d)| vec![0u8; d.size]).collect();
    let mut seen: Vec<Vec<bool>> = layout.iter().map(|(_, d)| vec![false; d.size]).collect();

    for (address, bytes) in pieces {
        let outside = || PatchError::OutsideBlock { address: *address };
        let rel = address.checked_sub(base).map_err(|_| outside())?.compact();
        let len = u32::try_from(bytes.len()).map_err(|_| outside())?;
        let end = rel.checked_add(len).ok_or_else(outside)?;
        let slot = layout
            .iter()
            .position(|(o, d)| rel >= *o && end <= *o + d.size as u32)
            .ok_or_else(outside)?;
        let start = (rel - layout[slot].0) as usize;
        for (k, &b) in bytes.iter().enumerate() {
            let i = start + k;
            if seen[slot][i] && data[slot][i] != b {
                return Err(PatchError::Overlap { address: *address });
            }
            data[slot][i] = b;
            seen[slot][i] = true;
        }
    }

    let mut blocks = Vec::with_capacity(layout.len());
    for (i, (_, def)) in layout.iter().enumerate() {
        let missing = seen[i].iter().filter(|&&s| !s).count();
        if missing > 0 {
            return Err(PatchError::Incomplete {
                block: def.id.clone(),
                missing,
            });
        }
        blocks.push(BlockBuf::from_bytes(def, &data[i])?);
    }
    Ok(blocks)
}

#[cfg(test)]
mod tests {
    use super::*;
    use xppatch_sysex::chunk::{split_dt1, MAX_DT1_DATA_TO_DEVICE};
    use xppatch_sysex::memory::user_performance;
    use xppatch_sysex::message::Message;

    fn pieces(msgs: Vec<Message>) -> Vec<(Address, Vec<u8>)> {
        msgs.into_iter()
            .map(|m| match m {
                Message::Dt1 { address, data } => (address, data),
                Message::Rq1 { .. } => unreachable!(),
            })
            .collect()
    }

    #[test]
    fn block_counts_and_sizes_match_the_map() {
        let p = Document::init(DocKind::Performance);
        assert_eq!(p.blocks().len(), 17);
        assert_eq!(p.blocks()[0].bytes().len(), 66);
        assert_eq!(p.blocks()[1].bytes().len(), 25);
        let r = Document::init(DocKind::Rhythm);
        assert_eq!(r.blocks().len(), 65);
        assert_eq!(r.blocks()[0].bytes().len(), 12);
        assert_eq!(r.blocks()[1].bytes().len(), 58);
    }

    #[test]
    fn layouts_follow_the_memory_map() {
        let base = user_performance(3).unwrap();
        let p = Document::init(DocKind::Performance).to_dt1(base).unwrap();
        assert_eq!(p[0].0.bytes(), [0x10, 0x03, 0x00, 0x00]);
        assert_eq!(p[1].0.bytes(), [0x10, 0x03, 0x10, 0x00]);
        assert_eq!(p[16].0.bytes(), [0x10, 0x03, 0x1F, 0x00]);
        let r = Document::init(DocKind::Rhythm)
            .to_dt1(Address::from_lanes([0x10, 0x40, 0, 0]))
            .unwrap();
        assert_eq!(r[1].0.bytes(), [0x10, 0x40, 0x23, 0x00]);
        assert_eq!(r[64].0.bytes(), [0x10, 0x40, 0x62, 0x00]);
    }

    #[test]
    fn labels_round_trip_and_reject_bad_ones() {
        let p = DocKind::Performance;
        assert_eq!(p.parse_label("common"), Some(0));
        assert_eq!(p.parse_label("part1"), Some(1));
        assert_eq!(p.parse_label("part16"), Some(16));
        assert_eq!(p.parse_label("part17"), None);
        assert_eq!(p.parse_label("part0"), None);
        assert_eq!(p.parse_label("note40"), None);
        let r = DocKind::Rhythm;
        assert_eq!(r.parse_label("note35"), Some(1));
        assert_eq!(r.parse_label("note98"), Some(64));
        assert_eq!(r.parse_label("note99"), None);
        assert_eq!(r.parse_label("note34"), None);
        for i in 0..PARTS {
            assert_eq!(p.parse_label(&p.item_label(i)), Some(i + 1));
        }
        for i in 0..RHYTHM_NOTES {
            assert_eq!(r.parse_label(&r.item_label(i)), Some(i + 1));
        }
    }

    #[test]
    fn a_modified_document_survives_a_split_round_trip() {
        for kind in [DocKind::Performance, DocKind::Rhythm] {
            let mut d = Document::init(kind);
            d.set_name("Round Trip").unwrap();
            d.block_mut(1).unwrap().set("tone_switch", 1).ok();
            if kind == DocKind::Performance {
                d.block_mut(3).unwrap().set("part_level", 99).unwrap();
            } else {
                d.block_mut(5).unwrap().set("tone_level", 77).unwrap();
            }
            let base = kind.temp_base();
            let mut msgs = Vec::new();
            for (a, data) in d.to_dt1(base).unwrap() {
                msgs.extend(split_dt1(a, &data, MAX_DT1_DATA_TO_DEVICE).unwrap());
            }
            let mut all = pieces(msgs);
            all.reverse(); // any order
            let back = Document::from_dt1(kind, base, &all).unwrap();
            assert_eq!(back, d);
            assert_eq!(back.name(), "Round Trip");
        }
    }

    #[test]
    fn missing_data_and_conflicts_are_errors() {
        let d = Document::init(DocKind::Performance);
        let base = TEMP_PERFORMANCE;
        let mut all = d.to_dt1(base).unwrap();
        let last = all.pop().unwrap();
        assert!(matches!(
            Document::from_dt1(DocKind::Performance, base, &all),
            Err(PatchError::Incomplete { .. })
        ));
        all.push(last.clone());
        let mut dup = last;
        dup.1[0] ^= 1;
        all.push(dup);
        assert!(matches!(
            Document::from_dt1(DocKind::Performance, base, &all),
            Err(PatchError::Overlap { .. })
        ));
        let stray = vec![(Address::from_lanes([1, 0, 0x30, 0]), vec![0u8])];
        assert!(matches!(
            Document::from_dt1(DocKind::Performance, base, &stray),
            Err(PatchError::OutsideBlock { .. })
        ));
    }

    #[test]
    fn a_parameter_message_has_the_right_address() {
        let mut d = Document::init(DocKind::Rhythm);
        d.block_mut(10).unwrap().set("tone_level", 90).unwrap(); // key 44
        let (a, data) = d
            .param_dt1(DocKind::Rhythm.temp_base(), 10, "tone_level")
            .unwrap();
        // 02 09 | key 44 = 0x2C | offset 41 = 0x29
        assert_eq!(a.bytes(), [0x02, 0x09, 0x2C, 0x29]);
        assert_eq!(data, vec![90]);
        assert!(d
            .param_dt1(DocKind::Rhythm.temp_base(), 99, "tone_level")
            .is_err());
        assert!(d.param_dt1(DocKind::Rhythm.temp_base(), 1, "nope").is_err());
    }

    #[test]
    fn invalid_values_are_refused_on_the_send_path() {
        let mut d = Document::init(DocKind::Performance);
        let raw = {
            let mut bytes = d.block(1).unwrap().bytes().to_vec();
            bytes[1] = 0x7F; // midi_channel is 0..=15
            bytes
        };
        *d.block_mut(1).unwrap() = BlockBuf::from_bytes(d.block(1).unwrap().def(), &raw).unwrap();
        assert!(matches!(
            d.to_dt1(TEMP_PERFORMANCE),
            Err(PatchError::Invalid { .. })
        ));
        assert!(d.param_dt1(TEMP_PERFORMANCE, 1, "midi_channel").is_err());
    }

    #[test]
    fn the_system_document_has_common_and_seventeen_scale_tunes() {
        let d = Document::init(DocKind::System);
        assert_eq!(d.blocks().len(), 18);
        assert_eq!(d.blocks()[0].bytes().len(), 96);
        assert_eq!(d.blocks()[1].bytes().len(), 12);
        let pieces = d.to_dt1(DocKind::System.temp_base()).unwrap();
        assert_eq!(pieces[0].0.bytes(), [0, 0, 0, 0]);
        assert_eq!(pieces[1].0.bytes(), [0, 0, 0x10, 0]);
        assert_eq!(pieces[17].0.bytes(), [0, 0, 0x20, 0]);
        assert_eq!(DocKind::System.parse_label("tune1"), Some(1));
        assert_eq!(DocKind::System.parse_label("tune17"), Some(17));
        assert_eq!(DocKind::System.parse_label("tune18"), None);
        assert_eq!(DocKind::System.user_slots(), 0);
        let back =
            Document::from_dt1(DocKind::System, DocKind::System.temp_base(), &pieces).unwrap();
        assert_eq!(back, d);
    }

    #[test]
    fn only_canonical_labels_name_a_block() {
        for bad in ["part+1", "part01", "part 1", "Part1", "part1 ", "part-0"] {
            assert_eq!(DocKind::Performance.parse_label(bad), None, "{bad}");
        }
        assert_eq!(DocKind::Rhythm.parse_label("note+40"), None);
        assert_eq!(DocKind::System.parse_label("tune007"), None);
        assert!(DocKind::Performance.is_live_editable());
        assert!(!DocKind::System.is_live_editable());
    }
}
