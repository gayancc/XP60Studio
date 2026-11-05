//! Roland XP-60 SysEx layer. Pure functions, no I/O and no UI.
//!
//! - [`address`]: 4-byte base-128 addresses and sizes
//! - [`checksum`]: the Roland checksum
//! - [`message`]: strict RQ1 / DT1 encode and parse
//! - [`stream`]: split a byte stream (`.syx` file or MIDI input) into frames
//! - [`chunk`]: split a block into DT1 messages the XP-60 accepts
//! - [`memory`]: the memory map and the temporary / permanent write classification

pub mod address;
pub mod checksum;
pub mod chunk;
pub mod memory;
pub mod message;
pub mod stream;

pub use address::{Address, AddressError, Size};
pub use checksum::checksum;
pub use message::{
    DeviceId, EncodeError, Message, ParseError, Parsed, MANUFACTURER_ROLAND, MODEL_ID,
};
