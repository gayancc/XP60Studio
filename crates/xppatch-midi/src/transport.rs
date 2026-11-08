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
    /// Opening or naming a port failed. The string is already musician-facing (no OS codes).
    #[error("{0}")]
    Port(String),
    /// Sending failed. The string is already musician-facing.
    #[error("{0}")]
    Send(String),
    #[error("The MIDI link was closed.")]
    Closed,
}

#[derive(Debug, Clone, Copy)]
enum MidiFailureKind {
    Open,
    Send,
}

/// Turn a midir / Windows MM / CoreMIDI raw error into a short message a musician can act on.
/// Never leave codes like `MMSYSERR_ALLOCATED` or `(os error 5)` in the UI.
pub fn explain_midi_os_error(raw: &str) -> String {
    explain_midi_failure(raw, MidiFailureKind::Open)
}

/// Same as [`explain_midi_os_error`], but wording for a failed send (port was already open).
pub fn explain_midi_send_error(raw: &str) -> String {
    explain_midi_failure(raw, MidiFailureKind::Send)
}

fn explain_midi_failure(raw: &str, kind: MidiFailureKind) -> String {
    let t = raw.to_ascii_lowercase();
    if t.contains("mmsyserr_allocated")
        || t.contains("already allocated")
        || (t.contains("device") && t.contains("in use"))
        || t.contains("resource busy")
    {
        return "That MIDI port is already open in another program (for example FL Studio). Close the other program, or pick a different port, then try Connect again.".into();
    }
    if t.contains("mmsyserr_baddeviceid")
        || t.contains("unknown port")
        || (t.contains("invalid device") && t.contains("midi"))
    {
        return "That MIDI port is no longer available. Press Rescan and choose it again.".into();
    }
    if t.contains("mmsyserr_nodriver") || (t.contains("no driver") && t.contains("midi")) {
        return "Windows has no driver for that MIDI port. Unplug the interface, plug it back in, then press Rescan.".into();
    }
    if t.contains("mmsyserr_notenabled") || (t.contains("not enabled") && t.contains("midi")) {
        return "That MIDI port is turned off in Windows. Enable it in Sound settings, then press Rescan.".into();
    }
    if (t.contains("access denied") || t.contains("permission") || t.contains("os error"))
        && (t.contains("midi") || t.contains("port") || t.contains("os error"))
    {
        return match kind {
            MidiFailureKind::Open => {
                "This app was not allowed to open that MIDI port. Quit other MIDI software and try again.".into()
            }
            MidiFailureKind::Send => {
                "Could not send MIDI to the instrument. Quit other MIDI software using the port, then try again.".into()
            }
        };
    }
    if t.contains("could not create") && t.contains("input") {
        return "Could not open the MIDI input. Close other MIDI software using that port, then try Connect again.".into();
    }
    if t.contains("could not create") && t.contains("output") {
        return "Could not open the MIDI output. Close other MIDI software using that port, then try Connect again.".into();
    }
    if t.contains("mmsyserr") || t.contains("osstatus") || t.contains("coremidi") {
        return generic_midi_failure(kind);
    }
    // Never pass OS / midir text through: it often looks "readable" while still carrying codes.
    generic_midi_failure(kind)
}

fn generic_midi_failure(kind: MidiFailureKind) -> String {
    match kind {
        MidiFailureKind::Open => {
            "Could not open the MIDI port. Close other programs using it, press Rescan, and try Connect again.".into()
        }
        MidiFailureKind::Send => {
            "Could not send MIDI to the instrument. Check the cable and connection, then try again.".into()
        }
    }
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn allocated_port_is_plain_english() {
        let msg = explain_midi_os_error(
            "MIDI port error: could not create Windows MM MIDI input stream (MMSYSERR_ALLOCATED)",
        );
        assert!(!msg.to_ascii_lowercase().contains("mmsyserr"));
        assert!(msg.contains("already open"));
    }

    #[test]
    fn os_error_codes_never_pass_through() {
        let msg = explain_midi_os_error("failed to open device (os error 5)");
        assert!(!msg.contains("os error"));
        assert!(!msg.contains('5') || msg.contains("Connect") || msg.contains("Quit"));
        let send = explain_midi_send_error("write failed (os error 5)");
        assert!(!send.contains("os error"));
        assert!(send.to_ascii_lowercase().contains("send") || send.contains("instrument"));
    }

    #[test]
    fn unknown_os_code_stays_actionable() {
        let msg = explain_midi_os_error("OSStatus -10830");
        assert!(!msg.contains("10830"));
        assert!(msg.contains("Rescan") || msg.contains("Connect"));
    }
}
