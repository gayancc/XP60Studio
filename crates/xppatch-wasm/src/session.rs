//! The patch-editing logic behind the wasm bindings, with no wasm types so it is tested natively.

use serde_json::{json, Value};
use std::fmt;
use xppatch_core::{
    BlockBuf, Display, DocKind, Document, ParamDef, ParameterMap, Patch, PatchBlock, SyxImage,
};
use xppatch_sysex::address::Address;
use xppatch_sysex::chunk::{split_dt1, MAX_DT1_DATA_TO_DEVICE};
use xppatch_sysex::memory::TEMP_PATCH;
use xppatch_sysex::message::DeviceId;

/// Total bytes of a patch's five blocks (Common 73 + 4 x Tone 129).
pub const PATCH_BYTES: usize = Patch::BLOCK_BYTES;

#[derive(Debug, PartialEq, Eq)]
pub struct EditError(pub String);

impl fmt::Display for EditError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

impl<E: std::error::Error> From<E> for EditError {
    fn from(e: E) -> Self {
        EditError(e.to_string())
    }
}

pub type Result<T> = std::result::Result<T, EditError>;

fn block_of(label: &str) -> Result<PatchBlock> {
    PatchBlock::parse(label).ok_or_else(|| EditError(format!("unknown patch block '{label}'")))
}

fn def_of<'a>(buf: &'a BlockBuf, id: &str) -> Result<&'a ParamDef> {
    buf.def()
        .param(id)
        .ok_or_else(|| EditError(format!("unknown parameter '{id}'")))
}

/// One patch being edited.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Session {
    patch: Patch,
}

impl Default for Session {
    fn default() -> Self {
        Session::new()
    }
}

impl Session {
    pub fn new() -> Session {
        Session {
            patch: Patch::init(),
        }
    }

    /// Concatenated blocks: Common (73) then Tone 1 to 4 (129 each).
    pub fn from_blocks(bytes: &[u8]) -> Result<Session> {
        Ok(Session {
            patch: Patch::from_blocks(bytes)?,
        })
    }

    pub fn to_blocks(&self) -> Vec<u8> {
        self.patch.to_blocks()
    }

    /// Load patch `slot` (0 to 127) from the bytes of a `.syx` bank file.
    pub fn from_syx_slot(bytes: &[u8], slot: u8) -> Result<Session> {
        let (image, skipped) = SyxImage::parse_lenient(bytes)?;
        let (patches, _) = image.user_patches_lenient(skipped.len())?;
        let found = patches
            .into_iter()
            .find(|(s, _)| *s == slot)
            .ok_or_else(|| EditError(format!("slot {slot} is not in this file")))?;
        Ok(Session { patch: found.1 })
    }

    /// `(slot, name)` for every patch in a `.syx` bank file.
    pub fn syx_names(bytes: &[u8]) -> Result<Vec<(u8, String)>> {
        let (image, skipped) = SyxImage::parse_lenient(bytes)?;
        Ok(image
            .user_patches_lenient(skipped.len())?
            .0
            .into_iter()
            .map(|(s, p)| (s, p.name()))
            .collect())
    }

    /// What reading a patch bank file left out: how many messages were skipped as damaged or not XP-60 data, and which
    /// patch parts were set to initial values because their messages were missing. Empty for a clean file.
    pub fn syx_report(bytes: &[u8]) -> Result<(usize, Vec<(u8, &'static str)>)> {
        let (image, skipped) = SyxImage::parse_lenient(bytes)?;
        let (_, filled) = image.user_patches_lenient(skipped.len())?;
        Ok((
            skipped.len(),
            filled.into_iter().map(|f| (f.slot, f.part)).collect(),
        ))
    }

    pub fn name(&self) -> String {
        self.patch.name()
    }

    pub fn set_name(&mut self, name: &str) -> Result<()> {
        Ok(self.patch.set_name(name)?)
    }

    pub fn get(&self, block: &str, id: &str) -> Result<i32> {
        let buf = self.buf(block)?;
        Ok(buf.get(id)?)
    }

    pub fn set(&mut self, block: &str, id: &str, value: i32) -> Result<()> {
        let b = block_of(block)?;
        let buf = self
            .patch
            .block_mut(b)
            .ok_or_else(|| EditError("no such block".into()))?;
        Ok(buf.set(id, value)?)
    }

    pub fn format(&self, block: &str, id: &str) -> Result<String> {
        let value = self.get(block, id)?;
        self.format_value(block, id, value)
    }

    pub fn format_value(&self, block: &str, id: &str, value: i32) -> Result<String> {
        let buf = self.buf(block)?;
        let def = def_of(buf, id)?;
        Ok(Display::resolve(def)?.format(value))
    }

    /// Out-of-range or malformed values as `[{block, param, problem}]` JSON.
    pub fn issues_json(&self) -> String {
        let list: Vec<Value> = self
            .patch
            .validate()
            .into_iter()
            .map(|(block, issue)| json!({ "block": block, "problem": format!("{issue:?}") }))
            .collect();
        Value::Array(list).to_string()
    }

    /// One DT1 frame (`F0 .. F7`) that writes this parameter into the instrument's **temporary** patch.
    pub fn param_frame(&self, device: u8, block: &str, id: &str) -> Result<Vec<u8>> {
        let (address, data) = self.patch.param_dt1(TEMP_PATCH, block_of(block)?, id)?;
        Ok(
            xppatch_sysex::message::Message::Dt1 { address, data }
                .encode(DeviceId::new(device)?)?,
        )
    }

    /// Every DT1 frame that writes the whole patch into the temporary patch, concatenated, each at most 128 data bytes.
    pub fn patch_frames(&self, device: u8) -> Result<Vec<u8>> {
        let id = DeviceId::new(device)?;
        let mut out = Vec::new();
        for (address, data) in self.patch.to_dt1(TEMP_PATCH)? {
            for m in split_dt1(address, &data, MAX_DT1_DATA_TO_DEVICE)? {
                out.extend(m.encode(id)?);
            }
        }
        Ok(out)
    }

    fn buf(&self, block: &str) -> Result<&BlockBuf> {
        self.patch
            .block(block_of(block)?)
            .ok_or_else(|| EditError("no such block".into()))
    }
}

/// Address of a parameter inside the temporary patch, for the UI's send queue to coalesce edits by location.
pub fn param_address(block: &str, id: &str) -> Result<u32> {
    static TEMPLATE: std::sync::OnceLock<Session> = std::sync::OnceLock::new();
    let session = TEMPLATE.get_or_init(Session::new);
    let b = block_of(block)?;
    let (a, _) = session
        .patch
        .param_dt1(Address::from_lanes([0, 0, 0, 0]), b, id)?;
    Ok(a.compact())
}

fn kind_of(kind: &str) -> Result<DocKind> {
    match kind {
        "performance" => Ok(DocKind::Performance),
        "rhythm" => Ok(DocKind::Rhythm),
        "system" => Ok(DocKind::System),
        other => Err(EditError(format!("unknown document kind '{other}'"))),
    }
}

/// A Performance or Rhythm Set being edited. Blocks are `common`, `part1` to `part16`, or `note35` to `note98`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DocSession {
    doc: Document,
}

impl DocSession {
    pub fn new(kind: &str) -> Result<DocSession> {
        Ok(DocSession {
            doc: Document::init(kind_of(kind)?),
        })
    }

    fn kind(&self) -> DocKind {
        self.doc.kind()
    }

    fn index(&self, block: &str) -> Result<usize> {
        self.kind()
            .parse_label(block)
            .ok_or_else(|| EditError(format!("unknown block '{block}'")))
    }

    fn buf(&self, block: &str) -> Result<&BlockBuf> {
        self.doc
            .block(self.index(block)?)
            .ok_or_else(|| EditError("no such block".into()))
    }

    /// Total bytes of all blocks, in transmission order.
    pub fn byte_len(kind: &str) -> Result<usize> {
        Ok(Document::init(kind_of(kind)?)
            .blocks()
            .iter()
            .map(|b| b.bytes().len())
            .sum())
    }

    pub fn from_blocks(kind: &str, bytes: &[u8]) -> Result<DocSession> {
        let k = kind_of(kind)?;
        let template = Document::init(k);
        let expected: usize = template.blocks().iter().map(|b| b.bytes().len()).sum();
        if bytes.len() != expected {
            return Err(EditError(format!(
                "expected {expected} bytes of {kind} data, got {}",
                bytes.len()
            )));
        }
        let mut doc = template;
        let mut at = 0;
        for i in 0..doc.blocks().len() {
            let def = doc.blocks()[i].def();
            let next = BlockBuf::from_bytes(def, &bytes[at..at + def.size])?;
            at += def.size;
            *doc.block_mut(i)
                .ok_or_else(|| EditError("no such block".into()))? = next;
        }
        Ok(DocSession { doc })
    }

    /// Load User Performance or User Rhythm Set `slot` from the bytes of a `.syx` file (a whole-memory backup holds 32 and 2).
    pub fn from_syx_slot(kind: &str, bytes: &[u8], slot: u8) -> Result<DocSession> {
        let found = SyxImage::parse(bytes)?
            .user_documents(kind_of(kind)?)?
            .into_iter()
            .find(|(s, _)| *s == slot)
            .ok_or_else(|| EditError(format!("slot {slot} is not in this file")))?;
        Ok(DocSession { doc: found.1 })
    }

    /// `(slot, name)` for every User Performance or User Rhythm Set in a `.syx` file.
    pub fn syx_names(kind: &str, bytes: &[u8]) -> Result<Vec<(u8, String)>> {
        Ok(SyxImage::parse(bytes)?
            .user_documents(kind_of(kind)?)?
            .into_iter()
            .map(|(s, d)| (s, d.name()))
            .collect())
    }

    /// The document as a `.syx` file: a Performance or Rhythm Set in User slot 1 (the first of a bank), or the System
    /// settings, in pieces of at most 128 data bytes so the file can be sent to the instrument as it is.
    pub fn to_syx(&self, device: u8) -> Result<Vec<u8>> {
        let id = DeviceId::new(device)?;
        let kind = self.kind();
        Ok(match kind {
            DocKind::System => xppatch_core::system_document_bytes(&self.doc, id, Some(128))?,
            _ => xppatch_core::user_document_bytes(kind, &[(0, self.doc.clone())], id, Some(128))?,
        })
    }

    pub fn to_blocks(&self) -> Vec<u8> {
        self.doc
            .blocks()
            .iter()
            .flat_map(|b| b.bytes().iter().copied())
            .collect()
    }

    pub fn name(&self) -> String {
        self.doc.name()
    }

    pub fn set_name(&mut self, name: &str) -> Result<()> {
        Ok(self.doc.set_name(name)?)
    }

    pub fn get(&self, block: &str, id: &str) -> Result<i32> {
        Ok(self.buf(block)?.get(id)?)
    }

    pub fn set(&mut self, block: &str, id: &str, value: i32) -> Result<()> {
        let i = self.index(block)?;
        let buf = self
            .doc
            .block_mut(i)
            .ok_or_else(|| EditError("no such block".into()))?;
        Ok(buf.set(id, value)?)
    }

    pub fn format_value(&self, block: &str, id: &str, value: i32) -> Result<String> {
        let buf = self.buf(block)?;
        Ok(Display::resolve(def_of(buf, id)?)?.format(value))
    }

    pub fn format(&self, block: &str, id: &str) -> Result<String> {
        let v = self.get(block, id)?;
        self.format_value(block, id, v)
    }

    pub fn issues_json(&self) -> String {
        let list: Vec<Value> = self
            .doc
            .validate()
            .into_iter()
            .map(|(block, issue)| json!({ "block": block, "problem": format!("{issue:?}") }))
            .collect();
        Value::Array(list).to_string()
    }

    /// One DT1 frame writing this parameter into the instrument's temporary copy of this kind.
    pub fn param_frame(&self, device: u8, block: &str, id: &str) -> Result<Vec<u8>> {
        self.check_live(self.index(block)?)?;
        let (address, data) =
            self.doc
                .param_dt1(self.kind().temp_base(), self.index(block)?, id)?;
        Ok(
            xppatch_sysex::message::Message::Dt1 { address, data }
                .encode(DeviceId::new(device)?)?,
        )
    }

    /// Live frames are refused for System (permanent memory) and for the Rhythm Setup's Common block, whose temporary
    /// address is not yet confirmed on hardware (ledger H-045).
    fn check_live(&self, index: usize) -> Result<()> {
        if !self.kind().is_live_editable() {
            return Err(EditError(
                "System settings are in permanent memory and are never sent live".into(),
            ));
        }
        if self.kind() == DocKind::Rhythm && index == 0 {
            return Err(EditError(
                "the rhythm set name is not sent live until its address is confirmed on the instrument".into(),
            ));
        }
        Ok(())
    }

    /// Every frame writing the whole document into its temporary copy, concatenated.
    pub fn doc_frames(&self, device: u8) -> Result<Vec<u8>> {
        self.check_live(1)?;
        let id = DeviceId::new(device)?;
        let mut out = Vec::new();
        // The Rhythm Setup's Common block is held back (see `check_live`).
        let skip = usize::from(self.kind() == DocKind::Rhythm);
        for (address, data) in self
            .doc
            .to_dt1(self.kind().temp_base())?
            .into_iter()
            .skip(skip)
        {
            for m in split_dt1(address, &data, MAX_DT1_DATA_TO_DEVICE)? {
                out.extend(m.encode(id)?);
            }
        }
        Ok(out)
    }
}

/// Address of a parameter inside the temporary copy of a Performance or Rhythm Set.
pub fn doc_param_address(kind: &str, block: &str, id: &str) -> Result<u32> {
    let doc = Document::init(kind_of(kind)?);
    let index = doc
        .kind()
        .parse_label(block)
        .ok_or_else(|| EditError(format!("unknown block '{block}'")))?;
    let (a, _) = doc.param_dt1(Address::from_lanes([0, 0, 0, 0]), index, id)?;
    Ok(a.compact())
}

fn display_json(d: &Display) -> Value {
    match d {
        Display::Raw => json!({ "kind": "raw" }),
        Display::Character => json!({ "kind": "character" }),
        Display::Numeric {
            first,
            step,
            min,
            pad,
            signed,
        } => {
            json!({ "kind": "numeric", "first": first, "step": step, "min": min, "pad": pad, "signed": signed })
        }
        Display::Pan { centre } => json!({ "kind": "pan", "centre": centre }),
        Display::Note => json!({ "kind": "note" }),
        Display::Options { min, labels } => {
            json!({ "kind": "options", "min": min, "labels": labels })
        }
    }
}

/// The editable catalogue for the patch blocks, as JSON: every parameter with range, encoding and display.
pub fn catalog_json() -> Result<String> {
    let map = ParameterMap::embedded();
    let mut blocks = Vec::new();
    for b in map.blocks().iter().filter(|b| {
        matches!(
            b.family.as_str(),
            "patch" | "performance" | "rhythm" | "system"
        )
    }) {
        let mut params = Vec::new();
        for p in &b.params {
            params.push(json!({
                "id": p.id, "name": p.name, "offset": p.offset, "bytes": p.bytes,
                "min": p.min, "max": p.max, "display": display_json(&Display::resolve(p)?),
            }));
        }
        blocks.push(json!({ "id": b.id, "name": b.name, "size": b.size, "params": params }));
    }
    Ok(json!({ "blocks": blocks }).to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    const BANK: &[u8] = include_bytes!("../../../tests/golden/xp60-user-bank.syx");

    #[test]
    fn a_new_session_round_trips_its_blocks() {
        let s = Session::new();
        let bytes = s.to_blocks();
        assert_eq!(bytes.len(), PATCH_BYTES);
        assert_eq!(Session::from_blocks(&bytes).unwrap(), s);
        assert!(Session::from_blocks(&bytes[1..]).is_err());
    }

    #[test]
    fn values_are_read_set_and_formatted() {
        let mut s = Session::new();
        s.set("tone2", "cutoff_frequency", 99).unwrap();
        assert_eq!(s.get("tone2", "cutoff_frequency").unwrap(), 99);
        assert_eq!(s.get("tone1", "cutoff_frequency").unwrap(), 0);
        s.set("tone3", "lfo1_waveform", 2).unwrap();
        assert_eq!(s.format("tone3", "lfo1_waveform").unwrap(), "SAW");
        s.set("common", "patch_pan", 64).unwrap();
        assert_eq!(s.format("common", "patch_pan").unwrap(), "0");
        assert_eq!(s.format_value("common", "patch_pan", 0).unwrap(), "L64");
    }

    #[test]
    fn bad_input_is_an_error_not_a_panic() {
        let mut s = Session::new();
        assert!(s.set("tone9", "tone_level", 1).is_err());
        assert!(s.set("tone1", "nope", 1).is_err());
        assert!(s.set("tone1", "tone_level", 500).is_err());
        assert!(s.get("common", "nope").is_err());
        assert!(s.param_frame(0x10, "tone1", "nope").is_err());
        assert!(s.param_frame(0x99, "tone1", "tone_level").is_err());
    }

    #[test]
    fn a_parameter_frame_targets_the_temporary_patch() {
        let mut s = Session::new();
        s.set("tone4", "tone_level", 100).unwrap();
        let f = s.param_frame(0x10, "tone4", "tone_level").unwrap();
        // F0 41 10 6A 12 | 03 00 16 65 | 64 | checksum F7  (Tone 4 at 16 00, Tone Level offset 0x65)
        assert_eq!(&f[..5], &[0xF0, 0x41, 0x10, 0x6A, 0x12]);
        assert_eq!(&f[5..9], &[0x03, 0x00, 0x16, 0x65]);
        assert_eq!(f[9], 100);
        assert_eq!(*f.last().unwrap(), 0xF7);
        assert_eq!(
            param_address("tone4", "tone_level").unwrap(),
            param_address("tone4", "tone_level").unwrap()
        );
    }

    #[test]
    fn whole_patch_frames_are_split_and_parse_back_to_the_same_patch() {
        use xppatch_sysex::stream::frames;
        let s = Session::from_syx_slot(BANK, 3).unwrap();
        let bytes = s.patch_frames(0x10).unwrap();
        let mut pieces = Vec::new();
        for frame in frames(&bytes) {
            let p = xppatch_sysex::message::Message::parse(&frame).unwrap();
            assert_eq!(p.device.display_number(), 17);
            if let xppatch_sysex::message::Message::Dt1 { address, data } = p.message {
                assert!(data.len() <= 128);
                pieces.push((address, data));
            }
        }
        assert_eq!(pieces.len(), 1 + 4 * 2);
        let back = Patch::from_dt1(TEMP_PATCH, &pieces).unwrap();
        assert_eq!(back, s.patch);
    }

    #[test]
    fn a_bank_file_lists_and_loads_its_patches() {
        let names = Session::syx_names(BANK).unwrap();
        assert_eq!(names.len(), 128);
        let s = Session::from_syx_slot(BANK, 0).unwrap();
        assert_eq!(s.name(), names[0].1);
        assert!(Session::from_syx_slot(BANK, 200).is_err());
        // a file cut short holds no whole patch: an error or an empty list, never a half patch
        assert!(Session::syx_names(&BANK[..100]).map_or(true, |n| n.is_empty()));
    }

    #[test]
    fn the_catalogue_covers_every_patch_parameter_with_a_display() {
        let v: Value = serde_json::from_str(&catalog_json().unwrap()).unwrap();
        let blocks = v["blocks"].as_array().unwrap();
        assert_eq!(blocks.len(), 8);
        let counts: Vec<usize> = blocks
            .iter()
            .map(|b| b["params"].as_array().unwrap().len())
            .collect();
        assert_eq!(counts, vec![72, 128, 65, 23, 12, 57, 95, 12]);
        for b in blocks {
            for p in b["params"].as_array().unwrap() {
                assert!(p["display"]["kind"].is_string(), "{p}");
            }
        }
    }

    #[test]
    fn the_name_can_be_changed() {
        let mut s = Session::new();
        s.set_name("My Pad").unwrap();
        assert_eq!(s.name(), "My Pad");
        assert!(s
            .set_name("a name that is far too long for twelve")
            .is_err());
    }

    #[test]
    fn a_document_exports_as_a_file_that_opens_again() {
        let bank = include_bytes!("../../../tests/golden/xp60-user-bank.syx");
        let perf = DocSession::from_syx_slot("performance", bank, 5).unwrap();
        let file = perf.to_syx(0x10).unwrap();
        assert_eq!(
            DocSession::syx_names("performance", &file).unwrap(),
            vec![(0, perf.name())]
        );
        assert_eq!(
            DocSession::from_syx_slot("performance", &file, 0)
                .unwrap()
                .to_blocks(),
            perf.to_blocks()
        );
        let rhythm = DocSession::from_syx_slot("rhythm", bank, 1).unwrap();
        let file = rhythm.to_syx(0x10).unwrap();
        assert_eq!(DocSession::syx_names("rhythm", &file).unwrap().len(), 1);
        let mut system = DocSession::new("system").unwrap();
        system.set("common", "master_tune", 30).unwrap();
        let file = system.to_syx(0x10).unwrap();
        let back = xppatch_core::SyxImage::parse(&file)
            .unwrap()
            .system_document()
            .unwrap();
        assert_eq!(back.map(|d| d.to_block_bytes()), Some(system.to_blocks()));
        assert!(perf.to_syx(0x01).is_err(), "device IDs run 0x10 to 0x1F");
    }

    #[test]
    fn whole_memory_files_give_their_performances_and_rhythm_sets() {
        let bank = include_bytes!("../../../tests/golden/xp60-user-bank.syx");
        let perfs = DocSession::syx_names("performance", bank).unwrap();
        assert_eq!(perfs.len(), 32);
        assert_eq!(perfs[0].0, 0);
        let rhythm = DocSession::syx_names("rhythm", bank).unwrap();
        assert_eq!(rhythm.len(), 2);
        let p = DocSession::from_syx_slot("performance", bank, 5).unwrap();
        assert_eq!(p.name(), perfs[5].1);
        assert!(DocSession::from_syx_slot("performance", bank, 40).is_err());
        assert!(DocSession::from_syx_slot("rhythm", bank, 1).is_ok());
        assert!(DocSession::syx_names("bogus", bank).is_err());
    }

    #[test]
    fn a_performance_session_edits_parts_and_sends_to_the_temporary_performance() {
        let mut s = DocSession::new("performance").unwrap();
        s.set("part3", "part_level", 88).unwrap();
        assert_eq!(s.get("part3", "part_level").unwrap(), 88);
        assert_eq!(s.get("part4", "part_level").unwrap(), 0);
        s.set_name("My Perf").unwrap();
        assert_eq!(s.name(), "My Perf");
        let f = s.param_frame(0x10, "part3", "part_level").unwrap();
        // 01 00 | part 3 = 0x12 | offset 6
        assert_eq!(&f[5..9], &[0x01, 0x00, 0x12, 0x06]);
        assert_eq!(f[9], 88);
        assert!(s.set("part17", "part_level", 1).is_err());
        assert!(s.set("note40", "tone_level", 1).is_err());
        assert!(s.set("part1", "part_level", 500).is_err());
        assert_eq!(s.format("part3", "midi_channel").unwrap(), "1");
    }

    #[test]
    fn a_rhythm_session_addresses_notes_by_key_number() {
        let mut s = DocSession::new("rhythm").unwrap();
        s.set("note44", "tone_level", 90).unwrap();
        let f = s.param_frame(0x10, "note44", "tone_level").unwrap();
        assert_eq!(&f[5..9], &[0x02, 0x09, 0x2C, 0x29]);
        assert_eq!(
            doc_param_address("rhythm", "note44", "tone_level").unwrap(),
            (0x2C << 7) | 0x29
        );
        assert!(s.set("note99", "tone_level", 1).is_err());
        assert!(DocSession::new("patch").is_err());
    }

    #[test]
    fn document_blocks_round_trip_and_frames_parse_back() {
        use xppatch_sysex::stream::frames;
        for kind in ["performance", "rhythm"] {
            let mut s = DocSession::new(kind).unwrap();
            s.set_name("Round").unwrap();
            let bytes = s.to_blocks();
            assert_eq!(bytes.len(), DocSession::byte_len(kind).unwrap());
            assert_eq!(DocSession::from_blocks(kind, &bytes).unwrap(), s);
            assert!(DocSession::from_blocks(kind, &bytes[1..]).is_err());
            let all = s.doc_frames(0x10).unwrap();
            let count = frames(&all).count();
            assert!(count + usize::from(kind == "rhythm") >= s.doc.blocks().len());
            for f in frames(&all) {
                let p = xppatch_sysex::message::Message::parse(&f).unwrap();
                if let xppatch_sysex::message::Message::Dt1 { data, .. } = p.message {
                    assert!(data.len() <= 128);
                }
            }
        }
    }

    #[test]
    fn system_never_makes_live_frames_and_rhythm_common_is_held_back() {
        let sys = DocSession::new("system").unwrap();
        assert!(sys.param_frame(0x10, "common", "master_tune").is_err());
        assert!(sys.doc_frames(0x10).is_err());
        let r = DocSession::new("rhythm").unwrap();
        assert!(r.param_frame(0x10, "common", "rhythm_name_1").is_err());
        assert!(r.param_frame(0x10, "note40", "tone_level").is_ok());
        use xppatch_sysex::stream::frames;
        let all = r.doc_frames(0x10).unwrap();
        for f in frames(&all) {
            let m = xppatch_sysex::message::Message::parse(&f).unwrap();
            if let xppatch_sysex::message::Message::Dt1 { address, .. } = m.message {
                assert!(address.bytes()[2] >= 0x23, "{address}");
            }
        }
    }
}
