//! The real MIDI transport, built on `midir` (feature `hardware`).
//!
//! SysEx must not be filtered by the backend, so only timing and Active Sensing messages are ignored.

use crate::transport::{MidiTransport, TransportError};
use midir::{Ignore, MidiInput, MidiInputConnection, MidiOutput, MidiOutputConnection};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{self, Receiver, RecvTimeoutError, TrySendError};
use std::sync::Arc;
use std::time::{Duration, Instant};

const CLIENT_NAME: &str = "XPPatchStudio";
/// Chunks buffered between the MIDI callback and the client; further chunks are dropped.
const QUEUE_CAPACITY: usize = 1024;

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
    TransportError::Port(e.to_string())
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

pub struct MidirTransport {
    // Held so the input connection stays open; messages arrive through `rx`.
    _input: MidiInputConnection<()>,
    output: MidiOutputConnection,
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
            output,
            rx,
            start: Instant::now(),
            listen_controllers,
        })
    }
}

impl MidiTransport for MidirTransport {
    fn send(&mut self, bytes: &[u8]) -> Result<(), TransportError> {
        self.output
            .send(bytes)
            .map_err(|e| TransportError::Send(e.to_string()))
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
        return Err(TransportError::Port(format!("no MIDI {kind} name given")));
    }
    let mut matches: Vec<(P, String)> = ports
        .into_iter()
        .filter_map(|p| name_of(&p).map(|n| (p, n)))
        .filter(|(_, n)| n.to_lowercase().contains(&wanted_lc))
        .collect();
    match matches.len() {
        0 => Err(TransportError::Port(format!(
            "no MIDI {kind} matching '{wanted}'"
        ))),
        1 => Ok(matches.remove(0).0),
        _ => Err(TransportError::Port(format!(
            "'{wanted}' matches several MIDI {kind}s: {}",
            matches
                .iter()
                .map(|(_, n)| n.as_str())
                .collect::<Vec<_>>()
                .join(", ")
        ))),
    }
}
