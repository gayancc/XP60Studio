//! MIDI transport and request/response client for the Roland XP-60.
//!
//! The [`Client`] talks to anything implementing [`MidiTransport`]: the real instrument through `midir` (feature
//! `hardware`) or the [`testing::VirtualXp60`] on a virtual clock.

#![forbid(unsafe_code)]

pub mod assembler;
pub mod client;
pub mod controls;
pub mod passthrough;
pub mod testing;
pub mod transport;

pub use assembler::{Incoming, MidiEvent, SysExAssembler};
pub use client::BankWriteReport;
pub use client::{
    BankFetch, CancelToken, Client, ClientConfig, ClientError, Expect, ProbeInfo, Progress,
    SendReport, SystemWritePlan, WritePlan,
};
pub use controls::{ControlChange, ControlDecoder, ControlSource};
pub use passthrough::{
    expected_message_len, is_channel_voice, is_system_realtime, prepare_forward, remap_channel,
    should_forward, PassThroughConfig,
};
pub use transport::{MidiTransport, TransportError};

#[cfg(feature = "hardware")]
pub mod hardware;
