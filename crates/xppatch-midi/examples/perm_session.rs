//! Hardware session part 3 (PERMANENT), first step: one User patch slot is written, read back and put back. Nothing else in User memory is touched.
//!
//! cargo run -p xppatch-midi --example perm_session -- <input name> <output name> <slot 1 to 128> <backup folder>
//!
//! Order: a full backup of User patches, performances, rhythm sets and System settings is read and saved first; then the slot is read
//! (its own backup file is saved), a test patch is written and read back, and the original is written back and read back. If any step
//! fails, the original is put back when the slot no longer holds it.

use std::time::{Instant, SystemTime, UNIX_EPOCH};
use xppatch_core::{
    system_document_bytes, user_document_bytes, user_patch_bank_bytes, DocKind, Patch, SyxImage,
};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::message::DeviceId;

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn main() {
    if let Err(e) = run() {
        eprintln!("error: {e}");
        std::process::exit(1);
    }
}

fn secs() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_or(0, |d| d.as_secs())
}

fn run() -> Res<()> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, slot, folder] = a.as_slice() else {
        return Err(
            "pass an input name, an output name, a slot (1 to 128) and a backup folder".into(),
        );
    };
    let slot: u8 = slot.parse::<u8>()?;
    if !(1..=128).contains(&slot) {
        return Err("the slot must be 1 to 128".into());
    }
    let index = slot - 1;
    std::fs::create_dir_all(folder)?;
    let dev = DeviceId::DEFAULT;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    client.set_device(dev);

    println!("== backup ==");
    let t = Instant::now();
    let bank = client.fetch_bank(0..128u8, &cancel, &mut |_| {})?;
    if bank.patches.len() != 128 || !bank.failures.is_empty() {
        return Err(format!(
            "the backup is not whole: {} patches, {} failures. Nothing was written.",
            bank.patches.len(),
            bank.failures.len()
        )
        .into());
    }
    let perfs = client.fetch_user_documents(DocKind::Performance, &cancel, &mut |_| {})?;
    let rhythm = client.fetch_user_documents(DocKind::Rhythm, &cancel, &mut |_| {})?;
    let system = client.read_system(&cancel)?;
    let mut file = user_patch_bank_bytes(&bank.patches, dev, Some(128))?;
    file.extend(user_document_bytes(
        DocKind::Performance,
        &perfs,
        dev,
        Some(128),
    )?);
    file.extend(user_document_bytes(
        DocKind::Rhythm,
        &rhythm,
        dev,
        Some(128),
    )?);
    file.extend(system_document_bytes(&system, dev, Some(128))?);
    let full =
        std::path::Path::new(folder).join(format!("{}-hardware-session-full-backup.syx", secs()));
    std::fs::write(&full, &file)?;
    let back = SyxImage::parse(&std::fs::read(&full)?)?;
    println!(
        "saved {} ({} bytes, {} messages) in {:?}: {} patches, {} performances, {} rhythm sets, System settings; the file parses again with {} patches",
        full.display(),
        file.len(),
        back.records.len(),
        t.elapsed(),
        bank.patches.len(),
        perfs.len(),
        rhythm.len(),
        back.user_patches()?.len()
    );
    let original_from_bank = bank
        .patches
        .iter()
        .find(|(s, _)| *s == index)
        .map(|(_, p)| p.clone())
        .ok_or("slot not in the bank read")?;

    println!("== H-030 ==");
    let plan = client.plan_user_write(index, &cancel)?;
    println!(
        "slot {slot} holds '{}'; it is backed up in memory and equals the bank read: {}",
        plan.occupant_name().trim_end(),
        plan.backup() == &original_from_bank
    );
    let own = std::path::Path::new(folder).join(format!("{}-user-{slot:03}.syx", secs()));
    std::fs::write(
        &own,
        user_patch_bank_bytes(&[(index, plan.backup().clone())], dev, Some(128))?,
    )?;
    println!("the slot's own backup file: {}", own.display());

    let mut test = Patch::init();
    test.set_name("HW TEST 128")?;
    let result = client.commit_user_write(&plan, &test, &cancel);
    match &result {
        Ok(r) => println!("== H-034 ==\nwrote the test patch into slot {slot}: {} messages, {} bytes, verified {}", r.messages, r.bytes, r.verified),
        Err(e) => println!("== H-030 ==\nwrite FAILED: {e}"),
    }

    println!("== H-036 ==");
    let now = client.fetch_user_patch(index, &cancel, &mut |_| {})?;
    println!(
        "fetched slot {slot}: name '{}', equal to what was written: {}",
        now.name().trim_end(),
        now == test
    );

    println!("== H-033 ==");
    let must_restore = now != *plan.backup();
    if must_restore {
        let r = client.restore_user_backup(&plan, &cancel)?;
        println!(
            "restored the backup into slot {slot}: {} messages, verified {}",
            r.messages, r.verified
        );
    } else {
        println!("slot {slot} still holds the original; nothing to restore");
    }
    let after = client.fetch_user_patch(index, &cancel, &mut |_| {})?;
    println!(
        "slot {slot} now: '{}', equal to the original: {}",
        after.name().trim_end(),
        after == *plan.backup()
    );
    if after != *plan.backup() {
        return Err(
            "the slot does not hold the original after the restore: use the backup files".into(),
        );
    }
    result.map(|_| ()).map_err(Into::into)
}
