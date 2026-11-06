//! XPPatchStudio desktop shell. The editor UI runs in a webview; these commands are its only way to reach MIDI.

pub mod storage;
pub mod synth;

use serde::Serialize;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;
use std::time::{SystemTime, UNIX_EPOCH};
use synth::{
    ArmedBank, ArmedSave, BankSaveReport, Connected, ControlMove, SaveReport, Synth, SynthError,
};
use tauri::{Emitter, Manager};
use xppatch_library::{Filter, Library, SaveTarget};
use xppatch_midi::hardware::{list_ports, MidirTransport};
use xppatch_midi::CancelToken;
use xppatch_midi::TransportError;

type LiveSynth = Synth<MidirTransport>;

/// The connection and the library. Commands lock them for the short time one operation takes.
struct AppState {
    synth: Mutex<Option<LiveSynth>>,
    /// Cancels the long operation in progress (bank read or write). A fresh one is made for each operation.
    cancel: Mutex<CancelToken>,
    /// A long operation (bank read or write) is running; other instrument commands are refused meanwhile instead of
    /// queueing behind it.
    busy: AtomicBool,
    library: Mutex<Result<Library, String>>,
}

impl Default for AppState {
    fn default() -> Self {
        AppState {
            synth: Mutex::new(None),
            cancel: Mutex::new(CancelToken::new()),
            busy: AtomicBool::new(false),
            library: Mutex::new(Err("the library is not open yet".to_owned())),
        }
    }
}

fn lock_synth<R>(
    state: &tauri::State<'_, AppState>,
    f: impl FnOnce(&mut LiveSynth) -> Result<R, SynthError>,
) -> Result<R, SynthError> {
    let mut guard = state
        .synth
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    f(guard.as_mut().ok_or(SynthError::NotConnected)?)
}

/// For ordinary short commands: refused while a long operation owns the instrument.
fn with_synth<R>(
    state: &tauri::State<'_, AppState>,
    f: impl FnOnce(&mut LiveSynth) -> Result<R, SynthError>,
) -> Result<R, SynthError> {
    if state.busy.load(Ordering::SeqCst) {
        return Err(SynthError::Busy);
    }
    lock_synth(state, f)
}

/// Marks a long operation as running and gives it a fresh cancel token; clears the mark when dropped.
struct LongOp<'a> {
    busy: &'a AtomicBool,
    pub cancel: CancelToken,
}

impl Drop for LongOp<'_> {
    fn drop(&mut self) {
        self.busy.store(false, Ordering::SeqCst);
    }
}

/// Start a long operation, or say that one is already running. Single-flight: a second one is refused, so a cancel
/// always reaches the operation that is actually running.
fn begin_long<'a>(state: &'a tauri::State<'_, AppState>) -> Result<LongOp<'a>, String> {
    state
        .busy
        .compare_exchange(false, true, Ordering::SeqCst, Ordering::SeqCst)
        .map_err(|_| SynthError::Busy.to_string())?;
    let cancel = CancelToken::new();
    *state
        .cancel
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner) = cancel.clone();
    Ok(LongOp {
        busy: &state.busy,
        cancel,
    })
}

#[derive(Serialize)]
struct Ports {
    inputs: Vec<String>,
    outputs: Vec<String>,
}

#[tauri::command(async)]
fn midi_ports() -> Result<Ports, String> {
    let p = list_ports().map_err(|e: TransportError| e.to_string())?;
    Ok(Ports {
        inputs: p.inputs,
        outputs: p.outputs,
    })
}

/// Open the named ports and find the instrument. `input` and `output` are substrings that must match exactly one port.
#[tauri::command(async)]
fn midi_connect(
    state: tauri::State<'_, AppState>,
    input: String,
    output: String,
    device: u8,
) -> Result<Connected, String> {
    // Windows MIDI ports are exclusive: let go of any previous connection before opening the ports again.
    drop(
        state
            .synth
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner)
            .take(),
    );
    let port = MidirTransport::connect(&input, &output).map_err(|e| e.to_string())?;
    let (synth, info) = Synth::connect(port, device).map_err(|e| e.to_string())?;
    *state
        .synth
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner) = Some(synth);
    Ok(info)
}

#[tauri::command(async)]
fn midi_disconnect(state: tauri::State<'_, AppState>) -> Result<(), String> {
    if state.busy.load(Ordering::SeqCst) {
        return Err(SynthError::Busy.to_string());
    }
    *state
        .synth
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner) = None;
    Ok(())
}

/// One live-edit DT1 frame for the temporary patch.
#[tauri::command(async)]
fn midi_send_frame(state: tauri::State<'_, AppState>, frame: Vec<u8>) -> Result<(), SynthError> {
    with_synth(&state, |s| s.send_frame(&frame))
}

/// The instrument's temporary patch as 590 bytes (Common, then Tone 1 to 4).
#[tauri::command(async)]
fn midi_fetch_temp_patch(state: tauri::State<'_, AppState>) -> Result<Vec<u8>, SynthError> {
    with_synth(&state, |s| s.fetch_temp_patch())
}

/// The temporary patch, Performance or Rhythm Set (`kind` is "patch", "performance" or "rhythm") as concatenated block bytes.
#[tauri::command(async)]
fn midi_fetch_temp_doc(
    state: tauri::State<'_, AppState>,
    kind: String,
) -> Result<Vec<u8>, SynthError> {
    with_synth(&state, |s| s.fetch_temp_doc(&kind))
}

/// Set the gap between writes (20 to 250 ms); returns the value in use.
#[tauri::command(async)]
fn midi_set_pacing(state: tauri::State<'_, AppState>, gap_ms: u32) -> Result<u32, SynthError> {
    with_synth(&state, |s| Ok(s.set_pacing(gap_ms)))
}

/// Play or release a note on the instrument (audition). Notes write nothing.
#[tauri::command(async)]
fn midi_note(
    state: tauri::State<'_, AppState>,
    channel: u8,
    note: u8,
    velocity: u8,
    on: bool,
) -> Result<(), SynthError> {
    with_synth(&state, |s| s.note(channel, note, velocity, on))
}

/// Select a sound on the instrument with Bank Select and Program Change, as the front panel's own patch buttons do.
/// Nothing is written to memory; the instrument's temporary sound is replaced.
#[tauri::command(async)]
fn midi_select_sound(
    state: tauri::State<'_, AppState>,
    channel: u8,
    msb: u8,
    lsb: u8,
    program: u8,
) -> Result<(), SynthError> {
    with_synth(&state, |s| s.select_sound(channel, msb, lsb, program))
}

#[tauri::command(async)]
fn midi_all_notes_off(state: tauri::State<'_, AppState>, channel: u8) -> Result<(), SynthError> {
    with_synth(&state, |s| s.all_notes_off(channel))
}

/// True once if the sound was changed from the instrument's panel.
/// Start or stop listening for controller moves (mod wheel, sliders, pedals) on the connected input.
#[tauri::command(async)]
fn midi_listen_controls(state: tauri::State<'_, AppState>, on: bool) -> Result<(), SynthError> {
    with_synth(&state, |s| {
        s.listen_controls(on);
        Ok(())
    })
}

/// The controllers that moved since the last call, each once with its latest value.
#[tauri::command(async)]
fn midi_poll_controls(state: tauri::State<'_, AppState>) -> Result<Vec<ControlMove>, SynthError> {
    with_synth(&state, |s| s.poll_controls())
}

#[tauri::command(async)]
fn midi_poll_stale(state: tauri::State<'_, AppState>) -> Result<bool, SynthError> {
    with_synth(&state, |s| s.poll_stale())
}

fn now_secs() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_or(0, |d| i64::try_from(d.as_secs()).unwrap_or(0))
}

/// The default data folder (the one that holds the storage pointer).
fn default_dir(app: &tauri::AppHandle) -> Result<std::path::PathBuf, String> {
    app.path().app_data_dir().map_err(|e| e.to_string())
}

/// The folder the library was opened from in this run. A change of location takes effect at the next start.
static IN_USE: std::sync::OnceLock<std::path::PathBuf> = std::sync::OnceLock::new();

/// Where the library and the backup files are in this run.
fn data_dir(app: &tauri::AppHandle) -> Result<std::path::PathBuf, String> {
    match IN_USE.get() {
        Some(dir) => Ok(dir.clone()),
        None => default_dir(app).map(|d| storage::resolve(&d)),
    }
}

/// The backup files written before saves, newest first.
#[tauri::command(async)]
fn backups_list(app: tauri::AppHandle) -> Result<Vec<storage::BackupFile>, String> {
    Ok(storage::list_backups(&data_dir(&app)?))
}

/// The bytes of one backup file, chosen by name.
#[tauri::command(async)]
fn backup_read(app: tauri::AppHandle, name: String) -> Result<Vec<u8>, String> {
    storage::read_backup(&data_dir(&app)?, &name)
}

#[tauri::command(async)]
fn storage_info(app: tauri::AppHandle) -> Result<storage::StorageInfo, String> {
    let default = default_dir(&app)?;
    let in_use = data_dir(&app)?;
    Ok(storage::info(&default, &in_use))
}

/// Choose the folder for the library and backups (`None` for the default). Copies, never moves; applies at next start.
#[tauri::command(async)]
fn storage_set(
    app: tauri::AppHandle,
    state: tauri::State<'_, AppState>,
    dir: Option<String>,
) -> Result<storage::StorageInfo, String> {
    let default = default_dir(&app)?;
    let in_use = data_dir(&app)?;
    storage::set(&default, &in_use, dir.as_deref(), |dest| {
        let guard = state
            .library
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        match guard.as_ref() {
            Ok(lib) => lib.snapshot_to(dest).map_err(|e| e.to_string()),
            // A library that did not open has nothing to copy; choosing another folder is how to start afresh.
            Err(_) => Ok(()),
        }
    })?;
    Ok(storage::info(&default, &in_use))
}

// ---- saving into the instrument's User memory (the guarded write) ----

/// Step one: read the slot, write its current contents to a backup file, and arm one save.
#[tauri::command(async)]
fn midi_arm_save(
    app: tauri::AppHandle,
    state: tauri::State<'_, AppState>,
    slot: u8,
) -> Result<ArmedSave, String> {
    let dir = data_dir(&app)?.join("backups");
    let stamp = u64::try_from(now_secs()).unwrap_or(0);
    with_synth(&state, |s| s.arm_user_save(slot, &dir, stamp)).map_err(|e| e.to_string())
}

#[tauri::command(async)]
fn midi_disarm_save(state: tauri::State<'_, AppState>) -> Result<(), String> {
    with_synth(&state, |s| {
        s.disarm_user_save();
        Ok(())
    })
    .map_err(|e| e.to_string())
}

/// Step two: write the patch (590 bytes) into the armed slot and read it back.
#[tauri::command(async)]
fn midi_commit_save(
    state: tauri::State<'_, AppState>,
    patch: Vec<u8>,
) -> Result<SaveReport, String> {
    with_synth(&state, |s| s.commit_user_save(&patch)).map_err(|e| e.to_string())
}

/// Put back what the last save replaced.
#[tauri::command(async)]
fn midi_restore_save(state: tauri::State<'_, AppState>) -> Result<SaveReport, String> {
    with_synth(&state, |s| s.restore_user_backup()).map_err(|e| e.to_string())
}

/// Step one of saving the System settings: read them, write them to a backup file, and arm one save.
#[tauri::command(async)]
fn midi_arm_system_save(
    app: tauri::AppHandle,
    state: tauri::State<'_, AppState>,
) -> Result<synth::ArmedSystemSave, String> {
    let dir = data_dir(&app)?.join("backups");
    let stamp = u64::try_from(now_secs()).unwrap_or(0);
    with_synth(&state, |s| s.arm_system_save(&dir, stamp)).map_err(|e| e.to_string())
}

#[tauri::command(async)]
fn midi_disarm_system_save(state: tauri::State<'_, AppState>) -> Result<(), String> {
    with_synth(&state, |s| {
        s.disarm_system_save();
        Ok(())
    })
    .map_err(|e| e.to_string())
}

/// Step two: write the System settings (concatenated block bytes) and read them back.
#[tauri::command(async)]
fn midi_commit_system_save(
    state: tauri::State<'_, AppState>,
    blocks: Vec<u8>,
) -> Result<synth::SystemSaveReport, String> {
    with_synth(&state, |s| s.commit_system_save(&blocks)).map_err(|e| e.to_string())
}

/// Put back the System settings the last save replaced.
#[tauri::command(async)]
fn midi_restore_system_save(
    state: tauri::State<'_, AppState>,
) -> Result<synth::SystemSaveReport, String> {
    with_synth(&state, |s| s.restore_system_backup()).map_err(|e| e.to_string())
}

#[derive(Clone, Serialize)]
struct ProgressOut {
    what: &'static str,
    done: usize,
    total: usize,
}

/// Stop the bank read or write that is running (it finishes the slot it is on).
#[tauri::command]
fn midi_cancel(state: tauri::State<'_, AppState>) {
    state
        .cancel
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner)
        .cancel();
}

/// Read all 128 User patches as `(slot, blocks)`. Progress arrives as `bank-progress` events.
#[tauri::command(async)]
fn midi_read_bank(
    app: tauri::AppHandle,
    state: tauri::State<'_, AppState>,
) -> Result<Vec<(u8, Vec<u8>)>, String> {
    let op = begin_long(&state)?;
    lock_synth(&state, |s| {
        s.read_bank(&op.cancel, &mut |p| {
            let _ = app.emit(
                "bank-progress",
                ProgressOut {
                    what: p.what,
                    done: p.done,
                    total: p.total,
                },
            );
        })
    })
    .map_err(|e| e.to_string())
}

/// Read all of User memory this editor has a map for (patches, performances, rhythm sets) as one `.syx`. Progress arrives
/// as `bank-progress` events. Nothing is written to the instrument.
#[tauri::command(async)]
fn midi_backup_all(
    app: tauri::AppHandle,
    state: tauri::State<'_, AppState>,
) -> Result<Vec<u8>, String> {
    let op = begin_long(&state)?;
    lock_synth(&state, |s| {
        s.backup_user_memory(&op.cancel, &mut |p| {
            let _ = app.emit(
                "bank-progress",
                ProgressOut {
                    what: p.what,
                    done: p.done,
                    total: p.total,
                },
            );
        })
    })
    .map_err(|e| e.to_string())
}

/// Step one of writing a bank: read all 128 slots and keep them in a backup file, then arm one bank write.
#[tauri::command(async)]
fn midi_arm_bank(
    app: tauri::AppHandle,
    state: tauri::State<'_, AppState>,
    slots: Vec<u8>,
) -> Result<ArmedBank, String> {
    let dir = data_dir(&app)?.join("backups");
    let stamp = u64::try_from(now_secs()).unwrap_or(0);
    let op = begin_long(&state)?;
    lock_synth(&state, |s| {
        s.arm_bank_write(&dir, stamp, &slots, &op.cancel, &mut |p| {
            let _ = app.emit(
                "bank-progress",
                ProgressOut {
                    what: p.what,
                    done: p.done,
                    total: p.total,
                },
            );
        })
    })
    .map_err(|e| e.to_string())
}

#[tauri::command(async)]
fn midi_disarm_bank(state: tauri::State<'_, AppState>) -> Result<(), String> {
    with_synth(&state, |s| {
        s.disarm_bank_write();
        Ok(())
    })
    .map_err(|e| e.to_string())
}

/// Step two: write the given `(slot, blocks)` pairs into the armed bank's slots.
#[tauri::command(async)]
fn midi_commit_bank(
    app: tauri::AppHandle,
    state: tauri::State<'_, AppState>,
    patches: Vec<(u8, Vec<u8>)>,
) -> Result<BankSaveReport, String> {
    let op = begin_long(&state)?;
    lock_synth(&state, |s| {
        s.commit_bank_write(&patches, &op.cancel, &mut |p| {
            let _ = app.emit(
                "bank-progress",
                ProgressOut {
                    what: p.what,
                    done: p.done,
                    total: p.total,
                },
            );
        })
    })
    .map_err(|e| e.to_string())
}

/// The given `(slot, blocks)` pairs as a User bank `.syx` file (each block split to 128 bytes), for saving to disk.
#[tauri::command(async)]
fn bank_to_syx(patches: Vec<(u8, Vec<u8>)>, device: u8) -> Result<Vec<u8>, String> {
    let mut parsed = Vec::with_capacity(patches.len());
    for (slot, bytes) in &patches {
        parsed.push((
            *slot,
            xppatch_core::Patch::from_blocks(bytes).map_err(|e| e.to_string())?,
        ));
    }
    let device = xppatch_sysex::message::DeviceId::new(device).map_err(|e| e.to_string())?;
    xppatch_core::user_patch_bank_bytes(&parsed, device, Some(128)).map_err(|e| e.to_string())
}

// ---- the library ----

#[derive(Serialize)]
struct RowOut {
    id: i64,
    name: String,
    tags: Vec<String>,
    favourite: bool,
    source: String,
    added_at: i64,
}

#[derive(Serialize)]
struct ImportOut {
    added: usize,
    duplicates: usize,
    bulk_id: i64,
    bulk_name: String,
}

fn with_library<R>(
    state: &tauri::State<'_, AppState>,
    f: impl FnOnce(&mut Library) -> Result<R, xppatch_library::LibraryError>,
) -> Result<R, String> {
    let mut guard = state
        .library
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    match guard.as_mut() {
        Ok(lib) => f(lib).map_err(|e| e.to_string()),
        Err(why) => Err(format!("the library is unavailable: {why}")),
    }
}

fn filter_of(
    text: Option<String>,
    tag: Option<String>,
    favourites_only: bool,
    bulk: Option<i64>,
    limit: Option<u32>,
    offset: u32,
) -> Filter {
    Filter {
        text,
        tag,
        favourites_only,
        bulk,
        limit,
        offset,
    }
}

#[tauri::command(async)]
fn library_import(
    state: tauri::State<'_, AppState>,
    bytes: Vec<u8>,
    source: String,
) -> Result<ImportOut, String> {
    with_library(&state, |l| l.import_syx(&bytes, &source, now_secs())).map(|r| ImportOut {
        added: r.added,
        duplicates: r.duplicates,
        bulk_id: r.bulk_id,
        bulk_name: r.bulk_name,
    })
}

#[tauri::command(async)]
fn library_list(
    state: tauri::State<'_, AppState>,
    text: Option<String>,
    tag: Option<String>,
    favourites_only: bool,
    bulk: Option<i64>,
    limit: Option<u32>,
    offset: u32,
) -> Result<Vec<RowOut>, String> {
    with_library(&state, |l| {
        l.list(&filter_of(text, tag, favourites_only, bulk, limit, offset))
    })
    .map(|rows| {
        rows.into_iter()
            .map(|r| RowOut {
                id: r.id,
                name: r.name,
                tags: r.tags,
                favourite: r.favourite,
                source: r.source,
                added_at: r.added_at,
            })
            .collect()
    })
}

#[tauri::command(async)]
fn library_count(
    state: tauri::State<'_, AppState>,
    text: Option<String>,
    tag: Option<String>,
    favourites_only: bool,
    bulk: Option<i64>,
) -> Result<u64, String> {
    with_library(&state, |l| {
        l.count(&filter_of(text, tag, favourites_only, bulk, None, 0))
    })
}

#[tauri::command(async)]
fn library_tags(state: tauri::State<'_, AppState>) -> Result<Vec<(String, u64)>, String> {
    with_library(&state, |l| l.all_tags())
}

/// A stored patch as 590 bytes of blocks.
#[tauri::command(async)]
fn library_get(state: tauri::State<'_, AppState>, id: i64) -> Result<Vec<u8>, String> {
    with_library(&state, |l| l.get_patch(id).map(|p| p.to_blocks()))
}

/// Store a patch (590 bytes of blocks), unless an identical one is already stored. Returns its id and whether it was new.
#[tauri::command(async)]
fn library_add(
    state: tauri::State<'_, AppState>,
    patch: Vec<u8>,
    source: String,
) -> Result<(i64, bool), String> {
    with_library(&state, |l| {
        let p = xppatch_core::Patch::from_blocks(&patch)?;
        Ok(match l.add_patch(&p, &source, now_secs())? {
            xppatch_library::AddOutcome::Added(id) => (id, true),
            xppatch_library::AddOutcome::Duplicate(id) => (id, false),
        })
    })
}

#[tauri::command(async)]
fn library_favourite(state: tauri::State<'_, AppState>, id: i64, on: bool) -> Result<(), String> {
    with_library(&state, |l| l.set_favourite(id, on))
}

#[tauri::command(async)]
fn library_tag_add(state: tauri::State<'_, AppState>, id: i64, tag: String) -> Result<(), String> {
    with_library(&state, |l| l.add_tag(id, &tag))
}

#[tauri::command(async)]
fn library_tag_remove(
    state: tauri::State<'_, AppState>,
    id: i64,
    tag: String,
) -> Result<(), String> {
    with_library(&state, |l| l.remove_tag(id, &tag))
}

#[tauri::command(async)]
fn library_rename(state: tauri::State<'_, AppState>, id: i64, name: String) -> Result<(), String> {
    with_library(&state, |l| l.rename(id, &name))
}

#[tauri::command(async)]
fn library_delete(state: tauri::State<'_, AppState>, id: i64) -> Result<(), String> {
    with_library(&state, |l| l.delete(id))
}

/// The chosen patches as a User bank `.syx` (slots 0, 1, 2 ...), ready to be saved or sent.
#[tauri::command(async)]
fn library_export(
    state: tauri::State<'_, AppState>,
    ids: Vec<i64>,
    device: u8,
) -> Result<Vec<u8>, String> {
    with_library(&state, |l| l.export_syx(&ids, device))
}

// ---- bulks: named, ordered sets of up to 128 patches ----

#[derive(Serialize)]
struct BulkOut {
    id: i64,
    name: String,
    source: String,
    created_at: i64,
    updated_at: i64,
    count: u32,
}

#[derive(Serialize)]
struct BulkSlotOut {
    position: u8,
    patch_id: i64,
    name: String,
}

#[derive(Serialize)]
struct BulkHitOut {
    bulk_id: i64,
    bulk_name: String,
    position: u8,
    patch_id: i64,
    name: String,
}

#[derive(Serialize)]
struct RevisionOut {
    id: i64,
    saved_at: i64,
    count: u32,
}

#[derive(Serialize)]
struct BulkSaveOut {
    bulk_id: i64,
    name: String,
    added: usize,
    reused: usize,
    kept_version: bool,
}

#[tauri::command(async)]
fn bulk_list(state: tauri::State<'_, AppState>) -> Result<Vec<BulkOut>, String> {
    with_library(&state, |l| l.list_bulks()).map(|rows| {
        rows.into_iter()
            .map(|b| BulkOut {
                id: b.id,
                name: b.name,
                source: b.source,
                created_at: b.created_at,
                updated_at: b.updated_at,
                count: b.count,
            })
            .collect()
    })
}

#[tauri::command(async)]
fn bulk_contents(state: tauri::State<'_, AppState>, id: i64) -> Result<Vec<BulkSlotOut>, String> {
    with_library(&state, |l| l.bulk_contents(id)).map(|rows| {
        rows.into_iter()
            .map(|s| BulkSlotOut {
                position: s.position,
                patch_id: s.patch_id,
                name: s.name,
            })
            .collect()
    })
}

/// Every patch of a bulk as `(position, 590 bytes of blocks)`, to load it into the bank.
#[tauri::command(async)]
fn bulk_blocks(state: tauri::State<'_, AppState>, id: i64) -> Result<Vec<(u8, Vec<u8>)>, String> {
    with_library(&state, |l| l.bulk_blocks(id))
}

#[tauri::command(async)]
fn bulk_search(
    state: tauri::State<'_, AppState>,
    text: String,
    limit: u32,
) -> Result<Vec<BulkHitOut>, String> {
    with_library(&state, |l| l.bulk_search(&text, limit)).map(|rows| {
        rows.into_iter()
            .map(|h| BulkHitOut {
                bulk_id: h.bulk_id,
                bulk_name: h.bulk_name,
                position: h.position,
                patch_id: h.patch_id,
                name: h.name,
            })
            .collect()
    })
}

#[tauri::command(async)]
fn bulk_revisions(state: tauri::State<'_, AppState>, id: i64) -> Result<Vec<RevisionOut>, String> {
    with_library(&state, |l| l.bulk_revisions(id)).map(|rows| {
        rows.into_iter()
            .map(|r| RevisionOut {
                id: r.id,
                saved_at: r.saved_at,
                count: r.count,
            })
            .collect()
    })
}

/// Save the bank's patches (`(slot, 590 bytes)` each) as a new bulk, or over the bulk `overwrite` (its old contents are kept as a version).
#[tauri::command(async)]
fn bulk_save(
    state: tauri::State<'_, AppState>,
    overwrite: Option<i64>,
    name: String,
    patches: Vec<(u8, Vec<u8>)>,
) -> Result<BulkSaveOut, String> {
    let target = overwrite.map_or(SaveTarget::New, SaveTarget::Overwrite);
    with_library(&state, |l| l.save_bulk(target, &name, &patches, now_secs())).map(|r| {
        BulkSaveOut {
            bulk_id: r.bulk_id,
            name: r.name,
            added: r.added,
            reused: r.reused,
            kept_version: r.kept_version,
        }
    })
}

#[tauri::command(async)]
fn bulk_rename(state: tauri::State<'_, AppState>, id: i64, name: String) -> Result<(), String> {
    with_library(&state, |l| l.rename_bulk(id, &name, now_secs()))
}

#[tauri::command(async)]
fn bulk_duplicate(state: tauri::State<'_, AppState>, id: i64, name: String) -> Result<i64, String> {
    with_library(&state, |l| l.duplicate_bulk(id, &name, now_secs()))
}

/// Delete a bulk; returns how many patches went with it (those no other bulk, favourite or tag holds).
#[tauri::command(async)]
fn bulk_delete(state: tauri::State<'_, AppState>, id: i64) -> Result<usize, String> {
    with_library(&state, |l| l.delete_bulk(id))
}

#[tauri::command(async)]
fn bulk_place(
    state: tauri::State<'_, AppState>,
    bulk: i64,
    position: u8,
    patch_id: i64,
) -> Result<(), String> {
    with_library(&state, |l| {
        l.bulk_place(bulk, position, patch_id, now_secs())
    })
}

#[tauri::command(async)]
fn bulk_append(state: tauri::State<'_, AppState>, bulk: i64, patch_id: i64) -> Result<u8, String> {
    with_library(&state, |l| l.bulk_append(bulk, patch_id, now_secs()))
}

#[tauri::command(async)]
fn bulk_clear(state: tauri::State<'_, AppState>, bulk: i64, position: u8) -> Result<(), String> {
    with_library(&state, |l| l.bulk_clear(bulk, position, now_secs()))
}

#[tauri::command(async)]
fn bulk_swap(state: tauri::State<'_, AppState>, bulk: i64, a: u8, b: u8) -> Result<(), String> {
    with_library(&state, |l| l.bulk_swap(bulk, a, b, now_secs()))
}

#[tauri::command(async)]
fn bulk_move(
    state: tauri::State<'_, AppState>,
    from_bulk: i64,
    from_position: u8,
    to_bulk: i64,
    to_position: u8,
) -> Result<(), String> {
    with_library(&state, |l| {
        l.bulk_move(
            (from_bulk, from_position),
            (to_bulk, to_position),
            now_secs(),
        )
    })
}

#[tauri::command(async)]
fn bulk_restore(state: tauri::State<'_, AppState>, bulk: i64, revision: i64) -> Result<(), String> {
    with_library(&state, |l| l.restore_revision(bulk, revision, now_secs()))
}

/// The bulk as a User bank `.syx`, each patch at its own position.
#[tauri::command(async)]
fn bulk_export(state: tauri::State<'_, AppState>, id: i64, device: u8) -> Result<Vec<u8>, String> {
    with_library(&state, |l| l.export_bulk(id, device))
}

/// Save a file the user asked for (an export). The window's own download would pop up a browser permission prompt on the second
/// file; this asks for the place with the system's Save dialog instead. Returns the path written, or `None` when the dialog was
/// cancelled. With `XPPATCH_SAVE_DIR` set (automated checks only) the file goes straight into that folder with no dialog.
#[tauri::command(async)]
fn save_file(
    app: tauri::AppHandle,
    name: String,
    bytes: Vec<u8>,
) -> Result<Option<String>, String> {
    use tauri_plugin_dialog::DialogExt;
    let name = storage::safe_export_name(&name)?;
    let path = if let Some(dir) = std::env::var_os("XPPATCH_SAVE_DIR") {
        std::path::PathBuf::from(dir).join(&name)
    } else {
        let ext = name.rsplit_once('.').map_or("syx", |(_, e)| e);
        let kind = if ext == "mid" {
            "Standard MIDI File"
        } else {
            "SysEx file"
        };
        let Some(chosen) = app
            .dialog()
            .file()
            .set_file_name(&name)
            .add_filter(kind, &[ext])
            .blocking_save_file()
        else {
            return Ok(None);
        };
        chosen.into_path().map_err(|e| e.to_string())?
    };
    std::fs::write(&path, &bytes)
        .map_err(|e| format!("could not write {}: {e}", path.display()))?;
    Ok(Some(path.display().to_string()))
}

pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(AppState::default())
        .setup(|app| {
            // A library that cannot be opened must not stop the editor; its commands report why.
            let opened = default_dir(app.handle())
                .map(|d| storage::resolve(&d))
                .and_then(|dir| {
                    std::fs::create_dir_all(&dir)
                        .map(|()| dir)
                        .map_err(|e| e.to_string())
                })
                .inspect(|dir| {
                    let _ = IN_USE.set(dir.clone());
                })
                .and_then(|dir| {
                    Library::open(&dir.join("library.sqlite")).map_err(|e| e.to_string())
                });
            let state = app.state::<AppState>();
            *state
                .library
                .lock()
                .unwrap_or_else(std::sync::PoisonError::into_inner) = opened;
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            midi_ports,
            midi_connect,
            midi_disconnect,
            midi_send_frame,
            midi_fetch_temp_patch,
            midi_fetch_temp_doc,
            midi_poll_stale,
            midi_listen_controls,
            midi_poll_controls,
            midi_note,
            midi_select_sound,
            midi_set_pacing,
            midi_all_notes_off,
            midi_arm_save,
            midi_disarm_save,
            midi_commit_save,
            midi_restore_save,
            midi_arm_system_save,
            save_file,
            midi_disarm_system_save,
            midi_commit_system_save,
            midi_restore_system_save,
            midi_cancel,
            midi_read_bank,
            midi_backup_all,
            midi_arm_bank,
            midi_disarm_bank,
            midi_commit_bank,
            bank_to_syx,
            library_import,
            library_list,
            library_count,
            library_tags,
            library_get,
            library_add,
            library_favourite,
            library_tag_add,
            library_tag_remove,
            library_rename,
            library_delete,
            library_export,
            bulk_list,
            bulk_contents,
            bulk_blocks,
            bulk_search,
            bulk_revisions,
            bulk_save,
            bulk_rename,
            bulk_duplicate,
            bulk_delete,
            bulk_place,
            bulk_append,
            bulk_clear,
            bulk_swap,
            bulk_move,
            bulk_restore,
            bulk_export,
            backups_list,
            backup_read,
            storage_info,
            storage_set
        ])
        .run(tauri::generate_context!())
        .expect("error while running XPPatchStudio");
}
