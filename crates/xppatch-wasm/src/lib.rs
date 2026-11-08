//! WebAssembly bindings for the editor UI. All logic is in [`session`]; this file only adapts types for JavaScript.

pub mod session;

use session::{DocSession, EditError, Session};
use wasm_bindgen::prelude::*;

fn js(e: EditError) -> JsError {
    JsError::new(&e.0)
}

/// The editable catalogue (JSON): every patch parameter with range, encoding and how to display it.
#[wasm_bindgen(js_name = catalogJson)]
pub fn catalog_json() -> Result<String, JsError> {
    session::catalog_json().map_err(js)
}

/// Compact address of a parameter inside the temporary patch (used to coalesce live edits by location).
#[wasm_bindgen(js_name = paramAddress)]
pub fn param_address(block: &str, id: &str) -> Result<u32, JsError> {
    session::param_address(block, id).map_err(js)
}

/// Names of the patches in a `.syx` bank file, as JSON `[[slot, name], ...]`.
#[wasm_bindgen(js_name = syxNames)]
pub fn syx_names(bytes: &[u8]) -> Result<String, JsError> {
    let names = Session::syx_names(bytes).map_err(js)?;
    serde_json::to_string(&names).map_err(|e| JsError::new(&e.to_string()))
}

/// What reading a patch bank file left out, as JSON `{"skipped": n, "filled": [[slot, "common" | "tone 1" ..], ...]}`: damaged or
/// non-XP-60 messages skipped, and patch parts set to initial values because their messages were missing.
#[wasm_bindgen(js_name = syxReport)]
pub fn syx_report(bytes: &[u8]) -> Result<String, JsError> {
    let (skipped, filled) = Session::syx_report(bytes).map_err(js)?;
    serde_json::to_string(&serde_json::json!({ "skipped": skipped, "filled": filled }))
        .map_err(|e| JsError::new(&e.to_string()))
}

/// Names of the Performances (`"performance"`) or Rhythm Sets (`"rhythm"`) in a `.syx` file, as JSON `[[slot, name], ...]`.
#[wasm_bindgen(js_name = docSyxNames)]
pub fn doc_syx_names(kind: &str, bytes: &[u8]) -> Result<String, JsError> {
    let names = DocSession::syx_names(kind, bytes).map_err(js)?;
    serde_json::to_string(&names).map_err(|e| JsError::new(&e.to_string()))
}

/// The SysEx messages of a `.syx` file as a Standard MIDI File (one event per message, spaced for the instrument).
#[wasm_bindgen(js_name = syxToSmf)]
pub fn syx_to_smf(bytes: &[u8]) -> Result<Vec<u8>, JsError> {
    xppatch_core::smf::syx_to_smf(bytes).map_err(|e| JsError::new(&e.to_string()))
}

/// The SysEx messages found in a Standard MIDI File, as `.syx` bytes (everything else in the file is skipped).
#[wasm_bindgen(js_name = smfToSyx)]
pub fn smf_to_syx(bytes: &[u8]) -> Result<Vec<u8>, JsError> {
    xppatch_core::smf::smf_to_syx(bytes).map_err(|e| JsError::new(&e.to_string()))
}

/// Cut the DT1 frames of a whole-patch send into frames of at most `max_data` data bytes (never above the instrument's 128).
#[wasm_bindgen(js_name = resplitDt1)]
pub fn resplit_dt1(bytes: &[u8], max_data: usize) -> Result<Vec<u8>, JsError> {
    xppatch_sysex::chunk::resplit_dt1(bytes, max_data).map_err(|e| JsError::new(&e.to_string()))
}

/// One patch being edited. Values are read and written by block (`common`, `tone1` to `tone4`) and parameter id.
#[wasm_bindgen]
pub struct PatchDoc {
    inner: Session,
}

impl Default for PatchDoc {
    fn default() -> Self {
        PatchDoc::new()
    }
}

#[wasm_bindgen]
impl PatchDoc {
    /// A patch with every parameter at its minimum.
    #[wasm_bindgen(constructor)]
    pub fn new() -> PatchDoc {
        PatchDoc {
            inner: Session::new(),
        }
    }

    /// The XP-60's own Patch Initialize / DEFAULT result (separate from the codec minimum template).
    #[wasm_bindgen(js_name = xp60Default)]
    pub fn xp60_default() -> PatchDoc {
        PatchDoc {
            inner: Session::from_blocks(&xppatch_core::Patch::xp60_default().to_blocks())
                .expect("embedded XP-60 Initialize patch is valid"),
        }
    }

    #[wasm_bindgen(js_name = fromBlocks)]
    pub fn from_blocks(bytes: &[u8]) -> Result<PatchDoc, JsError> {
        Ok(PatchDoc {
            inner: Session::from_blocks(bytes).map_err(js)?,
        })
    }

    #[wasm_bindgen(js_name = fromSyxSlot)]
    pub fn from_syx_slot(bytes: &[u8], slot: u8) -> Result<PatchDoc, JsError> {
        Ok(PatchDoc {
            inner: Session::from_syx_slot(bytes, slot).map_err(js)?,
        })
    }

    /// Snapshot of all five blocks (Common then Tone 1 to 4), for undo history.
    #[wasm_bindgen(js_name = toBlocks)]
    pub fn to_blocks(&self) -> Vec<u8> {
        self.inner.to_blocks()
    }

    pub fn name(&self) -> String {
        self.inner.name()
    }

    #[wasm_bindgen(js_name = setName)]
    pub fn set_name(&mut self, name: &str) -> Result<(), JsError> {
        self.inner.set_name(name).map_err(js)
    }

    pub fn get(&self, block: &str, id: &str) -> Result<i32, JsError> {
        self.inner.get(block, id).map_err(js)
    }

    pub fn set(&mut self, block: &str, id: &str, value: i32) -> Result<(), JsError> {
        self.inner.set(block, id, value).map_err(js)
    }

    /// The parameter's current value as the musician sees it ("SAW", "+12", "L20").
    pub fn format(&self, block: &str, id: &str) -> Result<String, JsError> {
        self.inner.format(block, id).map_err(js)
    }

    #[wasm_bindgen(js_name = formatValue)]
    pub fn format_value(&self, block: &str, id: &str, value: i32) -> Result<String, JsError> {
        self.inner.format_value(block, id, value).map_err(js)
    }

    #[wasm_bindgen(js_name = issuesJson)]
    pub fn issues_json(&self) -> String {
        self.inner.issues_json()
    }

    /// One DT1 frame writing this parameter into the instrument's temporary patch.
    #[wasm_bindgen(js_name = paramFrame)]
    pub fn param_frame(&self, device: u8, block: &str, id: &str) -> Result<Vec<u8>, JsError> {
        self.inner.param_frame(device, block, id).map_err(js)
    }

    /// Every frame writing the whole patch into the temporary patch (concatenated; split on `F7`).
    #[wasm_bindgen(js_name = patchFrames)]
    pub fn patch_frames(&self, device: u8) -> Result<Vec<u8>, JsError> {
        self.inner.patch_frames(device).map_err(js)
    }
}

/// Compact address of a parameter inside the temporary copy of a Performance (`"performance"`) or Rhythm Set (`"rhythm"`).
#[wasm_bindgen(js_name = docParamAddress)]
pub fn doc_param_address(kind: &str, block: &str, id: &str) -> Result<u32, JsError> {
    session::doc_param_address(kind, block, id).map_err(js)
}

/// A Performance or Rhythm Set being edited. Blocks are `common`, `part1` to `part16`, or `note35` to `note98`.
#[wasm_bindgen]
pub struct Doc {
    inner: DocSession,
}

#[wasm_bindgen]
impl Doc {
    /// `kind` is `"performance"` or `"rhythm"`. Every parameter starts at its minimum.
    #[wasm_bindgen(constructor)]
    pub fn new(kind: &str) -> Result<Doc, JsError> {
        Ok(Doc {
            inner: DocSession::new(kind).map_err(js)?,
        })
    }

    #[wasm_bindgen(js_name = fromBlocks)]
    pub fn from_blocks(kind: &str, bytes: &[u8]) -> Result<Doc, JsError> {
        Ok(Doc {
            inner: DocSession::from_blocks(kind, bytes).map_err(js)?,
        })
    }

    #[wasm_bindgen(js_name = fromSyxSlot)]
    pub fn from_syx_slot(kind: &str, bytes: &[u8], slot: u8) -> Result<Doc, JsError> {
        Ok(Doc {
            inner: DocSession::from_syx_slot(kind, bytes, slot).map_err(js)?,
        })
    }

    /// The document as `.syx` bytes (a Performance or Rhythm Set in User slot 1, or the System settings).
    #[wasm_bindgen(js_name = toSyx)]
    pub fn to_syx(&self, device: u8) -> Result<Vec<u8>, JsError> {
        self.inner.to_syx(device).map_err(js)
    }

    #[wasm_bindgen(js_name = byteLen)]
    pub fn byte_len(kind: &str) -> Result<usize, JsError> {
        DocSession::byte_len(kind).map_err(js)
    }

    #[wasm_bindgen(js_name = toBlocks)]
    pub fn to_blocks(&self) -> Vec<u8> {
        self.inner.to_blocks()
    }

    pub fn name(&self) -> String {
        self.inner.name()
    }

    #[wasm_bindgen(js_name = setName)]
    pub fn set_name(&mut self, name: &str) -> Result<(), JsError> {
        self.inner.set_name(name).map_err(js)
    }

    pub fn get(&self, block: &str, id: &str) -> Result<i32, JsError> {
        self.inner.get(block, id).map_err(js)
    }

    pub fn set(&mut self, block: &str, id: &str, value: i32) -> Result<(), JsError> {
        self.inner.set(block, id, value).map_err(js)
    }

    pub fn format(&self, block: &str, id: &str) -> Result<String, JsError> {
        self.inner.format(block, id).map_err(js)
    }

    #[wasm_bindgen(js_name = formatValue)]
    pub fn format_value(&self, block: &str, id: &str, value: i32) -> Result<String, JsError> {
        self.inner.format_value(block, id, value).map_err(js)
    }

    #[wasm_bindgen(js_name = issuesJson)]
    pub fn issues_json(&self) -> String {
        self.inner.issues_json()
    }

    #[wasm_bindgen(js_name = paramFrame)]
    pub fn param_frame(&self, device: u8, block: &str, id: &str) -> Result<Vec<u8>, JsError> {
        self.inner.param_frame(device, block, id).map_err(js)
    }

    #[wasm_bindgen(js_name = patchFrames)]
    pub fn patch_frames(&self, device: u8) -> Result<Vec<u8>, JsError> {
        self.inner.doc_frames(device).map_err(js)
    }
}
