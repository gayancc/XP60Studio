//! Hardware session, PERMANENT part, real bank writes with User Memory Protect OFF (H-032, H-050b). The whole User bank is read first (the backup, also saved as a file).
//!   1. H-032: every slot is renamed "RT nnn" and written, each read back; the bank is read again and compared with what was written;
//!   2. a restore of the whole bank is started and cancelled after a few seconds (cancel stops within one slot), then finished;
//!   3. the bank is compared with the backup;
//!   4. H-050b: three slots (126 to 128) are renamed and written alone, the rest of the bank is checked untouched, and the three are restored;
//!   5. a last full comparison with the backup; any slot that differs is put back.
//!
//! cargo run -p xppatch-midi --example bank_real -- <input name> <output name> <backup folder>

use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};
use xppatch_core::{user_patch_bank_bytes, Patch};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig, ClientError, Progress, WritePlan};
use xppatch_sysex::message::DeviceId;

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn main() {
    if let Err(e) = run() {
        eprintln!("error: {e}");
        std::process::exit(1);
    }
}

fn say(text: &str) {
    println!("{text}");
}

fn differing(
    client: &mut Client<MidirTransport>,
    expected: &[(u8, Patch)],
    cancel: &CancelToken,
) -> Res<Vec<u8>> {
    let now = client.fetch_bank(0..128u8, cancel, &mut |_| {})?;
    if now.patches.len() != 128 {
        return Err(format!("the comparison read got {} patches", now.patches.len()).into());
    }
    Ok(now
        .patches
        .iter()
        .filter(|(s, p)| expected.iter().find(|(o, _)| o == s).map(|(_, q)| q) != Some(p))
        .map(|(s, _)| *s)
        .collect())
}

fn restore(
    client: &mut Client<MidirTransport>,
    plans: &[WritePlan],
    slots: &[u8],
    cancel: &CancelToken,
) -> Res<usize> {
    let mut done = 0;
    for s in slots {
        let plan = plans.iter().find(|p| p.slot() == *s).ok_or("no plan")?;
        match client.restore_user_backup(plan, cancel) {
            Ok(_) => done += 1,
            Err(ClientError::Cancelled) => break,
            Err(e) => return Err(e.into()),
        }
    }
    Ok(done)
}

fn run() -> Res<()> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, folder] = a.as_slice() else {
        return Err("pass an input name, an output name and a backup folder".into());
    };
    std::fs::create_dir_all(folder)?;
    let dev = DeviceId::DEFAULT;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    client.set_device(dev);

    say("== backup ==");
    let t = Instant::now();
    let plans = client.plan_user_bank(&cancel, &mut |_| {})?;
    let original: Vec<(u8, Patch)> = plans
        .iter()
        .map(|p| (p.slot(), p.backup().clone()))
        .collect();
    let secs = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_or(0, |d| d.as_secs());
    let file = std::path::Path::new(folder).join(format!("{secs}-user-bank.syx"));
    std::fs::write(&file, user_patch_bank_bytes(&original, dev, Some(128))?)?;
    say(&format!(
        "read and backed up {} patches in {:?}; saved {}",
        original.len(),
        t.elapsed(),
        file.display()
    ));

    let result = steps(&mut client, &plans, &original);

    say("== final check ==");
    let differs = differing(&mut client, &original, &cancel)?;
    say(&format!("slots that differ from the backup: {differs:?}"));
    if !differs.is_empty() {
        let n = restore(&mut client, &plans, &differs, &cancel)?;
        let again = differing(&mut client, &original, &cancel)?;
        say(&format!(
            "put {n} slot(s) back; slots that differ now: {again:?}"
        ));
    }
    result
}

fn steps(
    client: &mut Client<MidirTransport>,
    plans: &[WritePlan],
    original: &[(u8, Patch)],
) -> Res<()> {
    let cancel = CancelToken::new();

    say("== H-032 ==");
    let renamed: Vec<(u8, Patch)> = original
        .iter()
        .map(|(s, p)| {
            let mut q = p.clone();
            q.set_name(&format!("RT {:03}", u16::from(*s) + 1))
                .expect("name");
            (*s, q)
        })
        .collect();
    let t = Instant::now();
    let mut next = 16;
    let report = client.commit_user_bank(plans, &renamed, &cancel, &mut |p: Progress| {
        if p.done >= next {
            next = p.done + 16;
            println!(
                "  {} {}/{} after {:?}",
                p.what,
                p.done,
                p.total,
                t.elapsed()
            );
        }
    })?;
    say(&format!(
        "wrote {} of 128 slots, stopped_at {:?}, cancelled {}, in {:?}",
        report.written.len(),
        report.stopped_at,
        report.cancelled,
        t.elapsed()
    ));
    let wrong = differing(client, &renamed, &cancel)?;
    say(&format!(
        "the bank read again differs from what was written in: {wrong:?} (none expected)"
    ));
    if report.written.len() != 128 {
        return Err(
            "the bank write did not complete; stopping before anything else is tried".into(),
        );
    }

    say("== H-032 cancel ==");
    let stop = CancelToken::new();
    let trigger = stop.clone();
    let handle = std::thread::spawn(move || {
        std::thread::sleep(Duration::from_secs(6));
        trigger.cancel();
    });
    let t = Instant::now();
    let all: Vec<u8> = (0..128u8).collect();
    let done = restore(client, plans, &all, &stop)?;
    let _ = handle.join();
    say(&format!("a restore of the whole bank, cancelled after {:?}: {done} slot(s) put back, then it stopped", t.elapsed()));
    let mixed = differing(client, original, &cancel)?;
    say(&format!(
        "slots still different from the backup after the cancel: {} (expected {})",
        mixed.len(),
        128 - done
    ));
    let rest = restore(client, plans, &mixed, &cancel)?;
    say(&format!("finished the restore: {rest} more slot(s)"));
    let left = differing(client, original, &cancel)?;
    say(&format!(
        "slots that differ from the backup now: {left:?} (none expected)"
    ));

    say("== H-050b ==");
    let changed: Vec<(u8, Patch)> = [125u8, 126, 127]
        .iter()
        .zip(["CHG 126", "CHG 127", "CHG 128"])
        .map(|(s, name)| {
            let mut p = original
                .iter()
                .find(|(o, _)| o == s)
                .map(|(_, p)| p.clone())
                .expect("slot");
            p.set_name(name).expect("name");
            (*s, p)
        })
        .collect();
    let report = client.commit_user_bank(plans, &changed, &cancel, &mut |_| {})?;
    say(&format!(
        "wrote the 3 changed slots: written {:?}, stopped_at {:?}",
        report.written, report.stopped_at
    ));
    let now = client.fetch_bank(0..128u8, &cancel, &mut |_| {})?;
    let others: Vec<u8> = now
        .patches
        .iter()
        .filter(|(s, p)| {
            !changed.iter().any(|(c, _)| c == s)
                && original.iter().find(|(o, _)| o == s).map(|(_, q)| q) != Some(p)
        })
        .map(|(s, _)| *s)
        .collect();
    let three = changed
        .iter()
        .all(|(s, p)| now.patches.iter().find(|(n, _)| n == s).map(|(_, q)| q) == Some(p));
    say(&format!("the three slots hold what was written: {three}; other slots that differ from the backup: {others:?}"));
    let n = restore(client, plans, &[125, 126, 127], &cancel)?;
    say(&format!("restored {n} slot(s)"));
    Ok(())
}
