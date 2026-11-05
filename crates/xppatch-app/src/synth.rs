//! The one connection to the instrument, shared by the Tauri commands. No Tauri types here, so it is tested natively
//! against the virtual XP-60.

use serde::Serialize;
use std::path::{Path, PathBuf};
use std::time::Duration;
use std::time::Instant;
use thiserror::Error;
use xppatch_core::{DocKind, Patch};
use xppatch_midi::{
    CancelToken, Client, ClientConfig, ClientError, ControlSource, MidiTransport, Progress,
    WritePlan,
};
use xppatch_sysex::message::{DeviceId, ParseError};

/// Bytes in a patch's five blocks: Common 73 and four Tones of 129.
pub const PATCH_BYTES: usize = xppatch_core::Patch::BLOCK_BYTES;

#[derive(Debug, Error)]
pub enum SynthError {
    #[error("not connected to an instrument")]
    NotConnected,
    #[error(transparent)]
    Client(#[from] ClientError),
    #[error("unknown document kind '{0}'")]
    Kind(String),
    #[error("invalid device ID: {0}")]
    Device(#[from] ParseError),
    #[error("saving is not armed: choose a slot and arm the save first")]
    NotArmed,
    #[error("no earlier save to undo")]
    NothingToRestore,
    #[error("the arming expired; arm the save again")]
    ArmingExpired,
    #[error("the instrument is busy with a long operation; wait or cancel it")]
    Busy,
    #[error("the bank write does not match what was confirmed: {0}")]
    NotConfirmed(String),
    #[error("User patch {slot} no longer holds what was saved; it was not put back. The earlier patch is in {backup}")]
    SlotChanged { slot: u16, backup: String },
    #[error("could not write the backup file ({0}); nothing was changed on the instrument")]
    Backup(String),
    #[error("invalid patch data: {0}")]
    BadPatch(String),
}

impl Serialize for SynthError {
    fn serialize<S: serde::Serializer>(&self, s: S) -> Result<S::Ok, S::Error> {
        s.serialize_str(&self.to_string())
    }
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct Connected {
    /// Device ID as shown on the instrument (17 to 32).
    pub device_number: u8,
    /// The raw ID used in messages (0x10 to 0x1F); this is what the editor must put in its frames.
    pub device_id: u8,
    pub patch_name: String,
}

/// Create a new backup file `<stem>.syx`, or `<stem>-2.syx` and so on if that exists. An earlier backup is never overwritten.
fn create_backup_file(dir: &Path, stem: &str) -> Result<(PathBuf, std::fs::File), SynthError> {
    for n in 1..=99 {
        let name = if n == 1 {
            format!("{stem}.syx")
        } else {
            format!("{stem}-{n}.syx")
        };
        let path = dir.join(name);
        match std::fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&path)
        {
            Ok(file) => return Ok((path, file)),
            Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => {}
            Err(e) => return Err(SynthError::Backup(e.to_string())),
        }
    }
    Err(SynthError::Backup(
        "too many backup files with this name".to_owned(),
    ))
}

/// An open connection. `T` is the real MIDI transport in the app and the virtual XP-60 in tests.
pub struct Synth<T: MidiTransport> {
    client: Client<T>,
    /// The planned save, if any, and whether it is armed. A save is armed by reading the slot and writing its backup to
    /// disk, is used by exactly one commit, and is then disarmed (the plan stays so the save can be undone).
    save: Option<(WritePlan, bool, Instant)>,
    /// The last save attempted, kept separately from arming so arming another slot cannot lose the way back.
    last: Option<Attempt>,
    last_backup_path: Option<String>,
    /// A planned whole-bank write (every slot read and backed up), armed or not.
    bank: Option<(Vec<WritePlan>, bool, Instant, Vec<u8>)>,
}

/// A save that was started: what was meant to be written, what it replaced, and where that was backed up.
struct Attempt {
    plan: WritePlan,
    wrote: Patch,
    backup_path: String,
    /// The write did not verify, so the slot may hold a partial patch.
    failed: bool,
}

/// How long an arming stays valid.
pub const ARMING_LIFETIME: std::time::Duration = std::time::Duration::from_secs(60);
/// A bank arming reads 128 slots, so it lasts longer; every slot is read again when it is written anyway.
pub const BANK_ARMING_LIFETIME: std::time::Duration = std::time::Duration::from_secs(300);

/// What arming a save found and kept.
/// A controller moved on the instrument or a controller keyboard (a mod wheel, a slider, a pedal).
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct ControlMove {
    pub channel: u8,
    /// "cc", "nrpn" or "rpn".
    pub kind: &'static str,
    /// The controller number (0..127) or the parameter number (0..16383).
    pub number: u16,
    pub value: u16,
    /// The largest value this source can carry (127 or 16383).
    pub max: u16,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct ArmedSave {
    /// 0 to 127 (shown as USER:001 to 128).
    pub slot: u8,
    /// The name of the patch that will be replaced.
    pub occupant_name: String,
    /// Where the replaced patch was saved before anything is written.
    pub backup_path: String,
}

/// What arming a bank write found and kept.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct ArmedBank {
    /// Where all 128 current patches were saved before anything is written.
    pub backup_path: String,
    /// The names of the 128 patches that are in the instrument now.
    pub names: Vec<String>,
}

/// What a bank write did.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct BankSaveReport {
    pub written: Vec<u8>,
    /// Where the write stopped (slot 0 to 127) and why, if it did.
    pub stopped_at: Option<(u8, String)>,
    pub cancelled: bool,
}

/// What a completed save did.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct SaveReport {
    pub slot: u8,
    pub messages: usize,
    pub bytes: usize,
    /// The slot was read back and equals what was sent.
    pub verified: bool,
}

impl<T: MidiTransport> Synth<T> {
    /// Take over `port`, find the device that answers (trying `device` first) and read its temporary patch's name.
    pub fn connect(port: T, device: u8) -> Result<(Synth<T>, Connected), SynthError> {
        let cfg = ClientConfig {
            device: DeviceId::new(device)?,
            ..ClientConfig::default()
        };
        let mut client = Client::new(port, cfg);
        let cancel = CancelToken::new();
        let info = match client.probe(&cancel) {
            Ok(info) => info,
            Err(ClientError::Timeout { .. }) => client
                .scan_device_ids(&cancel, &mut |_| {})?
                .ok_or(ClientError::Timeout {
                    attempts: 1,
                    missing: Vec::new(),
                })?,
            Err(e) => return Err(e.into()),
        };
        let connected = Connected {
            device_number: info.device.display_number(),
            device_id: info.device.byte(),
            patch_name: info.patch_name,
        };
        Ok((
            Synth {
                client,
                save: None,
                last: None,
                last_backup_path: None,
                bank: None,
            },
            connected,
        ))
    }

    /// Send one live-edit frame to the temporary patch. The client refuses anything that could reach permanent memory.
    pub fn send_frame(&mut self, frame: &[u8]) -> Result<(), SynthError> {
        Ok(self.client.send_temp_frame(frame, &CancelToken::new())?)
    }

    /// Read the temporary patch as Common (73 bytes) then Tone 1 to 4 (129 each).
    pub fn fetch_temp_patch(&mut self) -> Result<Vec<u8>, SynthError> {
        let patch = self
            .client
            .fetch_temp_patch(&CancelToken::new(), &mut |_| {})?;
        Ok(patch.to_blocks())
    }

    /// Step one of saving into a User slot: read what is there, write it to `backup_dir` as a `.syx` file (the slot is
    /// untouched until that file exists), and arm exactly one save. `stamp` names the file (seconds since the epoch).
    pub fn arm_user_save(
        &mut self,
        slot: u8,
        backup_dir: &Path,
        stamp: u64,
    ) -> Result<ArmedSave, SynthError> {
        self.save = None;
        self.bank = None;
        let plan = self.client.plan_user_write(slot, &CancelToken::new())?;
        let device = self.client.config().device;
        let bytes = xppatch_core::user_patch_bank_bytes(
            &[(slot, plan.backup().clone())],
            device,
            Some(128),
        )
        .map_err(|e| SynthError::Backup(e.to_string()))?;
        std::fs::create_dir_all(backup_dir).map_err(|e| SynthError::Backup(e.to_string()))?;
        let (path, mut file) = create_backup_file(
            backup_dir,
            &format!("{stamp}-user-{:03}", u16::from(slot) + 1),
        )?;
        let written = std::io::Write::write_all(&mut file, &bytes).and_then(|()| file.sync_all());
        drop(file);
        // The backup must read back as exactly the patch it replaces, or the save does not go ahead.
        let verified = written
            .map_err(|e| e.to_string())
            .and_then(|()| std::fs::read(&path).map_err(|e| e.to_string()))
            .and_then(|raw| {
                let image = xppatch_core::SyxImage::parse(&raw).map_err(|e| e.to_string())?;
                let slots = image.user_patches().map_err(|e| e.to_string())?;
                match slots.as_slice() {
                    [(s, p)] if *s == slot && p == plan.backup() => Ok(()),
                    _ => Err("the backup file does not match what it should hold".to_owned()),
                }
            });
        if let Err(e) = verified {
            let _ = std::fs::remove_file(&path);
            return Err(SynthError::Backup(e));
        }
        let info = ArmedSave {
            slot,
            occupant_name: plan.occupant_name(),
            backup_path: path.display().to_string(),
        };
        self.last_backup_path = Some(info.backup_path.clone());
        self.save = Some((plan, true, Instant::now()));
        Ok(info)
    }

    /// Cancel an armed save without writing anything.
    pub fn disarm_user_save(&mut self) {
        if let Some((_, armed, _)) = self.save.as_mut() {
            *armed = false;
        }
    }

    /// Step two: write `patch_bytes` (590 bytes of patch blocks) into the armed slot and read it back. The save is
    /// disarmed first, whatever happens, so one arming allows one write; an arming older than a minute is refused, and
    /// the slot is read again and must still hold what was backed up.
    pub fn commit_user_save(&mut self, patch_bytes: &[u8]) -> Result<SaveReport, SynthError> {
        let (plan, armed, since) = self.save.as_mut().ok_or(SynthError::NotArmed)?;
        if !std::mem::replace(armed, false) {
            return Err(SynthError::NotArmed);
        }
        if since.elapsed() > ARMING_LIFETIME {
            return Err(SynthError::ArmingExpired);
        }
        let plan = plan.clone();
        let patch =
            Patch::from_blocks(patch_bytes).map_err(|e| SynthError::BadPatch(e.to_string()))?;
        let backup_path = self.last_backup_path.clone().unwrap_or_default();
        let result = self
            .client
            .commit_user_write(&plan, &patch, &CancelToken::new());
        // Remember the attempt unless nothing could have been written (a refusal or a read failure before any write).
        let wrote_something = !matches!(
            result,
            Err(ClientError::Refused(_)
                | ClientError::Timeout { .. }
                | ClientError::Cancelled
                | ClientError::Patch(_))
        );
        if wrote_something {
            self.last = Some(Attempt {
                plan: plan.clone(),
                wrote: patch,
                backup_path,
                failed: result.is_err(),
            });
        }
        let report = result?;
        Ok(SaveReport {
            slot: plan.slot(),
            messages: report.messages,
            bytes: report.bytes,
            verified: report.verified,
        })
    }

    /// Put back what the last save replaced, once. It is refused if the slot no longer holds what that save wrote (a
    /// later change would be lost), unless the save itself failed part way and the slot may hold a mix.
    pub fn restore_user_backup(&mut self) -> Result<SaveReport, SynthError> {
        let attempt = self.last.as_ref().ok_or(SynthError::NothingToRestore)?;
        let (plan, failed, wrote, backup) = (
            attempt.plan.clone(),
            attempt.failed,
            attempt.wrote.clone(),
            attempt.backup_path.clone(),
        );
        if !failed {
            let now = self
                .client
                .read_user_slot(plan.slot(), &CancelToken::new())?;
            if now != wrote {
                return Err(SynthError::SlotChanged {
                    slot: u16::from(plan.slot()) + 1,
                    backup,
                });
            }
        }
        let report = self
            .client
            .restore_user_backup(&plan, &CancelToken::new())?;
        self.last = None;
        Ok(SaveReport {
            slot: plan.slot(),
            messages: report.messages,
            bytes: report.bytes,
            verified: report.verified,
        })
    }

    /// Read all 128 User patches as `(slot, blocks)`. Reads only.
    pub fn read_bank(
        &mut self,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<Vec<(u8, Vec<u8>)>, SynthError> {
        let bank = self.client.fetch_bank(0..128, cancel, progress)?;
        if bank.cancelled {
            return Err(ClientError::Cancelled.into());
        }
        if let Some((slot, e)) = bank.failures.first() {
            return Err(ClientError::Refused(format!(
                "User patch {} could not be read ({e}); the bank is incomplete",
                u16::from(*slot) + 1
            ))
            .into());
        }
        Ok(bank
            .patches
            .into_iter()
            .map(|(slot, p)| (slot, p.to_blocks()))
            .collect())
    }

    /// Read everything the instrument keeps in User memory that this editor has a map for (128 patches, 32 performances,
    /// 2 rhythm sets) and return it as one `.syx` the instrument itself would accept back. Nothing is written to the
    /// instrument. Any slot that cannot be read ends the run: a backup with holes would be worse than none.
    pub fn backup_user_memory(
        &mut self,
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<Vec<u8>, SynthError> {
        let device = self.client.config().device;
        let bank = self.client.fetch_bank(0..128, cancel, progress)?;
        if bank.cancelled {
            return Err(ClientError::Cancelled.into());
        }
        if let Some((slot, e)) = bank.failures.first() {
            return Err(ClientError::Refused(format!(
                "User patch {} could not be read ({e}); the backup would be incomplete",
                u16::from(*slot) + 1
            ))
            .into());
        }
        let performances =
            self.client
                .fetch_user_documents(DocKind::Performance, cancel, progress)?;
        let rhythm_sets = self
            .client
            .fetch_user_documents(DocKind::Rhythm, cancel, progress)?;
        let mut out = xppatch_core::user_patch_bank_bytes(&bank.patches, device, None)
            .map_err(|e| ClientError::Refused(e.to_string()))?;
        out.extend(
            xppatch_core::user_document_bytes(DocKind::Performance, &performances, device, None)
                .map_err(|e| ClientError::Refused(e.to_string()))?,
        );
        out.extend(
            xppatch_core::user_document_bytes(DocKind::Rhythm, &rhythm_sets, device, None)
                .map_err(|e| ClientError::Refused(e.to_string()))?,
        );
        Ok(out)
    }

    /// Step one of writing a bank: read all 128 slots, save them to a backup `.syx` file (checked by reading it back),
    /// and arm one bank write. Any slot that cannot be read stops this: a bank is never overwritten without a full backup.
    pub fn arm_bank_write(
        &mut self,
        backup_dir: &Path,
        stamp: u64,
        slots_to_write: &[u8],
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<ArmedBank, SynthError> {
        self.bank = None;
        // Arming a bank replaces any single-slot arming, so only one write can ever be pending.
        self.save = None;
        let mut confirmed: Vec<u8> = slots_to_write.to_vec();
        confirmed.sort_unstable();
        confirmed.dedup();
        if confirmed.is_empty() || confirmed.iter().any(|s| *s >= 128) {
            return Err(SynthError::NotConfirmed(
                "choose at least one slot, from 1 to 128".to_owned(),
            ));
        }
        let plans = self.client.plan_user_bank(cancel, progress)?;
        let slots: Vec<(u8, Patch)> = plans
            .iter()
            .map(|p| (p.slot(), p.backup().clone()))
            .collect();
        let device = self.client.config().device;
        let bytes = xppatch_core::user_patch_bank_bytes(&slots, device, Some(128))
            .map_err(|e| SynthError::Backup(e.to_string()))?;
        std::fs::create_dir_all(backup_dir).map_err(|e| SynthError::Backup(e.to_string()))?;
        let (path, mut file) = create_backup_file(backup_dir, &format!("{stamp}-user-bank"))?;
        let written = std::io::Write::write_all(&mut file, &bytes).and_then(|()| file.sync_all());
        drop(file);
        let verified = written
            .map_err(|e| e.to_string())
            .and_then(|()| std::fs::read(&path).map_err(|e| e.to_string()))
            .and_then(|raw| {
                let back = xppatch_core::SyxImage::parse(&raw)
                    .and_then(|i| i.user_patches())
                    .map_err(|e| e.to_string())?;
                if back == slots {
                    Ok(())
                } else {
                    Err("the backup file does not match the bank".to_owned())
                }
            });
        if let Err(e) = verified {
            let _ = std::fs::remove_file(&path);
            return Err(SynthError::Backup(e));
        }
        let names = slots.iter().map(|(_, p)| p.name()).collect();
        self.bank = Some((plans, true, Instant::now(), confirmed));
        Ok(ArmedBank {
            backup_path: path.display().to_string(),
            names,
        })
    }

    pub fn disarm_bank_write(&mut self) {
        if let Some((_, armed, _, _)) = self.bank.as_mut() {
            *armed = false;
        }
    }

    /// Step two: write `patches` (slot, 590 bytes of blocks) into their slots. Every patch is checked before the first
    /// write; the arming is used up at once; each slot is read again, written inside itself and read back.
    pub fn commit_bank_write(
        &mut self,
        patches: &[(u8, Vec<u8>)],
        cancel: &CancelToken,
        progress: &mut dyn FnMut(Progress),
    ) -> Result<BankSaveReport, SynthError> {
        let (plans, armed, since, confirmed) = self.bank.as_mut().ok_or(SynthError::NotArmed)?;
        if !std::mem::replace(armed, false) {
            return Err(SynthError::NotArmed);
        }
        if since.elapsed() > BANK_ARMING_LIFETIME {
            return Err(SynthError::ArmingExpired);
        }
        let plans = plans.clone();
        let confirmed = confirmed.clone();
        // What is written must be exactly the set of slots the user confirmed, nothing more, nothing less.
        let mut given: Vec<u8> = patches.iter().map(|(s, _)| *s).collect();
        given.sort_unstable();
        if given != confirmed {
            return Err(SynthError::NotConfirmed(format!(
                "confirmed {} slot(s), asked to write {}",
                confirmed.len(),
                given.len()
            )));
        }
        if patches.len() > 128 || patches.iter().any(|(_, b)| b.len() != PATCH_BYTES) {
            return Err(SynthError::BadPatch(
                "each patch must be exactly 590 bytes".to_owned(),
            ));
        }
        let mut parsed = Vec::with_capacity(patches.len());
        for (slot, bytes) in patches {
            let p = Patch::from_blocks(bytes)
                .map_err(|e| SynthError::BadPatch(format!("slot {}: {e}", u16::from(*slot) + 1)))?;
            if !p.validate().is_empty() {
                return Err(SynthError::BadPatch(format!(
                    "slot {} holds out-of-range values",
                    u16::from(*slot) + 1
                )));
            }
            parsed.push((*slot, p));
        }
        let report = self
            .client
            .commit_user_bank(&plans, &parsed, cancel, progress)?;
        Ok(BankSaveReport {
            written: report.written,
            stopped_at: report.stopped_at,
            cancelled: report.cancelled,
        })
    }

    /// Read the instrument's temporary patch (`"patch"`), Performance or Rhythm Set as the concatenated bytes of all
    /// blocks, in transmission order.
    pub fn fetch_temp_doc(&mut self, kind: &str) -> Result<Vec<u8>, SynthError> {
        let kind = match kind {
            "patch" => return self.fetch_temp_patch(),
            "performance" => DocKind::Performance,
            "rhythm" => DocKind::Rhythm,
            "system" => DocKind::System,
            other => return Err(SynthError::Kind(other.to_owned())),
        };
        let doc =
            self.client
                .fetch_document(kind, kind.temp_base(), &CancelToken::new(), &mut |_| {})?;
        Ok(doc
            .blocks()
            .iter()
            .flat_map(|b| b.bytes().iter().copied())
            .collect())
    }

    /// Set the gap between writes in milliseconds (20 to 250); returns the value in use.
    pub fn set_pacing(&mut self, gap_ms: u32) -> u32 {
        let gap = self
            .client
            .set_min_dt1_gap(Duration::from_millis(u64::from(gap_ms)));
        u32::try_from(gap.as_millis()).unwrap_or(250)
    }

    /// Play or release a note (`channel` 0 to 15).
    pub fn note(
        &mut self,
        channel: u8,
        note: u8,
        velocity: u8,
        on: bool,
    ) -> Result<(), SynthError> {
        Ok(self.client.send_note(channel, note, velocity, on)?)
    }

    pub fn all_notes_off(&mut self, channel: u8) -> Result<(), SynthError> {
        Ok(self.client.all_notes_off(channel)?)
    }

    /// Start or stop listening for controller moves (used to drive parameters from a hardware controller).
    pub fn listen_controls(&mut self, on: bool) {
        self.client.listen_controllers(on);
    }

    /// The controllers that moved since the last call, each once with its latest value.
    pub fn poll_controls(&mut self) -> Result<Vec<ControlMove>, SynthError> {
        self.client.poll(Duration::from_millis(1))?;
        Ok(self
            .client
            .take_controls()
            .into_iter()
            .map(|c| {
                let (kind, number) = match c.source {
                    ControlSource::Cc(n) => ("cc", u16::from(n)),
                    ControlSource::Nrpn(n) => ("nrpn", n),
                    ControlSource::Rpn(n) => ("rpn", n),
                };
                ControlMove {
                    channel: c.channel,
                    kind,
                    number,
                    value: c.value,
                    max: c.source.max_value(),
                }
            })
            .collect())
    }

    /// True when the panel changed the sound since the last call (a Program or Bank Change was heard).
    pub fn poll_stale(&mut self) -> Result<bool, SynthError> {
        self.client.poll(Duration::from_millis(1))?;
        Ok(!self.client.take_events().is_empty())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use xppatch_core::{Patch, SyxImage};
    use xppatch_midi::testing::VirtualXp60;

    const BANK: &[u8] = include_bytes!("../../../tests/golden/xp60-user-bank.syx");

    fn virtual_synth(device: u8) -> VirtualXp60 {
        let mut d = VirtualXp60::new(DeviceId::new(device).unwrap());
        let patches = SyxImage::parse(BANK).unwrap().user_patches().unwrap();
        d.set_temp_patch(&patches[0].1).unwrap();
        d
    }

    fn patch0() -> Patch {
        SyxImage::parse(BANK).unwrap().user_patches().unwrap()[0]
            .1
            .clone()
    }

    #[test]
    fn connecting_reports_the_device_and_the_sound_name() {
        let (_, info) = Synth::connect(virtual_synth(0x10), 0x10).unwrap();
        assert_eq!(info.device_number, 17);
        assert_eq!(info.patch_name, patch0().name().trim_end());
    }

    #[test]
    fn connecting_finds_a_device_on_another_id() {
        let (_, info) = Synth::connect(virtual_synth(0x14), 0x10).unwrap();
        assert_eq!(info.device_number, 21);
    }

    #[test]
    fn connecting_to_silence_fails_and_a_bad_id_is_rejected() {
        let silent = VirtualXp60::new(DeviceId::DEFAULT);
        assert!(matches!(
            Synth::connect(silent, 0x10),
            Err(SynthError::Client(_))
        ));
        assert!(matches!(
            Synth::connect(virtual_synth(0x10), 0x99),
            Err(SynthError::Device(_))
        ));
    }

    #[test]
    fn the_temporary_patch_is_fetched_as_five_blocks() {
        let (mut s, _) = Synth::connect(virtual_synth(0x10), 0x10).unwrap();
        let bytes = s.fetch_temp_patch().unwrap();
        assert_eq!(bytes.len(), PATCH_BYTES);
        assert_eq!(&bytes[..73], patch0().common.bytes());
    }

    #[test]
    fn live_frames_reach_the_temporary_patch_and_nothing_else() {
        use xppatch_sysex::address::Address;
        use xppatch_sysex::message::Message;
        let (mut s, _) = Synth::connect(virtual_synth(0x10), 0x10).unwrap();
        let ok = Message::Dt1 {
            address: Address::from_lanes([3, 0, 0x10, 0x65]),
            data: vec![99],
        }
        .encode(DeviceId::DEFAULT)
        .unwrap();
        s.send_frame(&ok).unwrap();
        let user = Message::Dt1 {
            address: Address::from_lanes([0x11, 0, 0, 0]),
            data: vec![1],
        }
        .encode(DeviceId::DEFAULT)
        .unwrap();
        assert!(s.send_frame(&user).is_err());
        let bytes = s.fetch_temp_patch().unwrap();
        assert_eq!(bytes[73 + 0x65], 99);
    }

    #[test]
    fn the_whole_user_memory_is_read_into_one_file_the_instrument_would_accept() {
        let mut d = virtual_synth(0x10);
        d.load_image(&SyxImage::parse(BANK).unwrap());
        let (mut s, _) = Synth::connect(d, 0x10).unwrap();
        let mut seen = std::collections::BTreeSet::new();
        let bytes = s
            .backup_user_memory(&CancelToken::new(), &mut |p| {
                seen.insert(p.what);
            })
            .unwrap();
        assert_eq!(
            seen.into_iter().collect::<Vec<_>>(),
            vec!["user patches", "user performances", "user rhythm sets"]
        );
        let image = SyxImage::parse(&bytes).unwrap();
        assert_eq!(image.user_patches().unwrap().len(), 128);
        assert_eq!(
            image.user_documents(DocKind::Performance).unwrap().len(),
            32
        );
        assert_eq!(image.user_documents(DocKind::Rhythm).unwrap().len(), 2);
        // Same patches as the file the virtual instrument was loaded from.
        let original = SyxImage::parse(BANK).unwrap();
        assert_eq!(
            image.user_patches().unwrap(),
            original.user_patches().unwrap()
        );
        // Every message is a DT1 that stays inside what the instrument accepts.
        assert!(image.records.iter().all(|r| r.data.len() <= 129));
    }

    #[test]
    fn a_backup_with_an_unreadable_performance_is_refused_whole() {
        let mut d = VirtualXp60::new(DeviceId::new(0x10).unwrap());
        let patches = SyxImage::parse(BANK).unwrap().user_patches().unwrap();
        d.set_temp_patch(&patches[0].1).unwrap();
        for (slot, p) in &patches {
            d.set_user_patch(*slot, p).unwrap();
        }
        // The patches are there; no performances were ever stored.
        let (mut s, _) = Synth::connect(d, 0x10).unwrap();
        assert!(s
            .backup_user_memory(&CancelToken::new(), &mut |_| {})
            .is_err());
    }

    #[test]
    fn controller_moves_are_polled_once_and_do_not_mark_the_sound_stale() {
        let mut d = virtual_synth(0x10);
        d.control_change(0, 1, 100);
        d.control_change(0, 1, 101);
        let (mut s, _) = Synth::connect(d, 0x10).unwrap();
        s.listen_controls(true);
        let moves = s.poll_controls().unwrap();
        assert_eq!(
            moves,
            vec![ControlMove {
                channel: 0,
                kind: "cc",
                number: 1,
                value: 101,
                max: 127
            }]
        );
        assert!(s.poll_controls().unwrap().is_empty());
        assert!(
            !s.poll_stale().unwrap(),
            "a controller is not a patch change"
        );
    }

    #[test]
    fn a_panel_selection_is_reported_once() {
        let mut d = virtual_synth(0x10);
        let (mut s, _) = {
            let (s, c) = Synth::connect(
                std::mem::replace(&mut d, VirtualXp60::new(DeviceId::DEFAULT)),
                0x10,
            )
            .unwrap();
            (s, c)
        };
        assert!(!s.poll_stale().unwrap());
        s.client.port_mut().panel_select(0, 81, 0, 3);
        assert!(s.poll_stale().unwrap());
        assert!(!s.poll_stale().unwrap());
    }

    #[test]
    fn performances_and_rhythm_sets_are_fetched_whole() {
        use xppatch_core::Document;
        let mut d = virtual_synth(0x10);
        d.set_document(
            DocKind::Performance.temp_base(),
            &Document::init(DocKind::Performance),
        )
        .unwrap();
        d.set_document(
            DocKind::Rhythm.temp_base(),
            &Document::init(DocKind::Rhythm),
        )
        .unwrap();
        d.set_document(
            DocKind::System.temp_base(),
            &Document::init(DocKind::System),
        )
        .unwrap();
        let (mut s, _) = Synth::connect(d, 0x10).unwrap();
        assert_eq!(s.fetch_temp_doc("system").unwrap().len(), 96 + 17 * 12);
        assert_eq!(s.fetch_temp_doc("performance").unwrap().len(), 66 + 16 * 25);
        assert_eq!(s.fetch_temp_doc("rhythm").unwrap().len(), 12 + 64 * 58);
        assert_eq!(s.fetch_temp_doc("patch").unwrap().len(), PATCH_BYTES);
        assert!(matches!(
            s.fetch_temp_doc("bogus"),
            Err(SynthError::Kind(_))
        ));
    }

    fn temp_dir(name: &str) -> std::path::PathBuf {
        let d = std::env::temp_dir().join(format!("xppatch-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        d
    }

    fn bank_patch(slot: usize) -> Patch {
        SyxImage::parse(BANK).unwrap().user_patches().unwrap()[slot]
            .1
            .clone()
    }

    fn synth_with_bank() -> Synth<VirtualXp60> {
        let mut d = virtual_synth(0x10);
        d.load_image(&SyxImage::parse(BANK).unwrap());
        Synth::connect(d, 0x10).unwrap().0
    }

    #[test]
    fn arming_saves_the_old_patch_to_disk_before_anything_can_be_written() {
        let dir = temp_dir("arm");
        let mut s = synth_with_bank();
        let armed = s.arm_user_save(5, &dir, 1000).unwrap();
        assert_eq!(armed.slot, 5);
        assert_eq!(armed.occupant_name, bank_patch(5).name());
        let file = std::fs::read(&armed.backup_path).unwrap();
        let slots = SyxImage::parse(&file).unwrap().user_patches().unwrap();
        assert_eq!(slots.len(), 1);
        assert_eq!(slots[0].0, 5);
        assert_eq!(slots[0].1, bank_patch(5));
        assert!(armed.backup_path.ends_with("1000-user-006.syx"));
        // Arming again with the same stamp gets a new file name; the earlier backup is never overwritten.
        let again = s.arm_user_save(5, &dir, 1000).unwrap();
        assert!(
            again.backup_path.ends_with("1000-user-006-2.syx"),
            "{}",
            again.backup_path
        );
        assert!(std::path::Path::new(&armed.backup_path).exists());
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_save_needs_arming_and_one_arming_allows_one_write() {
        let dir = temp_dir("once");
        let mut s = synth_with_bank();
        let new = {
            let mut p = bank_patch(0);
            p.set_name("Saved One").unwrap();
            p
        };
        assert!(matches!(
            s.commit_user_save(&new.to_blocks()),
            Err(SynthError::NotArmed)
        ));
        s.arm_user_save(9, &dir, 1).unwrap();
        let report = s.commit_user_save(&new.to_blocks()).unwrap();
        assert!(report.verified);
        assert_eq!(report.slot, 9);
        // The arming is used up.
        assert!(matches!(
            s.commit_user_save(&new.to_blocks()),
            Err(SynthError::NotArmed)
        ));
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn disarming_cancels_and_bad_data_is_refused_without_writing() {
        let dir = temp_dir("disarm");
        let mut s = synth_with_bank();
        s.arm_user_save(1, &dir, 2).unwrap();
        s.disarm_user_save();
        assert!(matches!(
            s.commit_user_save(&bank_patch(0).to_blocks()),
            Err(SynthError::NotArmed)
        ));
        s.arm_user_save(1, &dir, 3).unwrap();
        assert!(matches!(
            s.commit_user_save(&[1, 2, 3]),
            Err(SynthError::BadPatch(_))
        ));
        // The bad attempt used the arming up.
        assert!(matches!(
            s.commit_user_save(&bank_patch(0).to_blocks()),
            Err(SynthError::NotArmed)
        ));
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn an_unwritable_backup_location_stops_the_save_before_arming() {
        let dir = temp_dir("blocked");
        std::fs::create_dir_all(&dir).unwrap();
        let file_in_the_way = dir.join("not-a-dir");
        std::fs::write(&file_in_the_way, b"x").unwrap();
        let mut s = synth_with_bank();
        assert!(matches!(
            s.arm_user_save(0, &file_in_the_way, 4),
            Err(SynthError::Backup(_))
        ));
        assert!(matches!(
            s.commit_user_save(&bank_patch(0).to_blocks()),
            Err(SynthError::NotArmed)
        ));
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn the_last_save_can_be_undone() {
        let dir = temp_dir("undo");
        let mut s = synth_with_bank();
        assert!(matches!(
            s.restore_user_backup(),
            Err(SynthError::NothingToRestore)
        ));
        s.arm_user_save(3, &dir, 5).unwrap();
        let mut p = bank_patch(3);
        p.set_name("Oops").unwrap();
        s.commit_user_save(&p.to_blocks()).unwrap();
        let restored = s.restore_user_backup().unwrap();
        assert!(restored.verified);
        // Reading the slot again through a fresh plan shows the original.
        let again = s.arm_user_save(3, &dir, 6).unwrap();
        assert_eq!(again.occupant_name, bank_patch(3).name());
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn undo_is_refused_when_the_slot_changed_after_the_save() {
        let dir = temp_dir("stale-undo");
        let mut s = synth_with_bank();
        s.arm_user_save(4, &dir, 10).unwrap();
        let mut p = bank_patch(4);
        p.set_name("Saved").unwrap();
        s.commit_user_save(&p.to_blocks()).unwrap();
        // The slot is changed from the panel afterwards: putting the old patch back would lose that change.
        let mut other = bank_patch(4);
        other.set_name("Later Edit").unwrap();
        s.client.port_mut().set_user_patch(4, &other).unwrap();
        let err = s.restore_user_backup().unwrap_err();
        assert!(
            matches!(err, SynthError::SlotChanged { slot: 5, .. }),
            "{err:?}"
        );
        // Nothing was written by the refused undo, and the attempt is still there to try again.
        assert_eq!(
            s.client.read_user_slot(4, &CancelToken::new()).unwrap(),
            other
        );
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn undo_works_once_and_arming_another_slot_does_not_lose_it() {
        let dir = temp_dir("undo-once");
        let mut s = synth_with_bank();
        s.arm_user_save(7, &dir, 20).unwrap();
        let mut p = bank_patch(7);
        p.set_name("First").unwrap();
        s.commit_user_save(&p.to_blocks()).unwrap();
        // Arm a different slot and walk away: the undo of slot 8 must still be about slot 8.
        s.arm_user_save(9, &dir, 21).unwrap();
        s.disarm_user_save();
        let restored = s.restore_user_backup().unwrap();
        assert_eq!(restored.slot, 7);
        assert_eq!(
            s.client.read_user_slot(7, &CancelToken::new()).unwrap(),
            bank_patch(7)
        );
        assert!(matches!(
            s.restore_user_backup(),
            Err(SynthError::NothingToRestore)
        ));
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_save_that_found_the_slot_changed_writes_nothing_and_cannot_be_undone() {
        let dir = temp_dir("stale-arm");
        let mut s = synth_with_bank();
        s.arm_user_save(2, &dir, 30).unwrap();
        let mut other = bank_patch(2);
        other.set_name("Changed").unwrap();
        s.client.port_mut().set_user_patch(2, &other).unwrap();
        let err = s.commit_user_save(&bank_patch(0).to_blocks()).unwrap_err();
        assert!(
            matches!(err, SynthError::Client(ClientError::Refused(_))),
            "{err:?}"
        );
        assert!(matches!(
            s.restore_user_backup(),
            Err(SynthError::NothingToRestore)
        ));
        assert_eq!(
            s.client.read_user_slot(2, &CancelToken::new()).unwrap(),
            other
        );
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_bank_write_backs_up_all_128_then_writes_only_what_it_is_given() {
        let dir = temp_dir("bank");
        let mut s = synth_with_bank();
        let cancel = CancelToken::new();
        let armed = s
            .arm_bank_write(&dir, 77, &[11], &cancel, &mut |_| {})
            .unwrap();
        assert!(armed.backup_path.ends_with("77-user-bank.syx"));
        assert_eq!(armed.names.len(), 128);
        assert_eq!(armed.names[3], bank_patch(3).name());
        let file = std::fs::read(&armed.backup_path).unwrap();
        assert_eq!(
            SyxImage::parse(&file)
                .unwrap()
                .user_patches()
                .unwrap()
                .len(),
            128
        );

        let mut p = bank_patch(0);
        p.set_name("Bank One").unwrap();
        let report = s
            .commit_bank_write(&[(11, p.to_blocks())], &cancel, &mut |_| {})
            .unwrap();
        assert_eq!(report.written, vec![11]);
        assert_eq!(
            s.client.read_user_slot(11, &cancel).unwrap().name(),
            "Bank One"
        );
        // One arming, one write.
        assert!(matches!(
            s.commit_bank_write(&[(12, p.to_blocks())], &cancel, &mut |_| {}),
            Err(SynthError::NotArmed)
        ));
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_bank_write_checks_every_patch_before_writing_anything() {
        let dir = temp_dir("bank-bad");
        let mut s = synth_with_bank();
        let cancel = CancelToken::new();
        s.arm_bank_write(&dir, 78, &[20, 21], &cancel, &mut |_| {})
            .unwrap();
        let good = bank_patch(1).to_blocks();
        let err = s
            .commit_bank_write(
                &[(20, good.clone()), (21, vec![1, 2, 3])],
                &cancel,
                &mut |_| {},
            )
            .unwrap_err();
        assert!(matches!(err, SynthError::BadPatch(_)), "{err:?}");
        // Nothing was written: slot 20 is as it was.
        assert_eq!(
            s.client.read_user_slot(20, &cancel).unwrap(),
            bank_patch(20)
        );
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn disarming_a_bank_and_an_unreadable_bank_both_stop_the_write() {
        let dir = temp_dir("bank-stop");
        let mut s = synth_with_bank();
        let cancel = CancelToken::new();
        s.arm_bank_write(&dir, 79, &[0], &cancel, &mut |_| {})
            .unwrap();
        s.disarm_bank_write();
        assert!(matches!(
            s.commit_bank_write(&[(0, bank_patch(0).to_blocks())], &cancel, &mut |_| {}),
            Err(SynthError::NotArmed)
        ));
        // A synth where one slot is silent cannot be backed up, so it cannot be armed.
        let mut d = virtual_synth(0x10);
        for slot in 0..127u8 {
            d.set_user_patch(slot, &bank_patch(usize::from(slot)))
                .unwrap();
        }
        let (mut partial, _) = Synth::connect(d, 0x10).unwrap();
        assert!(partial
            .arm_bank_write(&dir, 80, &[0], &cancel, &mut |_| {})
            .is_err());
        assert!(matches!(
            partial.commit_bank_write(&[(0, bank_patch(0).to_blocks())], &cancel, &mut |_| {}),
            Err(SynthError::NotArmed)
        ));
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn reading_the_bank_returns_all_128_as_blocks() {
        let mut s = synth_with_bank();
        let bank = s.read_bank(&CancelToken::new(), &mut |_| {}).unwrap();
        assert_eq!(bank.len(), 128);
        assert_eq!(bank[9].1, bank_patch(9).to_blocks());
    }

    #[test]
    fn a_bank_write_must_match_the_slots_that_were_confirmed() {
        let dir = temp_dir("bank-confirmed");
        let mut s = synth_with_bank();
        let cancel = CancelToken::new();
        s.arm_bank_write(&dir, 90, &[5, 6], &cancel, &mut |_| {})
            .unwrap();
        let p = bank_patch(0).to_blocks();
        // An extra slot, a missing one, or a different one: refused, and the arming is used up.
        let err = s
            .commit_bank_write(
                &[(5, p.clone()), (6, p.clone()), (7, p.clone())],
                &cancel,
                &mut |_| {},
            )
            .unwrap_err();
        assert!(matches!(err, SynthError::NotConfirmed(_)), "{err:?}");
        assert!(matches!(
            s.commit_bank_write(&[(5, p.clone()), (6, p.clone())], &cancel, &mut |_| {}),
            Err(SynthError::NotArmed)
        ));
        s.arm_bank_write(&dir, 91, &[5, 6], &cancel, &mut |_| {})
            .unwrap();
        let err = s
            .commit_bank_write(&[(5, p.clone())], &cancel, &mut |_| {})
            .unwrap_err();
        assert!(matches!(err, SynthError::NotConfirmed(_)), "{err:?}");
        // Nothing was written.
        assert_eq!(s.client.read_user_slot(5, &cancel).unwrap(), bank_patch(5));
        // An empty or out-of-range confirmed set cannot be armed at all.
        assert!(s
            .arm_bank_write(&dir, 92, &[], &cancel, &mut |_| {})
            .is_err());
        assert!(s
            .arm_bank_write(&dir, 93, &[200], &cancel, &mut |_| {})
            .is_err());
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn arming_one_kind_of_save_drops_the_other() {
        let dir = temp_dir("both");
        let mut s = synth_with_bank();
        let cancel = CancelToken::new();
        s.arm_user_save(1, &dir, 100).unwrap();
        s.arm_bank_write(&dir, 101, &[1], &cancel, &mut |_| {})
            .unwrap();
        assert!(matches!(
            s.commit_user_save(&bank_patch(0).to_blocks()),
            Err(SynthError::NotArmed)
        ));
        s.arm_user_save(2, &dir, 102).unwrap();
        assert!(matches!(
            s.commit_bank_write(&[(1, bank_patch(0).to_blocks())], &cancel, &mut |_| {}),
            Err(SynthError::NotArmed)
        ));
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_partly_unreadable_bank_is_an_error_not_a_short_list() {
        let mut d = virtual_synth(0x10);
        for slot in 0..100u8 {
            d.set_user_patch(slot, &bank_patch(usize::from(slot)))
                .unwrap();
        }
        let (mut s, _) = Synth::connect(d, 0x10).unwrap();
        assert!(s.read_bank(&CancelToken::new(), &mut |_| {}).is_err());
    }
}
