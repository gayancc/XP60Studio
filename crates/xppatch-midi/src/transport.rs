//! The MIDI transport abstraction.
//!
//! A transport moves raw MIDI bytes and also owns the notion of time (`now`, `sleep`). Keeping time here lets the same
//! client code run against real hardware (wall clock) and against the virtual XP-60 in tests (a virtual clock), so
//! timeouts, pacing and retries are tested instantly and deterministically.

use std::time::Duration;
use thiserror::Error;

#[derive(Debug, Error, PartialEq, Eq, Clone)]
#[non_exhaustive]
pub enum TransportError {
    #[error("MIDI port error: {0}")]
    Port(String),
    #[error("MIDI send failed: {0}")]
    Send(String),
    #[error("the MIDI connection was closed")]
    Closed,
}

/// Raw MIDI in/out plus a clock.
pub trait MidiTransport {
    /// Send one complete MIDI message (for SysEx, one whole `F0 .. F7` frame).
    fn send(&mut self, bytes: &[u8]) -> Result<(), TransportError>;

    /// Wait up to `timeout` for the next incoming MIDI chunk. `Ok(None)` means nothing arrived in time. A chunk is
    /// whatever the backend delivers in one callback (usually a whole message, sometimes a fragment of a long SysEx).
    fn recv(&mut self, timeout: Duration) -> Result<Option<Vec<u8>>, TransportError>;

    /// Monotonic time since this transport was created.
    fn now(&self) -> Duration;

    /// Block for `duration` (virtual transports just advance their clock).
    fn sleep(&mut self, duration: Duration);

    /// Ask the backend to deliver controller messages (mod wheel, sliders, pedals). Off by default, so an idle
    /// connection to a busy keyboard cannot flood the queue; backends that see everything anyway ignore this.
    fn set_listen_controllers(&mut self, _on: bool) {}
}

/// Allow `&mut T` wherever a transport is expected.
impl<T: MidiTransport + ?Sized> MidiTransport for &mut T {
    fn send(&mut self, bytes: &[u8]) -> Result<(), TransportError> {
        (**self).send(bytes)
    }
    fn recv(&mut self, timeout: Duration) -> Result<Option<Vec<u8>>, TransportError> {
        (**self).recv(timeout)
    }
    fn now(&self) -> Duration {
        (**self).now()
    }
    fn sleep(&mut self, duration: Duration) {
        (**self).sleep(duration);
    }
    fn set_listen_controllers(&mut self, on: bool) {
        (**self).set_listen_controllers(on);
    }
}
