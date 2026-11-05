//! A virtual XP-60 on a virtual clock, for deterministic tests (and for developing the UI without hardware).
//!
//! It models what the legacy hardware log and the manual establish, so the client is tested against those rules:
//!
//! - replies are whole blocks, one DT1 each, ascending by address, spaced about 40 ms apart after about 30 ms;
//! - an RQ1 that arrives while replies are still streaming is **dropped** (requests must not be pipelined);
//! - a DT1 that arrives less than 20 ms after the previous one is dropped; a DT1 with more than 128 data bytes is dropped;
//! - a request for a span returns only the populated blocks inside it (sparse replies);
//! - messages for another device ID are ignored.
//!
//! None of this is hardware-verified by *this* project; the real instrument is checked in the hardware phase
//! (`artifact/docs/xppatchstudio/07-hardware-acceptance.md`). Faults can be injected to exercise timeouts, retries and noise.

use crate::transport::{MidiTransport, TransportError};
use std::collections::{BTreeMap, VecDeque};
use std::time::Duration;
use xppatch_core::{Patch, PatchError, SyxImage};
use xppatch_sysex::address::Address;
use xppatch_sysex::memory::{user_patch, TEMP_PATCH};
use xppatch_sysex::message::{DeviceId, Message};
use xppatch_sysex::stream::frames;

/// Faults the virtual instrument can be told to produce.
#[derive(Debug, Clone, Default)]
pub struct Faults {
    /// Ignore the next N requests entirely (no reply at all).
    pub drop_next_requests: usize,
    /// Corrupt the checksum of the next N reply messages.
    pub corrupt_next_replies: usize,
    /// Interleave Active Sensing and a stray note-on before every reply.
    pub noise_before_each_reply: bool,
    /// Accept but ignore writes (what User Memory Protect looks like on the wire).
    pub ignore_writes: bool,
    /// Extra delay before the first reply of each request.
    pub extra_latency: Duration,
    /// Echo every received DT1 back out 100 ms later (Soft Thru or a loopback).
    pub echo_writes: bool,
}

/// Counters describing how the client behaved, as seen by the instrument.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct DeviceStats {
    pub requests_served: usize,
    /// RQ1 that arrived while the previous reply was still streaming.
    pub pipelined_requests_dropped: usize,
    pub writes_applied: usize,
    pub writes_ignored: usize,
    /// DT1 that arrived less than the minimum gap after the previous one.
    pub fast_writes_dropped: usize,
    /// DT1 with more than 128 data bytes.
    pub oversize_writes_dropped: usize,
}

pub struct VirtualXp60 {
    now: Duration,
    device: DeviceId,
    blocks: BTreeMap<u32, Vec<u8>>,
    queue: VecDeque<(Duration, Vec<u8>)>,
    busy_until: Duration,
    last_dt1: Option<Duration>,
    pub faults: Faults,
    pub latency: Duration,
    pub reply_gap: Duration,
    pub min_dt1_gap: Duration,
    /// Every message the client sent, with the virtual time it was sent.
    pub sent: Vec<(Duration, Vec<u8>)>,
    pub stats: DeviceStats,
}

impl VirtualXp60 {
    pub fn new(device: DeviceId) -> Self {
        VirtualXp60 {
            now: Duration::ZERO,
            device,
            blocks: BTreeMap::new(),
            queue: VecDeque::new(),
            busy_until: Duration::ZERO,
            last_dt1: None,
            faults: Faults::default(),
            latency: Duration::from_millis(30),
            reply_gap: Duration::from_millis(40),
            min_dt1_gap: Duration::from_millis(20),
            sent: Vec::new(),
            stats: DeviceStats::default(),
        }
    }

    /// Place a block of data in the instrument's memory.
    pub fn set_block(&mut self, address: Address, data: Vec<u8>) {
        self.blocks.insert(address.compact(), data);
    }

    pub fn block(&self, address: Address) -> Option<&[u8]> {
        self.blocks.get(&address.compact()).map(Vec::as_slice)
    }

    /// Install a patch's five blocks at `base`.
    pub fn set_patch(&mut self, base: Address, patch: &Patch) -> Result<(), PatchError> {
        for (address, data) in patch.to_dt1(base)? {
            self.set_block(address, data);
        }
        Ok(())
    }

    /// Install a Performance or Rhythm Set's blocks at `base`.
    pub fn set_document(
        &mut self,
        base: Address,
        doc: &xppatch_core::Document,
    ) -> Result<(), PatchError> {
        for (address, data) in doc.to_dt1(base)? {
            self.set_block(address, data);
        }
        Ok(())
    }

    pub fn set_temp_patch(&mut self, patch: &Patch) -> Result<(), PatchError> {
        self.set_patch(TEMP_PATCH, patch)
    }

    pub fn set_user_patch(&mut self, slot: u8, patch: &Patch) -> Result<(), PatchError> {
        self.set_patch(user_patch(slot)?, patch)
    }

    /// Load every DT1 of a `.syx` image as a block (for example the real User bank).
    pub fn load_image(&mut self, image: &SyxImage) {
        for r in &image.records {
            self.set_block(r.address, r.data.clone());
        }
    }

    /// Read the temporary patch back out of the instrument's memory.
    pub fn temp_patch(&self) -> Result<Patch, PatchError> {
        self.patch_at(TEMP_PATCH)
    }

    pub fn patch_at(&self, base: Address) -> Result<Patch, PatchError> {
        let pieces: Vec<(Address, Vec<u8>)> = self
            .blocks
            .iter()
            .filter(|(&k, _)| k >= base.compact() && k < base.compact() + 0x1800)
            .filter_map(|(&k, v)| Address::from_compact(k).ok().map(|a| (a, v.clone())))
            .collect();
        Patch::from_dt1(base, &pieces)
    }

    /// Change the device ID the instrument answers to.
    pub fn set_device(&mut self, device: DeviceId) {
        self.device = device;
    }

    /// The panel selected a patch: Bank Select (MSB, LSB) and Program Change arrive on MIDI OUT.
    pub fn panel_select(&mut self, channel: u8, bank_msb: u8, bank_lsb: u8, program: u8) {
        let at = self.now;
        self.enqueue(at, vec![0xB0 | channel, 0x00, bank_msb]);
        self.enqueue(at, vec![0xB0 | channel, 0x20, bank_lsb]);
        self.enqueue(at, vec![0xC0 | channel, program]);
    }

    /// A hardware controller moved: a Control Change arrives on MIDI OUT.
    pub fn control_change(&mut self, channel: u8, controller: u8, value: u8) {
        let at = self.now;
        self.enqueue(at, vec![0xB0 | channel, controller, value]);
    }

    /// Queue bytes to be delivered at virtual time `at`, keeping the queue ordered by time.
    fn enqueue(&mut self, at: Duration, bytes: Vec<u8>) {
        let pos = self
            .queue
            .iter()
            .position(|(t, _)| *t > at)
            .unwrap_or(self.queue.len());
        self.queue.insert(pos, (at, bytes));
    }

    fn handle(&mut self, device: DeviceId, message: Message) {
        if device != self.device {
            return; // other device IDs are ignored
        }
        match message {
            Message::Rq1 { address, size } => self.rq1(address.compact(), size.compact()),
            Message::Dt1 { address, data } => self.dt1(address.compact(), data),
        }
    }

    fn rq1(&mut self, start: u32, size: u32) {
        if self.now < self.busy_until {
            self.stats.pipelined_requests_dropped += 1;
            return;
        }
        if self.faults.drop_next_requests > 0 {
            self.faults.drop_next_requests -= 1;
            return;
        }
        let end = start.saturating_add(size);
        let mut replies = Vec::new();
        for (&bstart, data) in &self.blocks {
            let bend = bstart + data.len() as u32;
            if bend <= start {
                continue;
            }
            if bstart >= end {
                break;
            }
            let lo = bstart.max(start);
            let hi = bend.min(end);
            replies.push((
                lo,
                data[(lo - bstart) as usize..(hi - bstart) as usize].to_vec(),
            ));
        }
        if replies.is_empty() {
            return; // nothing lives there: the instrument stays silent
        }
        self.stats.requests_served += 1;
        let first = self.now + self.latency + self.faults.extra_latency;
        let mut at = first;
        for (i, (lo, data)) in replies.into_iter().enumerate() {
            at = first + self.reply_gap * i as u32;
            let Ok(address) = Address::from_compact(lo) else {
                continue;
            };
            let Ok(mut frame) = Message::Dt1 { address, data }.encode(self.device) else {
                continue;
            };
            if self.faults.corrupt_next_replies > 0 {
                self.faults.corrupt_next_replies -= 1;
                let n = frame.len();
                frame[n - 2] ^= 0x01;
            }
            if self.faults.noise_before_each_reply {
                self.enqueue(at, vec![0xFE]);
                self.enqueue(at, vec![0x90, 0x3C, 0x40]);
            }
            self.enqueue(at, frame);
        }
        self.busy_until = at;
    }

    fn dt1(&mut self, start: u32, data: Vec<u8>) {
        let too_fast = self
            .last_dt1
            .is_some_and(|last| self.now.saturating_sub(last) < self.min_dt1_gap);
        self.last_dt1 = Some(self.now);
        if too_fast {
            self.stats.fast_writes_dropped += 1;
            return;
        }
        if data.len() > 128 {
            self.stats.oversize_writes_dropped += 1;
            return;
        }
        if self.faults.ignore_writes {
            self.stats.writes_ignored += 1;
            return;
        }
        let Some((&bstart, block)) = self.blocks.range_mut(..=start).next_back() else {
            self.stats.writes_ignored += 1;
            return;
        };
        let offset = (start - bstart) as usize;
        if offset >= block.len() {
            self.stats.writes_ignored += 1;
            return;
        }
        let n = data.len().min(block.len() - offset);
        block[offset..offset + n].copy_from_slice(&data[..n]);
        self.stats.writes_applied += 1;
    }
}

impl MidiTransport for VirtualXp60 {
    fn send(&mut self, bytes: &[u8]) -> Result<(), TransportError> {
        self.sent.push((self.now, bytes.to_vec()));
        if self.faults.echo_writes && bytes.get(4) == Some(&0x12) {
            let at = self.now + Duration::from_millis(100);
            self.enqueue(at, bytes.to_vec());
        }
        for frame in frames(bytes) {
            if let Ok(parsed) = Message::parse(&frame) {
                self.handle(parsed.device, parsed.message);
            }
        }
        Ok(())
    }

    fn recv(&mut self, timeout: Duration) -> Result<Option<Vec<u8>>, TransportError> {
        let deadline = self.now + timeout;
        match self.queue.front() {
            Some(&(at, _)) if at <= deadline => {
                self.now = self.now.max(at);
                Ok(self.queue.pop_front().map(|(_, bytes)| bytes))
            }
            _ => {
                self.now = deadline;
                Ok(None)
            }
        }
    }

    fn now(&self) -> Duration {
        self.now
    }

    fn sleep(&mut self, duration: Duration) {
        self.now += duration;
    }
}
