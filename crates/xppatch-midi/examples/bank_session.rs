//! Hardware session, PERMANENT part, bank writes (H-032, H-050b). The whole User bank is read first (the backup, also saved as a file), then:
//!   1. all 128 slots are written with exactly the content they hold now (nothing changes), read back one by one, then the bank is read again and compared;
//!   2. the same write is started and cancelled after a few slots (nothing changes either way), and the bank is compared again;
//!   3. three slots (126 to 128) are given other names and written alone, read back, the rest of the bank is checked untouched, and the three are restored;
//!   4. a last full comparison with the backup, and any slot that differs is put back.
//!
//! cargo run -p xppatch-midi --example bank_session -- <input name> <output name> <backup folder>

use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};
use xppatch_core::{user_patch_bank_bytes, Patch};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig, Progress};
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

fn bank_equal(
    client: &mut Client<MidirTransport>,
    original: &[(u8, Patch)],
    cancel: &CancelToken,
) -> Res<Vec<u8>> {
    let now = client.fetch_bank(0..128u8, cancel, &mut |_| {})?;
    if now.patches.len() != 128 {
        return Err(format!("the comparison read got {} patches", now.patches.len()).into());
    }
    Ok(now
        .patches
        .iter()
        .filter(|(s, p)| original.iter().find(|(o, _)| o == s).map(|(_, q)| q) != Some(p))
        .map(|(s, _)| *s)
        .collect())
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
    let differing = bank_equal(&mut client, &original, &cancel)?;
    say(&format!("slots that differ from the backup: {differing:?}"));
    for s in &differing {
        if let Some(plan) = plans.iter().find(|p| p.slot() == *s) {
            let r = client.restore_user_backup(plan, &cancel)?;
            say(&format!(
                "put slot {} back ({} messages, verified {})",
                u16::from(*s) + 1,
                r.messages,
                r.verified
            ));
        }
    }
    if !differing.is_empty() {
        let again = bank_equal(&mut client, &original, &cancel)?;
        say(&format!("after putting back, slots that differ: {again:?}"));
    }
    result
}

fn steps(
    client: &mut Client<MidirTransport>,
    plans: &[xppatch_midi::WritePlan],
    original: &[(u8, Patch)],
) -> Res<()> {
    let cancel = CancelToken::new();

    say("== H-032 ==");
    let t = Instant::now();
    let mut last_report = 0;
    let report = client.commit_user_bank(plans, original, &cancel, &mut |p: Progress| {
        if p.done >= last_report + 16 {
            last_report = p.done;
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
    let differing = bank_equal(client, original, &cancel)?;
    say(&format!("the bank read again differs from the backup in: {differing:?} (none expected: the content written was the content it already had)"));

    say("== H-032 cancel ==");
    let stop = CancelToken::new();
    let trigger = stop.clone();
    let handle = std::thread::spawn(move || {
        std::thread::sleep(Duration::from_secs(6));
        trigger.cancel();
    });
    let t = Instant::now();
    let report = client.commit_user_bank(plans, original, &stop, &mut |_| {})?;
    let _ = handle.join();
    say(&format!(
        "cancelled after {:?}: {} slots written, cancelled {}, stopped_at {:?}",
        t.elapsed(),
        report.written.len(),
        report.cancelled,
        report.stopped_at
    ));
    let differing = bank_equal(client, original, &cancel)?;
    say(&format!(
        "the bank read again differs from the backup in: {differing:?}"
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
    let three_ok = changed
        .iter()
        .all(|(s, p)| now.patches.iter().find(|(n, _)| n == s).map(|(_, q)| q) == Some(p));
    say(&format!("the three slots hold what was written: {three_ok}; other slots that differ from the backup: {others:?}"));
    // The plans were made before the three slots changed, so a normal save would call them stale; putting a backup back is what restore is for.
    for (s, _) in &changed {
        let plan = plans.iter().find(|p| p.slot() == *s).expect("plan");
        let r = client.restore_user_backup(plan, &cancel)?;
        say(&format!(
            "restored slot {}: {} messages, verified {}",
            u16::from(*s) + 1,
            r.messages,
            r.verified
        ));
    }
    Ok(())
}
