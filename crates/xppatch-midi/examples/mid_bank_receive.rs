//! H-053: receive a Bank/Library `.mid` export on the XP-60 by extracting its SysEx and writing the patches.
//! Requires User Memory Protect OFF. Restores any changed slots from a full-bank `.syx` backup afterward.
//! Usage: mid_bank_receive <input> <output> <bank.mid> <prewrite-full.syx> <slot-count>
//! `slot-count` is 1..128: how many leading User slots from the MIDI file to write (use 10 for a short check).
use xppatch_core::smf::smf_to_syx;
use xppatch_core::{Patch, SyxImage};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn read_slot(client: &mut Client<MidirTransport>, slot: u8) -> Res<Patch> {
    let cancel = CancelToken::new();
    match client.read_user_slot(slot, &cancel) {
        Ok(p) => Ok(p),
        Err(first) => {
            println!(
                "User slot {} first read failed ({first}); retrying",
                u16::from(slot) + 1
            );
            Ok(client.read_user_slot(slot, &cancel)?)
        }
    }
}

fn main() -> Res<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, mid_file, backup_file, count_text] = args.as_slice() else {
        return Err("pass input, output, bank.mid, prewrite-full.syx, slot-count".into());
    };
    let count: usize = count_text.parse()?;
    if !(1..=128).contains(&count) {
        return Err("slot-count must be 1..128".into());
    }
    let syx = smf_to_syx(&std::fs::read(mid_file)?)?;
    let from_mid = SyxImage::parse(&syx)?.user_patches()?;
    if from_mid.len() < count {
        return Err(format!("MIDI file decoded only {} patches", from_mid.len()).into());
    }
    let originals = SyxImage::parse(&std::fs::read(backup_file)?)?.user_patches()?;
    if originals.len() != 128 {
        return Err("prewrite backup must hold all 128 User patches".into());
    }
    let ten: Vec<(u8, Patch)> = (0..count as u8)
        .map(|slot| {
            from_mid
                .iter()
                .find(|(s, _)| *s == slot)
                .map(|(s, p)| (*s, p.clone()))
                .ok_or_else(|| {
                    format!("MIDI file missing User slot {}", u16::from(slot) + 1).into()
                })
        })
        .collect::<Result<Vec<_>, Box<dyn std::error::Error>>>()?;

    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    for slot in 0..count as u8 {
        let live = read_slot(&mut client, slot)?;
        let original = &originals
            .iter()
            .find(|(s, _)| *s == slot)
            .ok_or("backup slot missing")?
            .1;
        if &live != original {
            return Err(format!(
                "User slot {} differs from prewrite backup before the MIDI send; refused",
                u16::from(slot) + 1
            )
            .into());
        }
    }

    let mut plans = Vec::with_capacity(count);
    for slot in 0..count as u8 {
        plans.push(client.plan_user_write(slot, &cancel)?);
    }
    let report = client.commit_user_bank(&plans, &ten, &cancel, &mut |_| {})?;
    println!("MIDI-derived send report: {report:?}");
    if report.cancelled || report.stopped_at.is_some() || report.written.len() != count {
        return Err(format!("MIDI-derived send incomplete: {report:?}").into());
    }
    for (slot, expected) in &ten {
        let live = read_slot(&mut client, *slot)?;
        if &live != expected {
            return Err(format!(
                "User slot {} did not match the MIDI export after send",
                u16::from(*slot) + 1
            )
            .into());
        }
    }
    println!(
        "H-053 RECEIVE PASSED: {} slot(s) from {} match the instrument",
        count, mid_file
    );

    // Restore from the full-bank .syx backup (same MIDI-tool path as H-049 for these slots).
    let mut restore_errors = Vec::new();
    for slot in 0..count as u8 {
        let original = &originals
            .iter()
            .find(|(s, _)| *s == slot)
            .ok_or("backup slot missing")?
            .1;
        let plan = client.plan_user_write(slot, &cancel)?;
        if plan.backup() == original {
            continue;
        }
        match client.commit_user_write(&plan, original, &cancel) {
            Ok(r) if r.verified => println!(
                "Restored User slot {} from prewrite backup",
                u16::from(slot) + 1
            ),
            Ok(_) => restore_errors.push(format!("slot {} not verified", u16::from(slot) + 1)),
            Err(e) => restore_errors.push(format!("slot {}: {e}", u16::from(slot) + 1)),
        }
    }
    if !restore_errors.is_empty() {
        return Err(format!("restore incomplete: {}", restore_errors.join("; ")).into());
    }
    for slot in 0..count as u8 {
        let original = &originals
            .iter()
            .find(|(s, _)| *s == slot)
            .ok_or("backup slot missing")?
            .1;
        if &read_slot(&mut client, slot)? != original {
            return Err(format!(
                "User slot {} differs from backup after restore",
                u16::from(slot) + 1
            )
            .into());
        }
    }
    println!(
        "H-053 CLEANUP PASSED: restored {} slot(s) from prewrite backup",
        count
    );
    Ok(())
}
