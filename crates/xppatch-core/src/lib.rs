//! XP-60 domain model. Data-driven from `data/xp60-parameters.json`; no I/O and no UI.
//!
//! - [`schema`] / [`map`]: the parameter map and its validating loader
//! - [`block`]: a block of bytes with typed, range-checked parameter access
//! - [`patch`]: Common + four Tones, to and from DT1 pieces
//! - [`syx`]: `.syx` files and User Patch banks

pub mod block;
pub mod display;
pub mod doc;
pub mod map;
pub mod patch;
pub mod schema;
pub mod smf;
pub mod syx;

pub use block::{BlockBuf, CodecError, Issue, IssueKind};
pub use display::{Display, DisplayError};
pub use doc::{DocKind, Document};
pub use map::{MapError, ParameterMap};
pub use patch::{Patch, PatchBlock, PatchError, TONES};
pub use schema::{BlockDef, Encoding, ParamDef};
pub use syx::{
    system_document_bytes, user_document_bytes, user_patch_bank_bytes, SyxError, SyxImage,
};
