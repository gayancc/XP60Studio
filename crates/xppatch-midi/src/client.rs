//! Request/response client for the XP-60.
//!
//! Rules it enforces (manual and legacy hardware log; re-confirmed in the hardware phase, see
//! `artifact/docs/xppatchstudio/07-hardware-acceptance.md`):
//!
//! - **One RQ1 at a time.** A new request is sent only after the previous one completed or was abandoned.
//! - **Replies are collected by expectation**: the caller says which blocks (address, length) must arrive; the request
//!   completes when every byte of every expected block is covered, so sparse replies and partial blocks work.
//! - **Timeouts and retries**: first response within `first_response_timeout`, later replies within
//!   `inter_reply_timeout`; a failed attempt is retried up to `max_retries` times; a final failure names the blocks
//!   that never arrived.
//! - **Paced writes**: DT1 messages are at most 128 data bytes and at least `min_dt1_gap` apart.
//! - **Only temporary memory is writable here.** Writes to User/System memory are deliberately not offered until the
//!   write ceremony (backup, explicit arm, read-back) exists in phase 6.

use crate::assembler::{Incoming, MidiEvent, SysExAssembler};
use crate::controls::{ControlChange, ControlDecoder};
use crate::transport::{MidiTransport, TransportError};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;
use thiserror::Error;
use xppatch_core::{DocKind, Document, Patch, PatchError};
use xppatch_sysex::address::{Address, AddressError};
use xppatch_sysex::chunk::{split_dt1, ChunkError, MAX_DT1_DATA_TO_DEVICE};
use xppatch_sysex::memory::{
    classify_range, user_patch, user_performance, user_rhythm, MemoryError, WriteKind,
    PATCH_TONE_OFFSETS, SYSTEM, TEMP_PATCH,
};
use xppatch_sysex::message::{DeviceId, EncodeError, Message, Parsed};

/// Longest slice handed to `recv`, so cancellation is noticed promptly.
const POLL_SLICE: Duration = Duration::from_millis(100);
/// The shortest gap the instrument is documented to accept between writes, and the longest one offered.
const MIN_DT1_GAP: Duration = Duration::from_millis(20);
const MAX_DT1_GAP: Duration = Duration::from_millis(250);
/// Hard ceiling for one request attempt, however many replies keep arriving.
const MAX_ATTEMPT: Duration = Duration::from_secs(10);
/// Most chunks discarded before a request; the rest stay queued rather than hanging the caller.
const MAX_DRAIN_CHUNKS: usize = 256;
/// Most distinct (channel, source) pairs kept between calls to `take_controls`.
const MAX_CONTROLS: usize = 512;

/// Most channel events kept between calls to `take_events` (the oldest are kept, extras are dropped).
const MAX_EVENTS: usize = 256;
/// Largest block a request may expect (the biggest XP-60 block is 129 bytes).
const MAX_EXPECT_LEN: usize = 1024;
/// Most retries honoured, whatever the configuration says.
const MAX_RETRIES: u32 = 10;
/// A bank read gives up after this many consecutive slots with no reply at all.
const MAX_SILENT_SLOTS: usize = 3;
/// Length of the patch name read by a probe.
const NAME_LEN: usize = 12;

#[derive(Debug, Clone)]
pub struct ClientConfig {
    pub device: DeviceId,
    pub first_response_timeout: Duration,
    pub inter_reply_timeout: Duration,
    /// Minimum time between DT1 messages sent to the instrument.
    pub min_dt1_gap: Duration,
    pub max_retries: u32,
    /// Timeout for a single probe (no retries) used by the device-ID scan.
    pub probe_timeout: Duration,
    /// Pause after the last write before reading back for verification.
    pub verify_settle: Duration,
    /// Max data bytes per DT1 to the device (Patch Base "chunk"; 16..=128, instrument limit 128).
    pub max_dt1_data: usize,
    /// Optional byte-rate throttle (Patch Base default 3125 B/s = MIDI wire rate). `None` means off.
    pub byte_rate: Option<u16>,
}

impl Default for ClientConfig {
    fn default() -> Self {
        ClientConfig {
            device: DeviceId::DEFAULT,
            first_response_timeout: Duration::from_millis(1500),
            inter_reply_timeout: Duration::from_millis(750),
            min_dt1_gap: Duration::from_millis(20),
            max_retries: 2,
            probe_timeout: Duration::from_millis(600),
            verify_settle: Duration::from_millis(50),
            max_dt1_data: MAX_DT1_DATA_TO_DEVICE,
            byte_rate: None,
        }
    }
}

/// A block (or part of one) that must arrive for a request to be complete.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Expect {
    pub address: Address,
    pub len: usize,
}

/// Cooperative cancellation shared with another thread (a UI button).
#[derive(Debug, Clone, Default)]
pub struct CancelToken(Arc<AtomicBool>);

impl CancelToken {
    pub fn new() -> Self {
        Self::default()
    }
    pub fn cancel(&self) {
        self.0.store(true, Ordering::SeqCst);
    }
    pub fn is_cancelled(&self) -> bool {
        self.0.load(Ordering::SeqCst)
    }
}

/// Progress report for long operations.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Progress {
    pub what: &'static str,
    pub done: usize,
    pub total: usize,
}

#[derive(Debug, Error, PartialEq, Eq)]
#[non_exhaustive]
pub enum ClientError {
    #[error(transparent)]
    Transport(#[from] TransportError),
    #[error(
        "The XP-60 did not answer in time (tried {attempts} times). Check the cable, power, and Device ID."
    )]
    Timeout {
        attempts: u32,
        missing: Vec<Address>,
    },
    // Keep this generic: Cancelled covers bank reads, document fetches and writes. A write-specific
    // residual warning misled the Bank Cancel hardware check (H-048) when only a read was stopped.
    #[error("Cancelled.")]
    Cancelled,
    #[error("The patch on the XP-60 changed from the panel while it was being read.")]
    Stale,
    #[error("That patch data is not valid.")]
    Patch(#[from] PatchError),
    #[error("{0}")]
    Refused(String),
    #[error("What came back from the XP-60 did not match what was sent.")]
    Mismatch { blocks: Vec<String> },
    #[error("Could not build that MIDI message.")]
    Encode(#[from] EncodeError),
    #[error("Could not split that MIDI message.")]
    Chunk(#[from] ChunkError),
    #[error("That MIDI address is not valid.")]
    Address(#[from] AddressError),
    #[error("That memory location is out of range.")]
    Memory(#[from] MemoryError),
}

/// What a successful probe learned.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProbeInfo {
    pub device: DeviceId,
    /// The temporary patch's name.
    pub patch_name: String,
}

/// What a temporary write did.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SendReport {
    pub messages: usize,
    pub bytes: usize,
    pub verified: bool,
}

/// A planned write into a User patch slot. It can only be made by reading the slot first, so what is about to be
/// overwritten is always in hand (to be backed up to disk and, if needed, put back).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct WritePlan {
    slot: u8,
    backup: Patch,
}

impl WritePlan {
    pub fn slot(&self) -> u8 {
        self.slot
    }

    /// What the slot held when the plan was made.
    pub fn backup(&self) -> &Patch {
        &self.backup
    }

    /// The name of the patch that will be replaced.
    pub fn occupant_name(&self) -> String {
        self.backup.name()
    }
}

/// A planned write of the System settings. It can only be made by reading the settings first, so what is about to be
/// overwritten is always in hand (to be backed up to disk and, if needed, put back).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SystemWritePlan {
    backup: Document,
}

impl SystemWritePlan {
    /// What the System settings held when the plan was made.
    pub fn backup(&self) -> &Document {
        &self.backup
    }
}

/// What a bank write did.
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct BankWriteReport {
    /// Slots written and read back equal.
    pub written: Vec<u8>,
    /// The slot where the write stopped, and why. The run stops at the first failure (with User Memory Protect on
    /// every slot would fail the same way).
    pub stopped_at: Option<(u8, String)>,
    pub cancelled: bool,
}

/// Result of a bank read: everything that worked, plus what did not.
#[derive(Debug, Default, PartialEq, Eq)]
pub struct BankFetch {
    pub patches: Vec<(u8, Patch)>,
    pub failures: Vec<(u8, ClientError)>,
    pub cancelled: bool,
    /// Stopped early because several slots in a row got no reply (nothing is connected).
    pub aborted: bool,
}

pub struct Client<T: MidiTransport> {
    port: T,
    cfg: ClientConfig,
    assembler: SysExAssembler,
    last_dt1: Option<Duration>,
    /// Data bytes in the last DT1, for the optional byte-rate throttle.
    last_dt1_data: Option<usize>,
    events: Vec<MidiEvent>,
    /// Controller changes since the last `take_controls`, one entry per (channel, source) holding its latest value.
    controls: Vec<ControlChange>,
    decoder: ControlDecoder,
    /// Every Program/Bank event ever seen, so a read can tell the patch changed underneath it.
    event_count: u64,
    ignored_frames: usize,
}

impl<T: MidiTransport> Client<T> {
    pub fn new(port: T, cfg: ClientConfig) -> Self {
        Client {
            port,
            cfg,
            assembler: SysExAssembler::new(),
            last_dt1: None,
            last_dt1_data: None,
            events: Vec::new(),
            controls: Vec::new(),
            decoder: ControlDecoder::new(),
            event_count: 0,
            ignored_frames: 0,
        }
    }

    pub fn config(&self) -> &ClientConfig {
        &self.cfg
    }

    /// Change the gap between DT1 messages. It can be made longer for a slow or unreliable cable but never shorter than
    /// the documented 20 ms; the value actually used is returned.
    pub fn set_min_dt1_gap(&mut self, gap: Duration) -> Duration {
        let gap = gap.clamp(MIN_DT1_GAP, MAX_DT1_GAP);
        self.cfg.min_dt1_gap = gap;
        gap
    }

    /// Cap DT1 data bytes (Patch Base chunk). Clamped to 16..=128; the value used is returned.
    pub fn set_max_dt1_data(&mut self, bytes: usize) -> usize {
        let bytes = bytes.clamp(16, MAX_DT1_DATA_TO_DEVICE);
        self.cfg.max_dt1_data = bytes;
        bytes
    }

    /// Optional byte-rate throttle (Patch Base: 3125 B/s when on). `None` turns it off.
    pub fn set_byte_rate(&mut self, rate: Option<u16>) {
        self.cfg.byte_rate = rate.filter(|r| *r > 0);
    }

    pub fn set_device(&mut self, device: DeviceId) {
        self.cfg.device = device;
    }

    pub fn port(&self) -> &T {
        &self.port
    }

    pub fn port_mut(&mut self) -> &mut T {
        &mut self.port
    }

    pub fn into_port(self) -> T {
        self.port
    }

    /// Frames that were received but not part of any reply (noise, wrong device, malformed).
    pub fn ignored_frames(&self) -> usize {
        self.ignored_frames
    }

    /// Program/Bank change events seen since the last call. Any event means the instrument's temporary patch may have
    /// been replaced from the panel, so a cached copy is **stale**.
    pub fn take_events(&mut self) -> Vec<MidiEvent> {
        std::mem::take(&mut self.events)
    }

    /// Controller changes heard since the last call, oldest first, each (channel, source) once with its latest value:
    /// a plain controller, or an NRPN or RPN value.
    /// These never mean the temporary patch changed, so they are kept apart from [`Client::take_events`].
    pub fn take_controls(&mut self) -> Vec<ControlChange> {
        std::mem::take(&mut self.controls)
    }

    /// Start or stop listening for controllers (a transport may not deliver them otherwise).
    pub fn listen_controllers(&mut self, on: bool) {
        self.port.set_listen_controllers(on);
        if !on {
            self.controls.clear();
        }
        self.decoder.reset();
    }

    /// Listen for up to `timeout` while idle, collecting events (for the stale-state indicator).
    pub fn poll(&mut self, timeout: Duration) -> Result<(), ClientError> {
        let deadline = self.port.now() + timeout;
        loop {
            let left = deadline.saturating_sub(self.port.now());
            if left.is_zero() {
                return Ok(());
            }
            if let Some(chunk) = self.port.recv(left.min(POLL_SLICE))? {
                self.absorb(&chunk);
            }
        }
    }

    fn absorb(&mut self, chunk: &[u8]) -> Vec<Parsed> {
        let mut parsed = Vec::new();
        for item in self.assembler.feed(chunk) {
            match item {
                Incoming::SysEx(frame) => match Message::parse(&frame) {
                    Ok(p) => parsed.push(p),
                    Err(_) => self.ignored_frames += 1,
                },
                Incoming::Truncated => self.ignored_frames += 1,
                Incoming::Event(MidiEvent::Controller {
                    channel,
                    controller,
                    value,
                }) => {
                    if let Some(change) = self.decoder.feed(channel, controller, value) {
                        let known = self
                            .controls
                            .iter()
                            .position(|c| c.channel == channel && c.source == change.source);
                        match known {
                            Some(i) => self.controls[i] = change,
                            None if self.controls.len() < MAX_CONTROLS => {
                                self.controls.push(change)
                            }
                            None => {}
                        }
                    }
                }
                Incoming::Event(e) => {
                    self.event_count += 1;
                    if self.events.len() < MAX_EVENTS {
                        self.events.push(e);
                    }
                }
            }
        }
        parsed
    }

    /// Send one RQ1 and collect replies until every expected block is fully covered.
    fn request(
        &mut self,
        address: Address,
        size: Address,
        expect: &[Expect],
        cancel: &CancelToken,
        retries: u32,
        first_timeout: Duration,
    ) -> Result<Vec<(Address, Vec<u8>)>, ClientError> {
        let frame = Message::Rq1 { address, size }.encode(self.cfg.device)?;
        let span = size.compact() as usize;
        if expect
            .iter()
            .any(|e| e.len == 0 || e.len > MAX_EXPECT_LEN || e.len > span)
        {
            return Err(ClientError::Refused(
                "expected block length is zero, larger than the request, or over the limit".into(),
            ));
        }
        let attempts = retries.min(MAX_RETRIES) + 1;
        let mut missing = Vec::new();
        for attempt in 0..attempts {
            if cancel.is_cancelled() {
                return Err(ClientError::Cancelled);
            }
            if attempt > 0 {
                // Let a reply that is still streaming from the failed attempt finish; the instrument drops a
                // request that arrives while it is busy.
                self.poll(self.cfg.inter_reply_timeout)?;
            }
            // Discard anything left over so it cannot be mistaken for this request's reply.
            for _ in 0..MAX_DRAIN_CHUNKS {
                if cancel.is_cancelled() {
                    return Err(ClientError::Cancelled);
                }
                match self.port.recv(Duration::ZERO)? {
                    Some(chunk) => {
                        self.absorb(&chunk);
                    }
                    None => break,
                }
            }
            self.assembler.reset();
            self.port.send(&frame)?;

            let mut collector = Collector::new(expect);
            let started = self.port.now();
            let ceiling = started.saturating_add(MAX_ATTEMPT);
            let mut deadline = started.saturating_add(first_timeout).min(ceiling);
            loop {
                if cancel.is_cancelled() {
                    return Err(ClientError::Cancelled);
                }
                let left = deadline.saturating_sub(self.port.now());
                if left.is_zero() {
                    break;
                }
                let Some(chunk) = self.port.recv(left.min(POLL_SLICE))? else {
                    continue;
                };
                for reply in self.absorb(&chunk) {
                    match reply.message {
                        Message::Dt1 { address, data } if reply.device == self.cfg.device => {
                            match collector.place(address, data) {
                                Placed::New => {
                                    deadline = self
                                        .port
                                        .now()
                                        .saturating_add(self.cfg.inter_reply_timeout)
                                        .min(ceiling);
                                }
                                Placed::Duplicate | Placed::Rejected => self.ignored_frames += 1,
                            }
                        }
                        _ => self.ignored_frames += 1,
                    }
                }
                if collector.is_complete() {
                    return Ok(collector.into_pieces());
                }
            }
            missing = collector.missing();
        }
        Err(ClientError::Timeout { attempts, missing })
    }

    /// Request a span and wait for the blocks named in `expect` (sparse replies are fine).
    pub fn fetch_span(
        &mut self,
        address: Address,
        size: Address,
        expect: &[Expect],
        cancel: &CancelToken,
    ) -> Result<Vec<(Address, Vec<u8>)>, ClientError> {
        let (retries, timeout) = (self.cfg.max_retries, self.cfg.first_response_timeout);
        self.request(address, size, expect, cancel, retries, timeout)
    }

    /// Read a whole patch (Common + 4 Tones) at `base`, one block per request (never pipelined).
    pub fn fetch_patch(
        &mut self,
        base: Address,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<Patch, ClientError> {
        for _ in 0..2 {
            let before = self.event_count;
            let patch = self.fetch_patch_once(base, cancel, progress)?;
            if self.event_count == before {
                return Ok(patch);
            }
        }
        Err(ClientError::Stale)
    }

    fn fetch_patch_once(
        &mut self,
        base: Address,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<Patch, ClientError> {
        let blocks: Vec<(Address, usize)> = Patch::init()
            .to_dt1(base)?
            .into_iter()
            .map(|(a, d)| (a, d.len()))
            .collect();
        let total = blocks.len();
        let mut pieces = Vec::new();
        for (i, (address, len)) in blocks.into_iter().enumerate() {
            progress(Progress {
                what: "patch blocks",
                done: i,
                total,
            });
            let size =
                Address::from_compact(u32::try_from(len).map_err(|_| AddressError::Overflow)?)?;
            pieces.extend(self.fetch_span(address, size, &[Expect { address, len }], cancel)?);
        }
        progress(Progress {
            what: "patch blocks",
            done: total,
            total,
        });
        Ok(Patch::from_dt1(base, &pieces)?)
    }

    /// Read a Performance or Rhythm Set based at `base`, one block per request (never pipelined).
    pub fn fetch_document(
        &mut self,
        kind: DocKind,
        base: Address,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<Document, ClientError> {
        let blocks: Vec<(Address, usize)> = Document::init(kind)
            .to_dt1(base)?
            .into_iter()
            .map(|(a, d)| (a, d.len()))
            .collect();
        let total = blocks.len();
        let mut pieces = Vec::new();
        for (i, (address, len)) in blocks.into_iter().enumerate() {
            progress(Progress {
                what: "document blocks",
                done: i,
                total,
            });
            let size =
                Address::from_compact(u32::try_from(len).map_err(|_| AddressError::Overflow)?)?;
            pieces.extend(self.fetch_span(address, size, &[Expect { address, len }], cancel)?);
        }
        progress(Progress {
            what: "document blocks",
            done: total,
            total,
        });
        Ok(Document::from_dt1(kind, base, &pieces)?)
    }

    /// Read the instrument's temporary Performance or Rhythm Set.
    pub fn fetch_temp_document(
        &mut self,
        kind: DocKind,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<Document, ClientError> {
        self.fetch_document(kind, kind.temp_base(), cancel, progress)
    }

    pub fn fetch_temp_patch(
        &mut self,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<Patch, ClientError> {
        self.fetch_patch(TEMP_PATCH, cancel, progress)
    }

    /// Read every User Performance (`kind` Performance, 32 slots) or User Rhythm Set (`kind` Rhythm, 2 slots), in slot
    /// order. A backup must be whole, so the first slot that cannot be read ends the run with its error (a cancel is
    /// `Cancelled`). Nothing is written to the instrument.
    pub fn fetch_user_documents(
        &mut self,
        kind: DocKind,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<Vec<(u8, Document)>, ClientError> {
        let (slots, what): (u8, &'static str) = match kind {
            DocKind::Performance => (kind.user_slots(), "user performances"),
            DocKind::Rhythm => (kind.user_slots(), "user rhythm sets"),
            DocKind::System => return Ok(Vec::new()),
        };
        let mut out = Vec::with_capacity(usize::from(slots));
        for slot in 0..slots {
            if cancel.is_cancelled() {
                return Err(ClientError::Cancelled);
            }
            progress(Progress {
                what,
                done: usize::from(slot),
                total: usize::from(slots),
            });
            let base = match kind {
                DocKind::Performance => user_performance(slot)?,
                _ => user_rhythm(slot)?,
            };
            out.push((slot, self.fetch_document(kind, base, cancel, &mut |_| {})?));
        }
        progress(Progress {
            what,
            done: usize::from(slots),
            total: usize::from(slots),
        });
        Ok(out)
    }

    /// Read User patch `slot` (0 to 127).
    pub fn fetch_user_patch(
        &mut self,
        slot: u8,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<Patch, ClientError> {
        self.fetch_patch(user_patch(slot)?, cancel, progress)
    }

    /// Read several User patches. A slot that fails is recorded and the next is tried; cancellation and transport
    /// failures stop the run. Nothing is written to the instrument.
    pub fn fetch_bank(
        &mut self,
        slots: impl IntoIterator<Item = u8>,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<BankFetch, ClientError> {
        let slots: Vec<u8> = slots.into_iter().collect();
        let total = slots.len();
        let mut out = BankFetch::default();
        let mut silent_in_a_row = 0;
        for (i, &slot) in slots.iter().enumerate() {
            if cancel.is_cancelled() {
                out.cancelled = true;
                break;
            }
            progress(Progress {
                what: "user patches",
                done: i,
                total,
            });
            match self.fetch_user_patch(slot, cancel, &mut |_| {}) {
                Ok(p) => {
                    silent_in_a_row = 0;
                    out.patches.push((slot, p));
                }
                Err(ClientError::Cancelled) => {
                    out.cancelled = true;
                    break;
                }
                Err(e @ ClientError::Transport(_)) => return Err(e),
                Err(e) => {
                    silent_in_a_row = if matches!(e, ClientError::Timeout { .. }) {
                        silent_in_a_row + 1
                    } else {
                        0
                    };
                    out.failures.push((slot, e));
                    if silent_in_a_row >= MAX_SILENT_SLOTS {
                        out.aborted = true;
                        break;
                    }
                }
            }
        }
        if !out.cancelled {
            progress(Progress {
                what: "user patches",
                done: total,
                total,
            });
        }
        Ok(out)
    }

    /// Read the temporary patch's 12-character name once, without retries (a cheap "is anyone there" check).
    pub fn probe(&mut self, cancel: &CancelToken) -> Result<ProbeInfo, ClientError> {
        let size = Address::from_compact(NAME_LEN as u32)?;
        let expect = [Expect {
            address: TEMP_PATCH,
            len: NAME_LEN,
        }];
        let timeout = self.cfg.probe_timeout;
        let pieces = self.request(TEMP_PATCH, size, &expect, cancel, 0, timeout)?;
        let mut name = [b'?'; NAME_LEN];
        for (address, data) in pieces {
            let rel = address.checked_sub(TEMP_PATCH)?.compact() as usize;
            for (i, &b) in data.iter().enumerate() {
                if let Some(slot) = name.get_mut(rel + i) {
                    *slot = if (0x20..0x7F).contains(&b) { b } else { b'?' };
                }
            }
        }
        Ok(ProbeInfo {
            device: self.cfg.device,
            patch_name: String::from_utf8_lossy(&name).trim_end().to_owned(),
        })
    }

    /// Try every device ID (`0x10..=0x1F`, shown 17 to 32) until one answers. The client keeps the ID that answered; if
    /// none does, the previous ID is restored. Only RQ1 reads are sent.
    pub fn scan_device_ids(
        &mut self,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<Option<ProbeInfo>, ClientError> {
        let previous = self.cfg.device;
        for (i, byte) in (0x10u8..=0x1F).enumerate() {
            progress(Progress {
                what: "device IDs",
                done: i,
                total: 16,
            });
            let Ok(id) = DeviceId::new(byte) else {
                continue;
            };
            self.cfg.device = id;
            match self.probe(cancel) {
                Ok(info) => return Ok(Some(info)),
                Err(ClientError::Timeout { .. }) => {}
                Err(e) => {
                    self.cfg.device = previous;
                    return Err(e);
                }
            }
        }
        self.cfg.device = previous;
        Ok(None)
    }

    /// Wait until `min_dt1_gap` (and, when set, the byte-rate interval) has passed since the previous DT1.
    fn pace(&mut self) {
        if let Some(last) = self.last_dt1 {
            let since = self.port.now().saturating_sub(last);
            let mut need = self.cfg.min_dt1_gap;
            if let (Some(rate), Some(bytes)) = (self.cfg.byte_rate, self.last_dt1_data) {
                // Patch Base: interval ≈ chunkSize / bytesPerSecond (B-05).
                let nanos = (u128::from(bytes as u64) * 1_000_000_000u128) / u128::from(rate);
                need = need.max(Duration::from_nanos(nanos as u64));
            }
            if since < need {
                self.port.sleep(need - since);
            }
        }
    }

    fn mark_dt1_sent(&mut self, data_bytes: usize) {
        self.last_dt1 = Some(self.port.now());
        self.last_dt1_data = Some(data_bytes);
    }

    /// Write `patch` into the instrument's **temporary** patch area (audible now, lost on the next sound change),
    /// splitting blocks into 128-byte DT1 messages at least `min_dt1_gap` apart. With `verify` the area is read back
    /// and compared; a difference is an error naming the blocks. Refuses patches with invalid values and any message
    /// that would reach permanent memory.
    pub fn send_temp_patch(
        &mut self,
        patch: &Patch,
        verify: bool,
        cancel: &CancelToken,
    ) -> Result<SendReport, ClientError> {
        let chunk = self.cfg.max_dt1_data.clamp(16, MAX_DT1_DATA_TO_DEVICE);
        let mut messages = Vec::new();
        for (address, data) in patch.to_dt1(TEMP_PATCH)? {
            messages.extend(split_dt1(address, &data, chunk)?);
        }
        for m in &messages {
            match m {
                Message::Dt1 { address, data } => {
                    if classify_range(*address, data.len()) != WriteKind::Temporary {
                        return Err(ClientError::Refused(format!(
                            "{} byte(s) at {address} are not in temporary memory",
                            data.len()
                        )));
                    }
                }
                Message::Rq1 { .. } => {
                    return Err(ClientError::Refused(
                        "a read request in a write batch".into(),
                    ));
                }
            }
        }
        let bytes = messages
            .iter()
            .map(|m| match m {
                Message::Dt1 { data, .. } => data.len(),
                Message::Rq1 { .. } => 0,
            })
            .sum();
        let count = messages.len();
        for m in &messages {
            if cancel.is_cancelled() {
                return Err(ClientError::Cancelled);
            }
            let data_len = match m {
                Message::Dt1 { data, .. } => data.len(),
                Message::Rq1 { .. } => 0,
            };
            let frame = m.encode(self.cfg.device)?;
            self.pace();
            self.port.send(&frame)?;
            self.mark_dt1_sent(data_len);
        }
        let verified = if verify {
            // Soft Thru or a loopback can echo our own writes back slowly (about 0.3 ms per byte on a DIN cable);
            // listen long enough that an echo cannot be mistaken for the instrument's read-back.
            let echo_window = self.cfg.verify_settle + Duration::from_micros(400 * bytes as u64);
            self.poll(echo_window)?;
            let back = self.fetch_temp_patch(cancel, &mut |_| {})?;
            let blocks = differing_blocks(patch, &back);
            if !blocks.is_empty() {
                return Err(ClientError::Mismatch { blocks });
            }
            true
        } else {
            false
        };
        Ok(SendReport {
            messages: count,
            bytes,
            verified,
        })
    }

    /// Read all 128 User patches and plan a write for each. Every slot must read cleanly: a bank is never overwritten
    /// when part of it could not be backed up. Nothing is written.
    pub fn plan_user_bank(
        &mut self,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<Vec<WritePlan>, ClientError> {
        let bank = self.fetch_bank(0..128, cancel, progress)?;
        if bank.cancelled {
            return Err(ClientError::Cancelled);
        }
        if let Some((slot, _)) = bank.failures.into_iter().next() {
            return Err(ClientError::Refused(format!(
                "User patch {} could not be read; the bank was not backed up. Check the cable and try again.",
                u16::from(slot) + 1
            )));
        }
        Ok(bank
            .patches
            .into_iter()
            .map(|(slot, backup)| WritePlan { slot, backup })
            .collect())
    }

    /// Write `patches` (slot, patch) into the slots of `plans`, one at a time with every safeguard of
    /// [`Client::commit_user_write`] (fresh read, writes inside the slot, read-back). Stops at the first failure.
    pub fn commit_user_bank(
        &mut self,
        plans: &[WritePlan],
        patches: &[(u8, Patch)],
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<BankWriteReport, ClientError> {
        let mut report = BankWriteReport::default();
        let total = patches.len();
        // Reject the whole call before the first write if it makes no sense.
        let mut seen = std::collections::BTreeSet::new();
        for (slot, _) in patches {
            if !seen.insert(*slot) {
                return Err(ClientError::Refused(format!(
                    "slot {} appears twice in the bank write",
                    u16::from(*slot) + 1
                )));
            }
            if !plans.iter().any(|p| p.slot == *slot) {
                return Err(ClientError::Refused(format!(
                    "no backup was taken for slot {}",
                    u16::from(*slot) + 1
                )));
            }
        }
        for (i, (slot, patch)) in patches.iter().enumerate() {
            if cancel.is_cancelled() {
                report.cancelled = true;
                break;
            }
            progress(Progress {
                what: "writing user patches",
                done: i,
                total,
            });
            let Some(plan) = plans.iter().find(|p| p.slot == *slot) else {
                report.stopped_at = Some((*slot, "no backup was taken for this slot".to_owned()));
                break;
            };
            match self.commit_user_write(plan, patch, cancel) {
                Ok(_) => report.written.push(*slot),
                Err(ClientError::Cancelled) => {
                    report.cancelled = true;
                    break;
                }
                Err(e) => {
                    // Whatever the cause (even a lost cable), say which slot it stopped at and keep what was written.
                    report.stopped_at = Some((*slot, e.to_string()));
                    break;
                }
            }
        }
        progress(Progress {
            what: "writing user patches",
            done: report.written.len(),
            total,
        });
        Ok(report)
    }

    /// First step of writing into User memory: read the slot that would be overwritten. Nothing is written.
    pub fn plan_user_write(
        &mut self,
        slot: u8,
        cancel: &CancelToken,
    ) -> Result<WritePlan, ClientError> {
        let backup = self.fetch_user_patch(slot, cancel, &mut |_| {})?;
        Ok(WritePlan { slot, backup })
    }

    /// Write `patch` into the User slot named by `plan` (the **permanent** write), then read the slot back and compare.
    /// The slot is read again first and must still hold what the plan backed up; if it changed since (a panel edit,
    /// another program), nothing is written and the plan is stale. Every message must lie inside that slot's blocks. A
    /// difference after the write is an error naming the blocks (typically User Memory Protect is on, or the instrument
    /// dropped data).
    pub fn commit_user_write(
        &mut self,
        plan: &WritePlan,
        patch: &Patch,
        cancel: &CancelToken,
    ) -> Result<SendReport, ClientError> {
        // Refuse an invalid patch before any reading or writing.
        patch.to_dt1(user_patch(plan.slot)?)?;
        let now = self.fetch_user_patch(plan.slot, cancel, &mut |_| {})?;
        if now != plan.backup {
            return Err(ClientError::Refused(format!(
                "User patch {} changed since it was backed up; arm the save again",
                u16::from(plan.slot) + 1
            )));
        }
        self.write_user_slot(plan.slot, patch, cancel)
    }

    /// Put back what the plan saved from the slot. This is a permanent write too: callers must have decided it is
    /// wanted (the application checks that the slot still holds what it wrote).
    pub fn restore_user_backup(
        &mut self,
        plan: &WritePlan,
        cancel: &CancelToken,
    ) -> Result<SendReport, ClientError> {
        let backup = plan.backup.clone();
        self.write_user_slot(plan.slot, &backup, cancel)
    }

    /// First step of writing the System settings: read them. Nothing is written.
    pub fn plan_system_write(
        &mut self,
        cancel: &CancelToken,
    ) -> Result<SystemWritePlan, ClientError> {
        let backup = self.fetch_document(DocKind::System, SYSTEM, cancel, &mut |_| {})?;
        Ok(SystemWritePlan { backup })
    }

    /// Write `doc` as the System settings (the **permanent** write), then read them back and compare. The settings are
    /// read again first and must still hold what the plan backed up; if they changed since (a panel edit, another
    /// program), nothing is written and the plan is stale. Every message must lie inside the System region.
    pub fn commit_system_write(
        &mut self,
        plan: &SystemWritePlan,
        doc: &Document,
        cancel: &CancelToken,
    ) -> Result<SendReport, ClientError> {
        if doc.kind() != DocKind::System {
            return Err(ClientError::Refused(
                "only System settings can be written here".into(),
            ));
        }
        if let Some((block, _)) = doc.validate().into_iter().next() {
            return Err(ClientError::Refused(format!(
                "A System setting in {block} is out of range."
            )));
        }
        let now = self.fetch_document(DocKind::System, SYSTEM, cancel, &mut |_| {})?;
        if now != plan.backup {
            return Err(ClientError::Refused(
                "the System settings changed since they were backed up; arm the save again".into(),
            ));
        }
        self.write_system(doc, cancel)
    }

    /// Put back what the plan saved. This is a permanent write too: callers must have decided it is wanted (the
    /// application checks that the settings still hold what it wrote).
    pub fn restore_system_backup(
        &mut self,
        plan: &SystemWritePlan,
        cancel: &CancelToken,
    ) -> Result<SendReport, ClientError> {
        let backup = plan.backup.clone();
        self.write_system(&backup, cancel)
    }

    /// Read the System settings (used by callers that need to compare them with something they wrote).
    pub fn read_system(&mut self, cancel: &CancelToken) -> Result<Document, ClientError> {
        self.fetch_document(DocKind::System, SYSTEM, cancel, &mut |_| {})
    }

    fn write_system(
        &mut self,
        doc: &Document,
        cancel: &CancelToken,
    ) -> Result<SendReport, ClientError> {
        let mut messages = Vec::new();
        for (address, data) in doc.to_dt1(SYSTEM)? {
            messages.extend(split_dt1(address, &data, MAX_DT1_DATA_TO_DEVICE)?);
        }
        for m in &messages {
            match m {
                Message::Dt1 { address, data } => {
                    let lanes = address.bytes();
                    if classify_range(*address, data.len()) != WriteKind::Permanent
                        || lanes[0] != 0x00
                        || lanes[1] != 0x00
                    {
                        return Err(ClientError::Refused(format!(
                            "{} byte(s) at {address} are not inside the System settings",
                            data.len()
                        )));
                    }
                }
                Message::Rq1 { .. } => {
                    return Err(ClientError::Refused(
                        "a read request in a write batch".into(),
                    ));
                }
            }
        }
        let bytes: usize = messages
            .iter()
            .map(|m| match m {
                Message::Dt1 { data, .. } => data.len(),
                Message::Rq1 { .. } => 0,
            })
            .sum();
        // Once the first message is sent the settings are written to the end and read back: cancelling part way would
        // leave half-written settings nobody is told about. Cancel is honoured before the first message only.
        if cancel.is_cancelled() {
            return Err(ClientError::Cancelled);
        }
        for m in &messages {
            let data_len = match m {
                Message::Dt1 { data, .. } => data.len(),
                Message::Rq1 { .. } => 0,
            };
            let frame = m.encode(self.cfg.device)?;
            self.pace();
            self.port.send(&frame)?;
            self.mark_dt1_sent(data_len);
        }
        let echo_window = self.cfg.verify_settle + Duration::from_micros(400 * bytes as u64);
        self.poll(echo_window)?;
        let back =
            self.fetch_document(DocKind::System, SYSTEM, &CancelToken::new(), &mut |_| {})?;
        let blocks = differing_document_blocks(doc, &back);
        if !blocks.is_empty() {
            return Err(ClientError::Mismatch { blocks });
        }
        Ok(SendReport {
            messages: messages.len(),
            bytes,
            verified: true,
        })
    }

    /// Read User patch `slot` (used by callers that need to compare it with something they wrote).
    pub fn read_user_slot(&mut self, slot: u8, cancel: &CancelToken) -> Result<Patch, ClientError> {
        self.fetch_user_patch(slot, cancel, &mut |_| {})
    }

    /// Write a User Performance or User Rhythm Set to `slot`, then read it back and compare. Permanent write.
    /// `doc.kind()` must be Performance or Rhythm and match `kind`. Used to prove exported `.syx` files round-trip
    /// (hardware ledger H-066).
    pub fn write_user_document(
        &mut self,
        kind: DocKind,
        slot: u8,
        doc: &Document,
        cancel: &CancelToken,
    ) -> Result<SendReport, ClientError> {
        if doc.kind() != kind {
            return Err(ClientError::Refused(
                "document kind does not match the User memory region".into(),
            ));
        }
        if kind != DocKind::Performance && kind != DocKind::Rhythm {
            return Err(ClientError::Refused(
                "only User Performances and User Rhythm Sets can be written here".into(),
            ));
        }
        if slot >= kind.user_slots() {
            return Err(ClientError::Refused(format!(
                "User {} slot must be 0 to {}",
                match kind {
                    DocKind::Performance => "Performance",
                    DocKind::Rhythm => "Rhythm Set",
                    DocKind::System => "System",
                },
                kind.user_slots().saturating_sub(1)
            )));
        }
        if let Some((block, _)) = doc.validate().into_iter().next() {
            return Err(ClientError::Refused(format!(
                "A value in {block} is out of range."
            )));
        }
        let base = match kind {
            DocKind::Performance => user_performance(slot)?,
            DocKind::Rhythm => user_rhythm(slot)?,
            DocKind::System => {
                return Err(ClientError::Refused(
                    "System settings use the System write path".into(),
                ));
            }
        };
        let mut messages = Vec::new();
        for (address, data) in doc.to_dt1(base)? {
            messages.extend(split_dt1(address, &data, MAX_DT1_DATA_TO_DEVICE)?);
        }
        for m in &messages {
            match m {
                Message::Dt1 { address, data } => {
                    if classify_range(*address, data.len()) != WriteKind::Permanent
                        || !inside_document_blocks(kind, base, *address, data.len())
                    {
                        return Err(ClientError::Refused(format!(
                            "{} byte(s) at {address} are not inside User {:?} slot {}",
                            data.len(),
                            kind,
                            u16::from(slot) + 1
                        )));
                    }
                }
                Message::Rq1 { .. } => {
                    return Err(ClientError::Refused(
                        "a read request in a write batch".into(),
                    ));
                }
            }
        }
        let bytes: usize = messages
            .iter()
            .map(|m| match m {
                Message::Dt1 { data, .. } => data.len(),
                Message::Rq1 { .. } => 0,
            })
            .sum();
        if cancel.is_cancelled() {
            return Err(ClientError::Cancelled);
        }
        for m in &messages {
            let data_len = match m {
                Message::Dt1 { data, .. } => data.len(),
                Message::Rq1 { .. } => 0,
            };
            let frame = m.encode(self.cfg.device)?;
            self.pace();
            self.port.send(&frame)?;
            self.mark_dt1_sent(data_len);
        }
        let echo_window = self.cfg.verify_settle + Duration::from_micros(400 * bytes as u64);
        self.poll(echo_window)?;
        let back = self.fetch_document(kind, base, &CancelToken::new(), &mut |_| {})?;
        let blocks = differing_document_blocks(doc, &back);
        if !blocks.is_empty() {
            return Err(ClientError::Mismatch { blocks });
        }
        Ok(SendReport {
            messages: messages.len(),
            bytes,
            verified: true,
        })
    }

    fn write_user_slot(
        &mut self,
        slot: u8,
        patch: &Patch,
        cancel: &CancelToken,
    ) -> Result<SendReport, ClientError> {
        let base = user_patch(slot)?;
        let mut messages = Vec::new();
        for (address, data) in patch.to_dt1(base)? {
            messages.extend(split_dt1(address, &data, MAX_DT1_DATA_TO_DEVICE)?);
        }
        for m in &messages {
            match m {
                Message::Dt1 { address, data } => {
                    if classify_range(*address, data.len()) != WriteKind::Permanent
                        || !inside_patch_blocks(base, *address, data.len())
                    {
                        return Err(ClientError::Refused(format!(
                            "{} byte(s) at {address} are not inside User patch {}",
                            data.len(),
                            u16::from(slot) + 1
                        )));
                    }
                }
                Message::Rq1 { .. } => {
                    return Err(ClientError::Refused(
                        "a read request in a write batch".into(),
                    ));
                }
            }
        }
        let bytes: usize = messages
            .iter()
            .map(|m| match m {
                Message::Dt1 { data, .. } => data.len(),
                Message::Rq1 { .. } => 0,
            })
            .sum();
        // Once the first message of a slot is sent the slot is written to the end and read back: cancelling part way
        // would leave a half-written patch nobody is told about. Cancel is honoured before a slot, never inside one.
        if cancel.is_cancelled() {
            return Err(ClientError::Cancelled);
        }
        for m in &messages {
            let data_len = match m {
                Message::Dt1 { data, .. } => data.len(),
                Message::Rq1 { .. } => 0,
            };
            let frame = m.encode(self.cfg.device)?;
            self.pace();
            self.port.send(&frame)?;
            self.mark_dt1_sent(data_len);
        }
        // Soft Thru or a loopback can echo our own writes back slowly; listen long enough that an echo cannot be
        // mistaken for the instrument's read-back (an ignored write would otherwise look verified).
        let echo_window = self.cfg.verify_settle + Duration::from_micros(400 * bytes as u64);
        self.poll(echo_window)?;
        let back = self.fetch_user_patch(slot, &CancelToken::new(), &mut |_| {})?;
        let blocks = differing_blocks(patch, &back);
        if !blocks.is_empty() {
            return Err(ClientError::Mismatch { blocks });
        }
        Ok(SendReport {
            messages: messages.len(),
            bytes,
            verified: true,
        })
    }

    /// Play or release a note on the instrument (Note On / Note Off on `channel` 0 to 15). Notes make sound but write
    /// nothing, so they are always allowed; out-of-range values are refused rather than clipped.
    pub fn send_note(
        &mut self,
        channel: u8,
        note: u8,
        velocity: u8,
        on: bool,
    ) -> Result<(), ClientError> {
        if channel > 15 || note > 127 || velocity > 127 {
            return Err(ClientError::Refused(
                "channel must be 0 to 15, note and velocity 0 to 127".into(),
            ));
        }
        let status = if on { 0x90 } else { 0x80 } | channel;
        let velocity = if on { velocity.max(1) } else { velocity };
        Ok(self.port.send(&[status, note, velocity])?)
    }

    /// Ask the instrument to select a sound the way an external keyboard does: Bank Select MSB (controller 0), Bank Select
    /// LSB (controller 32), then Program Change, all on `channel` 0 to 15. It writes nothing to memory, but the instrument
    /// replaces its temporary sound with the one chosen, and it only listens when its Receive Program Change and Bank
    /// Select switches are on. Out-of-range values are refused rather than clipped.
    pub fn select_sound(
        &mut self,
        channel: u8,
        msb: u8,
        lsb: u8,
        program: u8,
    ) -> Result<(), ClientError> {
        if channel > 15 || msb > 127 || lsb > 127 || program > 127 {
            return Err(ClientError::Refused(
                "channel must be 0 to 15, bank select and program 0 to 127".into(),
            ));
        }
        self.port.send(&[0xB0 | channel, 0, msb])?;
        self.port.send(&[0xB0 | channel, 32, lsb])?;
        Ok(self.port.send(&[0xC0 | channel, program])?)
    }

    /// Silence everything on `channel` (All Notes Off, controller 123).
    pub fn all_notes_off(&mut self, channel: u8) -> Result<(), ClientError> {
        if channel > 15 {
            return Err(ClientError::Refused("channel must be 0 to 15".into()));
        }
        Ok(self.port.send(&[0xB0 | channel, 123, 0])?)
    }

    /// Send one already-encoded DT1 frame (a live edit from the editor) to the **temporary** patch. It must parse as a
    /// DT1 for this client's device ID, carry at most 128 data bytes, and lie entirely in temporary memory; anything
    /// else is refused and nothing is sent. Paced like every other write.
    pub fn send_temp_frame(
        &mut self,
        frame: &[u8],
        cancel: &CancelToken,
    ) -> Result<(), ClientError> {
        if cancel.is_cancelled() {
            return Err(ClientError::Cancelled);
        }
        let parsed = Message::parse(frame).map_err(|_| {
            ClientError::Refused("That MIDI message is not valid SysEx for the XP-60.".into())
        })?;
        if parsed.device != self.cfg.device {
            return Err(ClientError::Refused(
                "That message is for a different Device ID.".into(),
            ));
        }
        let data_len = match &parsed.message {
            Message::Dt1 { address, data } => {
                if data.len() > MAX_DT1_DATA_TO_DEVICE {
                    return Err(ClientError::Refused(
                        "That message is larger than the XP-60 can accept in one write.".into(),
                    ));
                }
                if classify_range(*address, data.len()) != WriteKind::Temporary {
                    return Err(ClientError::Refused(
                        "That write is not aimed at temporary memory.".into(),
                    ));
                }
                if !inside_temp_patch_block(*address, data.len()) {
                    return Err(ClientError::Refused(
                        "That write is not inside one block of the temporary patch.".into(),
                    ));
                }
                data.len()
            }
            Message::Rq1 { .. } => {
                return Err(ClientError::Refused(
                    "A read request is not a write.".into(),
                ));
            }
        };
        self.pace();
        self.port.send(frame)?;
        self.mark_dt1_sent(data_len);
        Ok(())
    }
}

/// True when `len` bytes at `address` lie wholly inside one known block of a temporary area: the temporary patch
/// (Common or one Tone), the temporary performance, or the temporary rhythm setup. Live edits are held to this so
/// nothing is ever written to a gap or an unmapped part of temporary memory.
fn inside_temp_patch_block(address: Address, len: usize) -> bool {
    let within = |base: Address, spans: Vec<(u32, usize)>| -> bool {
        let Ok(rel) = address.checked_sub(base) else {
            return false;
        };
        let (rel, len) = (u64::from(rel.compact()), len as u64);
        spans.into_iter().any(|(start, size)| {
            rel >= u64::from(start) && rel + len <= u64::from(start) + size as u64
        })
    };
    let mut patch = vec![(0u32, 73usize)];
    patch.extend(PATCH_TONE_OFFSETS.iter().map(|o| (o.compact(), 129usize)));
    within(TEMP_PATCH, patch)
        || within(DocKind::Performance.temp_base(), DocKind::Performance.block_spans())
        // The Rhythm Setup's Common block is held back until its address is confirmed on hardware (ledger H-045).
        || within(
            DocKind::Rhythm.temp_base(),
            DocKind::Rhythm.block_spans().into_iter().skip(1).collect(),
        )
}

/// True when `len` bytes at `address` lie wholly inside one block (Common or a Tone) of the patch based at `base`.
fn inside_patch_blocks(base: Address, address: Address, len: usize) -> bool {
    let Ok(rel) = address.checked_sub(base) else {
        return false;
    };
    let (rel, len) = (u64::from(rel.compact()), len as u64);
    let mut spans = vec![(0u64, 73u64)];
    spans.extend(
        PATCH_TONE_OFFSETS
            .iter()
            .map(|o| (u64::from(o.compact()), 129u64)),
    );
    spans
        .into_iter()
        .any(|(start, size)| rel >= start && rel + len <= start + size)
}

/// True when `len` bytes at `address` lie wholly inside one known block of a User Performance or Rhythm at `base`.
fn inside_document_blocks(kind: DocKind, base: Address, address: Address, len: usize) -> bool {
    let Ok(rel) = address.checked_sub(base) else {
        return false;
    };
    let (rel, len) = (u64::from(rel.compact()), len as u64);
    kind.block_spans()
        .into_iter()
        .any(|(start, size)| rel >= u64::from(start) && rel + len <= u64::from(start) + size as u64)
}

fn differing_document_blocks(a: &Document, b: &Document) -> Vec<String> {
    a.blocks()
        .iter()
        .zip(b.blocks())
        .enumerate()
        .filter(|(_, (x, y))| x != y)
        .map(|(i, _)| {
            if i == 0 {
                "common".to_owned()
            } else {
                a.kind().item_label(i - 1)
            }
        })
        .collect()
}

fn differing_blocks(a: &Patch, b: &Patch) -> Vec<String> {
    let mut out = Vec::new();
    if a.common != b.common {
        out.push("common".to_owned());
    }
    for (i, (x, y)) in a.tones.iter().zip(&b.tones).enumerate() {
        if x != y {
            out.push(format!("tone{}", i + 1));
        }
    }
    out
}

enum Placed {
    New,
    Duplicate,
    Rejected,
}

/// Collects DT1 pieces and tracks which bytes of which expected blocks have arrived.
struct Collector<'a> {
    expect: &'a [Expect],
    covered: Vec<Vec<bool>>,
    pieces: Vec<(Address, Vec<u8>)>,
}

impl<'a> Collector<'a> {
    fn new(expect: &'a [Expect]) -> Self {
        Collector {
            expect,
            covered: expect.iter().map(|e| vec![false; e.len]).collect(),
            pieces: Vec::new(),
        }
    }

    /// Accept a piece that lies fully inside an expected block and covers bytes not seen before. Pieces that add
    /// nothing are dropped, so memory stays bounded by the expected sizes and repeats cannot extend a request.
    fn place(&mut self, address: Address, data: Vec<u8>) -> Placed {
        for (i, e) in self.expect.iter().enumerate() {
            let Ok(rel) = address.checked_sub(e.address) else {
                continue;
            };
            let rel = rel.compact() as usize;
            if rel < e.len {
                let take = data.len().min(e.len - rel);
                let data = data[..take].to_vec();
                let range = &mut self.covered[i][rel..rel + take];
                if range.iter().all(|&b| b) {
                    return Placed::Duplicate;
                }
                range.fill(true);
                self.pieces.push((address, data));
                return Placed::New;
            }
        }
        Placed::Rejected
    }

    fn is_complete(&self) -> bool {
        self.covered.iter().all(|c| c.iter().all(|&b| b))
    }

    fn missing(&self) -> Vec<Address> {
        self.expect
            .iter()
            .zip(&self.covered)
            .filter(|(_, c)| !c.iter().all(|&b| b))
            .map(|(e, _)| e.address)
            .collect()
    }

    fn into_pieces(self) -> Vec<(Address, Vec<u8>)> {
        self.pieces
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn addr(n: u32) -> Address {
        Address::from_compact(n).unwrap()
    }

    #[test]
    fn a_repeated_piece_adds_nothing_and_is_not_stored() {
        let expect = [Expect {
            address: addr(0x100),
            len: 4,
        }];
        let mut c = Collector::new(&expect);
        assert!(matches!(c.place(addr(0x100), vec![1, 2]), Placed::New));
        assert!(matches!(
            c.place(addr(0x100), vec![1, 2]),
            Placed::Duplicate
        ));
        assert_eq!(c.pieces.len(), 1);
        assert!(!c.is_complete());
        assert!(matches!(c.place(addr(0x102), vec![3, 4]), Placed::New));
        assert!(c.is_complete());
    }

    #[test]
    fn a_piece_running_past_the_block_is_clipped_not_dropped() {
        let expect = [Expect {
            address: addr(0x100),
            len: 3,
        }];
        let mut c = Collector::new(&expect);
        assert!(matches!(
            c.place(addr(0x100), vec![1, 2, 3, 4, 5]),
            Placed::New
        ));
        assert!(c.is_complete());
        assert_eq!(c.into_pieces(), vec![(addr(0x100), vec![1, 2, 3])]);
    }

    #[test]
    fn pieces_outside_every_block_are_rejected() {
        let expect = [Expect {
            address: addr(0x100),
            len: 3,
        }];
        let mut c = Collector::new(&expect);
        assert!(matches!(c.place(addr(0x200), vec![1]), Placed::Rejected));
        assert!(matches!(c.place(addr(0x0FF), vec![1]), Placed::Rejected));
    }
}
