//! The real MIDI transport, built on `midir` (feature `hardware`).
//!
//! SysEx must not be filtered by the backend, so only timing and Active Sensing messages are ignored
//! on the primary instrument input. A separate [`MidiControllerPassThrough`] can open a second
//! controller input and forward channel-voice messages onto the same shared output.

use crate::passthrough::{prepare_forward, PassThroughConfig};
use crate::transport::{
    explain_midi_os_error, explain_midi_send_error, MidiTransport, TransportError,
};
use midir::{Ignore, MidiInput, MidiInputConnection, MidiOutput, MidiOutputConnection};
use std::sync::atomic::{AtomicBool, AtomicU8, Ordering};
use std::sync::mpsc::{self, Receiver, RecvTimeoutError, SyncSender, TrySendError};
use std::sync::{Arc, Mutex};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

const CLIENT_NAME: &str = "XPPatchStudio";
/// Chunks buffered between the MIDI callback and the client; further chunks are dropped.
const QUEUE_CAPACITY: usize = 1024;
/// Pass-through queue: small and bounded so a chatty controller cannot grow memory.
const THRU_QUEUE_CAPACITY: usize = 256;
/// Sentinel: keep the controller's original MIDI channel.
const CHANNEL_DIRECT: u8 = 0xFF;

/// Keep only what the client uses: SysEx, Program Change and Bank Select, and the other controllers while someone asks for
/// them. Notes, aftertouch and (by default) controllers from a busy keyboard are dropped in the callback so an idle
/// connection cannot grow memory.
fn is_relevant(bytes: &[u8], listen_controllers: bool) -> bool {
    match bytes.first() {
        Some(0xF0) => true,
        Some(&s) if (0xC0..0xD0).contains(&s) => true,
        Some(&s) if (0xB0..0xC0).contains(&s) => {
            listen_controllers || matches!(bytes.get(1), Some(0 | 32))
        }
        // A continuation fragment of a long SysEx has no status byte.
        Some(&s) => s < 0x80,
        None => false,
    }
}

/// Names of the available MIDI ports.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct PortList {
    pub inputs: Vec<String>,
    pub outputs: Vec<String>,
}

fn port_err(e: impl std::fmt::Display) -> TransportError {
    TransportError::Port(explain_midi_os_error(&e.to_string()))
}

pub fn list_ports() -> Result<PortList, TransportError> {
    let input = MidiInput::new(CLIENT_NAME).map_err(port_err)?;
    let output = MidiOutput::new(CLIENT_NAME).map_err(port_err)?;
    let inputs = input
        .ports()
        .iter()
        .map(|p| input.port_name(p).map_err(port_err))
        .collect::<Result<_, _>>()?;
    let outputs = output
        .ports()
        .iter()
        .map(|p| output.port_name(p).map_err(port_err))
        .collect::<Result<_, _>>()?;
    Ok(PortList { inputs, outputs })
}

/// Cloneable handle to the instrument MIDI output. Every send takes a short mutex so whole messages
/// (SysEx frames from the client and voice messages from controller pass-through) never interleave
/// mid-message. [`Self::close`] drops the OS port even while clones remain (disconnect / reconnect).
#[derive(Clone)]
pub struct SharedMidiOut {
    inner: Arc<Mutex<Option<MidiOutputConnection>>>,
}

impl SharedMidiOut {
    fn new(output: MidiOutputConnection) -> Self {
        Self {
            inner: Arc::new(Mutex::new(Some(output))),
        }
    }

    /// Send one complete MIDI message. Safe to call from the pass-through forwarder thread and from
    /// the client transport on the UI/worker thread.
    pub fn send(&self, bytes: &[u8]) -> Result<(), TransportError> {
        let mut guard = self
            .inner
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        let Some(out) = guard.as_mut() else {
            return Err(TransportError::Closed);
        };
        out.send(bytes)
            .map_err(|e| TransportError::Send(explain_midi_send_error(&e.to_string())))
    }

    /// Release the Windows/macOS output port. Further [`send`] calls fail with [`TransportError::Closed`].
    pub fn close(&self) {
        *self
            .inner
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner) = None;
    }
}

pub struct MidirTransport {
    // Held so the input connection stays open; messages arrive through `rx`.
    _input: MidiInputConnection<()>,
    output: SharedMidiOut,
    /// Resolved primary input port name (for refusing the same port as controller thru).
    pub input_name: String,
    rx: Receiver<Vec<u8>>,
    start: Instant,
    listen_controllers: Arc<AtomicBool>,
}

impl MidirTransport {
    /// Open the first input and output whose names contain the given text (case-insensitive).
    pub fn connect(input_name: &str, output_name: &str) -> Result<Self, TransportError> {
        let mut midi_in = MidiInput::new(CLIENT_NAME).map_err(port_err)?;
        midi_in.ignore(Ignore::TimeAndActiveSense);
        let midi_out = MidiOutput::new(CLIENT_NAME).map_err(port_err)?;

        let in_port = pick(
            midi_in.ports(),
            |p| midi_in.port_name(p).ok(),
            input_name,
            "input",
        )?;
        let resolved_in = midi_in.port_name(&in_port).map_err(port_err)?;
        let out_port = pick(
            midi_out.ports(),
            |p| midi_out.port_name(p).ok(),
            output_name,
            "output",
        )?;

        let (tx, rx) = mpsc::sync_channel(QUEUE_CAPACITY);
        let listen_controllers = Arc::new(AtomicBool::new(false));
        let listen_in_callback = Arc::clone(&listen_controllers);
        let input = midi_in
            .connect(
                &in_port,
                "xppatch-in",
                move |_, bytes, ()| {
                    if is_relevant(bytes, listen_in_callback.load(Ordering::Relaxed)) {
                        // Full: drop. Disconnected: the transport was dropped.
                        if let Err(TrySendError::Full(_) | TrySendError::Disconnected(_)) =
                            tx.try_send(bytes.to_vec())
                        {}
                    }
                },
                (),
            )
            .map_err(port_err)?;
        let output = midi_out
            .connect(&out_port, "xppatch-out")
            .map_err(port_err)?;
        Ok(MidirTransport {
            _input: input,
            output: SharedMidiOut::new(output),
            input_name: resolved_in,
            rx,
            start: Instant::now(),
            listen_controllers,
        })
    }

    /// Shared instrument output for a secondary controller pass-through (same cable, no second open).
    pub fn shared_output(&self) -> SharedMidiOut {
        self.output.clone()
    }

    /// Release the output port (used before reconnect so a stale pass-through clone cannot keep it open).
    pub fn close_output(&self) {
        self.output.close();
    }
}

impl MidiTransport for MidirTransport {
    fn send(&mut self, bytes: &[u8]) -> Result<(), TransportError> {
        self.output.send(bytes)
    }

    fn recv(&mut self, timeout: Duration) -> Result<Option<Vec<u8>>, TransportError> {
        match self.rx.recv_timeout(timeout) {
            Ok(bytes) => Ok(Some(bytes)),
            Err(RecvTimeoutError::Timeout) => Ok(None),
            Err(RecvTimeoutError::Disconnected) => Err(TransportError::Closed),
        }
    }

    fn now(&self) -> Duration {
        self.start.elapsed()
    }

    fn sleep(&mut self, duration: Duration) {
        std::thread::sleep(duration);
    }

    fn set_listen_controllers(&mut self, on: bool) {
        self.listen_controllers.store(on, Ordering::Relaxed);
    }
}

/// Live state for [`MidiControllerPassThrough`], readable without taking the MIDI mutex.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PassThroughStatus {
    pub input: String,
    pub enabled: bool,
    /// Held during long SysEx bank operations so voice traffic cannot contend with dumps.
    pub held: bool,
    pub filter_realtime: bool,
    /// `None` = original channel; `Some(0..=15)` = remapped audition channel.
    pub target_channel: Option<u8>,
}

/// Secondary controller input → shared XP-60 output. The MIDI callback only filters and
/// `try_send`s; a dedicated forwarder thread performs the short output lock and send.
pub struct MidiControllerPassThrough {
    input_name: String,
    input: Option<MidiInputConnection<()>>,
    forwarder: Option<JoinHandle<()>>,
    stop: Arc<AtomicBool>,
    enabled: Arc<AtomicBool>,
    hold: Arc<AtomicBool>,
    filter_realtime: Arc<AtomicBool>,
    /// `CHANNEL_DIRECT` = keep original; otherwise 0..=15.
    target_channel: Arc<AtomicU8>,
    out: SharedMidiOut,
}

impl MidiControllerPassThrough {
    /// Open `input_name` (substring match, same rules as [`MidirTransport::connect`]) and start
    /// forwarding onto `out`.
    pub fn open(
        input_name: &str,
        out: SharedMidiOut,
        config: PassThroughConfig,
    ) -> Result<Self, TransportError> {
        if let Some(ch) = config.target_channel {
            if ch > 15 {
                return Err(TransportError::Port(
                    "Controller pass-through channel must be 1 to 16.".into(),
                ));
            }
        }

        let mut midi_in = MidiInput::new(&format!("{CLIENT_NAME} Thru")).map_err(port_err)?;
        // Always ignore Time/ActiveSense at the OS level when filtering; if the user later disables
        // the filter we still drop them in the callback unless filter_realtime is false — midir's
        // Ignore is set once at connect, so we use Ignore::None and enforce policy in software.
        midi_in.ignore(Ignore::None);

        let in_port = pick(
            midi_in.ports(),
            |p| midi_in.port_name(p).ok(),
            input_name,
            "input",
        )?;
        let resolved = midi_in.port_name(&in_port).map_err(port_err)?;

        let (tx, rx) = mpsc::sync_channel::<Vec<u8>>(THRU_QUEUE_CAPACITY);
        let stop = Arc::new(AtomicBool::new(false));
        let enabled = Arc::new(AtomicBool::new(true));
        let hold = Arc::new(AtomicBool::new(false));
        let filter_realtime = Arc::new(AtomicBool::new(config.filter_realtime));
        let target_channel = Arc::new(AtomicU8::new(match config.target_channel {
            Some(ch) => ch,
            None => CHANNEL_DIRECT,
        }));

        let cb_enabled = Arc::clone(&enabled);
        let cb_hold = Arc::clone(&hold);
        let cb_filter = Arc::clone(&filter_realtime);
        let cb_target = Arc::clone(&target_channel);
        let cb_tx: SyncSender<Vec<u8>> = tx;

        let input = midi_in
            .connect(
                &in_port,
                "xppatch-thru-in",
                move |_, bytes, ()| {
                    if !cb_enabled.load(Ordering::Relaxed) || cb_hold.load(Ordering::Relaxed) {
                        return;
                    }
                    let target = match cb_target.load(Ordering::Relaxed) {
                        CHANNEL_DIRECT => None,
                        ch if ch <= 15 => Some(ch),
                        _ => None,
                    };
                    let cfg = PassThroughConfig {
                        target_channel: target,
                        filter_realtime: cb_filter.load(Ordering::Relaxed),
                    };
                    let Some(msg) = prepare_forward(bytes, &cfg) else {
                        return;
                    };
                    // Full or disconnected: drop. Callback stays non-blocking.
                    let _ = cb_tx.try_send(msg);
                },
                (),
            )
            .map_err(port_err)?;

        let fwd_stop = Arc::clone(&stop);
        let fwd_enabled = Arc::clone(&enabled);
        let fwd_hold = Arc::clone(&hold);
        let fwd_out = out.clone();
        let forwarder = thread::Builder::new()
            .name("xppatch-midi-thru".into())
            .spawn(move || thru_forwarder(rx, fwd_out, fwd_stop, fwd_enabled, fwd_hold))
            .map_err(|_| {
                TransportError::Port(
                    "Could not start controller pass-through. Try Connect again.".into(),
                )
            })?;

        Ok(Self {
            input_name: resolved,
            input: Some(input),
            forwarder: Some(forwarder),
            stop,
            enabled,
            hold,
            filter_realtime,
            target_channel,
            out,
        })
    }

    pub fn status(&self) -> PassThroughStatus {
        let raw = self.target_channel.load(Ordering::Relaxed);
        PassThroughStatus {
            input: self.input_name.clone(),
            enabled: self.enabled.load(Ordering::Relaxed),
            held: self.hold.load(Ordering::Relaxed),
            filter_realtime: self.filter_realtime.load(Ordering::Relaxed),
            target_channel: if raw == CHANNEL_DIRECT {
                None
            } else {
                Some(raw)
            },
        }
    }

    /// Enable or disable forwarding without closing the port. Disabling silences hanging notes.
    pub fn set_enabled(&self, on: bool) {
        let was = self.enabled.swap(on, Ordering::SeqCst);
        if was && !on {
            silence_all(&self.out);
        }
    }

    /// Hold forwarding during long SysEx bank reads/writes. Entering hold silences hanging notes
    /// and the forwarder drops anything already queued (it does not emit after silence).
    pub fn set_hold(&self, on: bool) {
        let was = self.hold.swap(on, Ordering::SeqCst);
        if !was && on {
            silence_all(&self.out);
        }
    }

    pub fn set_filter_realtime(&self, on: bool) {
        self.filter_realtime.store(on, Ordering::Relaxed);
    }

    /// `None` = original channel; `Some(0..=15)` = remap. Invalid channels are refused.
    /// Changing the channel silences first so Note Offs are not rewritten away from sounding notes.
    pub fn set_target_channel(&self, channel: Option<u8>) -> Result<(), TransportError> {
        let next = match channel {
            None => CHANNEL_DIRECT,
            Some(ch) if ch <= 15 => ch,
            Some(_) => {
                return Err(TransportError::Port(
                    "Controller pass-through channel must be 1 to 16.".into(),
                ));
            }
        };
        let prev = self.target_channel.swap(next, Ordering::SeqCst);
        if prev != next {
            silence_all(&self.out);
        }
        Ok(())
    }

    /// Apply a full config snapshot (channel + real-time filter). Does not change enabled/hold.
    pub fn apply_config(&self, config: PassThroughConfig) -> Result<(), TransportError> {
        self.set_target_channel(config.target_channel)?;
        self.set_filter_realtime(config.filter_realtime);
        Ok(())
    }

    /// Close the input and stop the forwarder. Safe to call more than once.
    pub fn shutdown(&mut self) {
        self.stop.store(true, Ordering::SeqCst);
        self.enabled.store(false, Ordering::SeqCst);
        self.hold.store(true, Ordering::SeqCst);
        silence_all(&self.out);
        drop(self.input.take());
        if let Some(handle) = self.forwarder.take() {
            let _ = handle.join();
        }
    }
}

impl Drop for MidiControllerPassThrough {
    fn drop(&mut self) {
        self.shutdown();
    }
}

fn thru_forwarder(
    rx: Receiver<Vec<u8>>,
    out: SharedMidiOut,
    stop: Arc<AtomicBool>,
    enabled: Arc<AtomicBool>,
    hold: Arc<AtomicBool>,
) {
    while !stop.load(Ordering::Relaxed) {
        match rx.recv_timeout(Duration::from_millis(50)) {
            Ok(msg) => {
                // Re-check at send time so a hold/disable that raced the callback still drops
                // queued voice traffic instead of injecting it between SysEx frames.
                if enabled.load(Ordering::SeqCst) && !hold.load(Ordering::SeqCst) {
                    let _ = out.send(&msg);
                }
            }
            Err(RecvTimeoutError::Timeout) => {}
            Err(RecvTimeoutError::Disconnected) => break,
        }
    }
    // Drain without sending: shutdown already silenced; do not inject notes into a dump.
    while rx.try_recv().is_ok() {}
}

fn silence_all(out: &SharedMidiOut) {
    for ch in 0..16u8 {
        // Damper off, All Sound Off, All Notes Off — cover held sustain during bank ops.
        let _ = out.send(&[0xB0 | ch, 64, 0]);
        let _ = out.send(&[0xB0 | ch, 120, 0]);
        let _ = out.send(&[0xB0 | ch, 123, 0]);
    }
}

/// Choose the one port whose name contains `wanted` (case-insensitive). An empty name or an ambiguous match is an
/// error that lists the candidates, so a write can never go to a device the user did not mean.
fn pick<P>(
    ports: Vec<P>,
    name_of: impl Fn(&P) -> Option<String>,
    wanted: &str,
    kind: &str,
) -> Result<P, TransportError> {
    let wanted_lc = wanted.trim().to_lowercase();
    if wanted_lc.is_empty() {
        return Err(TransportError::Port(format!("Choose a MIDI {kind} first.")));
    }
    let mut matches: Vec<(P, String)> = ports
        .into_iter()
        .filter_map(|p| name_of(&p).map(|n| (p, n)))
        .filter(|(_, n)| n.to_lowercase().contains(&wanted_lc))
        .collect();
    match matches.len() {
        0 => Err(TransportError::Port(format!(
            "No MIDI {kind} named \"{wanted}\" was found. Press Rescan — the name may have changed."
        ))),
        1 => Ok(matches.remove(0).0),
        _ => Err(TransportError::Port(format!(
            "\"{wanted}\" matches more than one MIDI {kind}. Pick the exact name from the list: {}",
            matches
                .iter()
                .map(|(_, n)| n.as_str())
                .collect::<Vec<_>>()
                .join(", ")
        ))),
    }
}
