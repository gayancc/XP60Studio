//! H-037/H-049: send ten patches exported by the native Library, then restore from a full-bank .syx backup.
//! Requires User Memory Protect OFF. Every User slot is read before writing; backup is created and synced first.
//! Usage: library_export_roundtrip <input> <output> <baseline-full.syx> <library-ten.syx> <backup-dir>
use std::fs::OpenOptions;
use std::io::Write;
use std::time::{SystemTime, UNIX_EPOCH};
use xppatch_core::{user_patch_bank_bytes, Patch, SyxImage};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig, WritePlan};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn one_based(slots: &[u8]) -> Vec<u16> {
    slots.iter().map(|s| u16::from(*s) + 1).collect()
}

fn read_slot_with_retry(client: &mut Client<MidirTransport>, slot: u8) -> Res<Patch> {
    let cancel = CancelToken::new();
    match client.read_user_slot(slot, &cancel) {
        Ok(patch) => Ok(patch),
        Err(first) => {
            println!(
                "User slot {} first read failed ({first}); retrying independently",
                u16::from(slot) + 1
            );
            Ok(client.read_user_slot(slot, &cancel)?)
        }
    }
}

/// Read every User slot. Retries `failures` and any slots never attempted after an aborted run.
fn read_all(client: &mut Client<MidirTransport>) -> Res<Vec<(u8, Patch)>> {
    let cancel = CancelToken::new();
    let mut bank = client.fetch_bank(0..128, &cancel, &mut |_| {})?;
    if bank.cancelled {
        return Err("bank read cancelled".into());
    }
    let mut have: std::collections::BTreeSet<u8> =
        bank.patches.iter().map(|(slot, _)| *slot).collect();
    for (slot, first_error) in bank.failures {
        println!(
            "Bank read missed User patch {} ({first_error}); retrying that slot independently",
            u16::from(slot) + 1
        );
        bank.patches
            .push((slot, read_slot_with_retry(client, slot)?));
        have.insert(slot);
    }
    if bank.aborted {
        for slot in 0..128u8 {
            if have.contains(&slot) {
                continue;
            }
            println!(
                "Bank read aborted before User patch {}; reading that slot independently",
                u16::from(slot) + 1
            );
            bank.patches
                .push((slot, read_slot_with_retry(client, slot)?));
            have.insert(slot);
        }
    }
    if bank.patches.len() != 128 {
        return Err(format!(
            "incomplete full User bank read: {} of 128",
            bank.patches.len()
        )
        .into());
    }
    bank.patches.sort_by_key(|(slot, _)| *slot);
    Ok(bank.patches)
}

fn differences(expected: &[(u8, Patch)], actual: &[(u8, Patch)]) -> Vec<u8> {
    expected
        .iter()
        .filter(|(s, p)| actual.iter().find(|(n, _)| n == s).map(|(_, q)| q) != Some(p))
        .map(|(s, _)| *s)
        .collect()
}

fn restore_one(client: &mut Client<MidirTransport>, slot: u8, original: &Patch) -> Res<()> {
    let cancel = CancelToken::new();
    let try_once = |client: &mut Client<MidirTransport>| -> Res<()> {
        let plan = client.plan_user_write(slot, &cancel)?;
        if plan.backup() == original {
            println!(
                "User slot {} already equals the prewrite backup",
                u16::from(slot) + 1
            );
            return Ok(());
        }
        let report = client.commit_user_write(&plan, original, &cancel)?;
        if !report.verified {
            return Err(
                format!("User slot {} restore was not verified", u16::from(slot) + 1).into(),
            );
        }
        println!(
            "Restored User slot {} from the full-bank .syx backup",
            u16::from(slot) + 1
        );
        Ok(())
    };
    match try_once(client) {
        Ok(()) => Ok(()),
        Err(first) => {
            println!(
                "User slot {} restore failed ({first}); retrying once",
                u16::from(slot) + 1
            );
            try_once(client)
        }
    }
}

/// Put slots 1-10 back from the prewrite backup first, then prove the whole bank.
/// A flaky mid-bank read must not leave the ten exported positions holding the Library export
/// (that leftover was the first H-037 abort on User 66).
fn restore_ten_then_verify(
    client: &mut Client<MidirTransport>,
    backup: &[(u8, Patch)],
    export: &[(u8, Patch)],
) -> Res<()> {
    let mut restore_errors = Vec::new();
    for slot in 0..10u8 {
        let original = &backup
            .iter()
            .find(|(s, _)| *s == slot)
            .ok_or("backup slot missing")?
            .1;
        let exported = &export
            .iter()
            .find(|(s, _)| *s == slot)
            .ok_or("export slot missing")?
            .1;
        match read_slot_with_retry(client, slot) {
            Ok(live) if &live == original => {
                println!(
                    "User slot {} already original before restore write",
                    u16::from(slot) + 1
                );
                continue;
            }
            Ok(live) if &live == exported => {}
            Ok(_) => {
                println!(
                    "WARNING: User slot {} matches neither export nor backup; restoring backup anyway",
                    u16::from(slot) + 1
                );
            }
            Err(error) => {
                println!(
                    "WARNING: could not pre-check User slot {} ({error}); attempting restore from backup",
                    u16::from(slot) + 1
                );
            }
        }
        if let Err(error) = restore_one(client, slot, original) {
            restore_errors.push(format!("slot {}: {error}", u16::from(slot) + 1));
        }
    }
    if !restore_errors.is_empty() {
        return Err(format!(
            "targeted restore of slots 1-10 incomplete: {}",
            restore_errors.join("; ")
        )
        .into());
    }

    let final_bank = read_all(client)?;
    let remaining = differences(backup, &final_bank);
    if !remaining.is_empty() {
        return Err(format!(
            "final User bank differs in slots {:?} (one-based)",
            one_based(&remaining)
        )
        .into());
    }
    println!("FULL USER BANK RESTORE VERIFIED: all 128 slots equal backup");
    Ok(())
}

fn run() -> Res<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, snapshot, export_path, backup_dir] = args.as_slice() else {
        return Err("pass input, output, baseline-full.syx, library-ten.syx, backup-dir".into());
    };
    let baseline = SyxImage::parse(&std::fs::read(snapshot)?)?.user_patches()?;
    if baseline.len() != 128 {
        return Err("baseline must hold all 128 User patches".into());
    }
    let ten = SyxImage::parse(&std::fs::read(export_path)?)?.user_patches()?;
    if ten.len() != 10
        || ten
            .iter()
            .enumerate()
            .any(|(i, (s, _))| usize::from(*s) != i)
    {
        return Err("Library export must hold exactly slots 1 through 10".into());
    }
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let before = read_all(&mut client)?;
    let drift = differences(&baseline, &before);
    if !drift.is_empty() {
        return Err(format!(
            "current User bank differs from session snapshot in slots {:?} (one-based); no write made",
            one_based(&drift)
        )
        .into());
    }
    // Plan only the ten exported slots. Avoid plan_user_bank: one flaky mid-bank reply previously
    // aborted after the ten writes had already verified (User 66 incomplete during restore).
    let cancel = CancelToken::new();
    let mut plans: Vec<WritePlan> = Vec::with_capacity(10);
    for slot in 0..10u8 {
        plans.push(client.plan_user_write(slot, &cancel)?);
    }
    std::fs::create_dir_all(backup_dir)?;
    let stamp = SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs();
    let file = std::path::Path::new(backup_dir)
        .join(format!("{stamp}-library-roundtrip-prewrite-user-bank.syx"));
    let bytes = user_patch_bank_bytes(
        &before,
        xppatch_sysex::message::DeviceId::DEFAULT,
        Some(128),
    )?;
    let mut handle = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(&file)?;
    handle.write_all(&bytes)?;
    handle.sync_all()?;
    drop(handle);
    let backed = SyxImage::parse(&std::fs::read(&file)?)?.user_patches()?;
    if backed != before {
        return Err("fresh backup file differs from instrument; no write made".into());
    }
    println!(
        "PREWRITE FULL BANK BACKUP: {} ({} bytes)",
        file.display(),
        bytes.len()
    );

    let send_result = (|| -> Res<()> {
        let report = client.commit_user_bank(&plans, &ten, &CancelToken::new(), &mut |_| {})?;
        println!("Library export send report: {report:?}");
        if report.cancelled || report.stopped_at.is_some() || report.written.len() != 10 {
            return Err(
                format!("Library export send stopped before all ten slots: {report:?}").into(),
            );
        }
        let actual = read_all(&mut client)?;
        let expected: Vec<_> = before
            .iter()
            .map(|(s, p)| {
                (
                    *s,
                    ten.iter()
                        .find(|(n, _)| n == s)
                        .map(|(_, q)| q.clone())
                        .unwrap_or_else(|| p.clone()),
                )
            })
            .collect();
        let mismatched = differences(&expected, &actual);
        if !mismatched.is_empty() {
            return Err(format!(
                "User bank read differs from the ten-patch Library export in slots {:?} (one-based)",
                one_based(&mismatched)
            )
            .into());
        }
        println!(
            "LIBRARY EXPORT VERIFIED: ten exported positions match; all other User slots unchanged"
        );
        Ok(())
    })();

    // Always attempt to put the ten slots back after any send attempt that may have written.
    let restore_result = restore_ten_then_verify(&mut client, &backed, &ten);
    match (send_result, restore_result) {
        (Ok(()), Ok(())) => {
            println!("LIBRARY EXPORT AND FULL-BANK BACKUP ROUNDTRIP PASSED");
            Ok(())
        }
        (Ok(()), Err(restore_error)) => Err(format!(
            "send verified, but restore/verification failed: {restore_error}"
        )
        .into()),
        (Err(send_error), Ok(())) => Err(format!(
            "send/verify failed ({send_error}); User bank was restored to the prewrite backup"
        )
        .into()),
        (Err(send_error), Err(restore_error)) => Err(format!(
            "send/verify failed ({send_error}); restore also failed ({restore_error}); use recover_library_ten with {}",
            file.display()
        )
        .into()),
    }
}

fn main() {
    if let Err(error) = run() {
        eprintln!("ERROR: {error}");
        std::process::exit(1);
    }
}
