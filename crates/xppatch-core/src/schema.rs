//! The parameter map as data. `data/xp60-parameters.json` is the single source of truth: the codec here, and the
//! editor controls in later phases, are driven by it and nothing is hand-copied.

use serde::Deserialize;
use std::collections::HashMap;

/// How a parameter's value is stored in its bytes.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Encoding {
    /// One data byte holding the raw value.
    Seven,
    /// One data byte holding an ASCII character.
    Ascii,
    /// `bytes` consecutive low-nibble bytes, most significant nibble first.
    Nibble,
}

/// One parameter inside a block.
#[derive(Debug, Clone, Deserialize)]
pub struct ParamDef {
    pub id: String,
    pub name: String,
    /// Offset in data bytes from the start of the block.
    pub offset: usize,
    pub bytes: usize,
    pub encoding: Encoding,
    pub min: i32,
    pub max: i32,
    pub display: String,
}

/// A contiguous block of parameters (for example one Patch Tone).
#[derive(Debug, Clone, Deserialize)]
pub struct BlockDef {
    pub id: String,
    pub family: String,
    pub name: String,
    /// Block length in data bytes.
    pub size: usize,
    pub params: Vec<ParamDef>,
    #[serde(skip)]
    pub(crate) index: HashMap<String, usize>,
}

impl BlockDef {
    /// Look a parameter up by its stable id.
    pub fn param(&self, id: &str) -> Option<&ParamDef> {
        self.index.get(id).map(|&i| &self.params[i])
    }
}

/// Transfer limits recorded with the map.
#[derive(Debug, Clone, Deserialize)]
pub struct Limits {
    pub max_dt1_data_to_device: usize,
    pub min_dt1_gap_ms: u32,
    pub first_response_timeout_ms: u32,
}

/// The whole document.
#[derive(Debug, Clone, Deserialize)]
pub struct Document {
    pub schema: u32,
    pub model: String,
    pub limits: Limits,
    pub blocks: Vec<BlockDef>,
}
