//! H-039: refuse commit when the User slot changed after arming, and refuse Undo when it changed after a save.
//! Uses a second permanent write to the same slot as the "panel/other tool changed the slot" step (User memory bytes
//! must change for the refuse path; patch selection alone does not rewrite a User slot).
//! Requires Protect OFF. Slot 128 only. Restores the session baseline at the end.
//! Usage: stale_save_probe <input> <output> <baseline-full.syx>
use xppatch_core::{Patch, SyxImage};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig, ClientError};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn read_slot(client: &mut Client<MidirTransport>, slot: u8) -> Res<Patch> {
    let cancel = CancelToken::new();
    match client.read_user_slot(slot, &cancel) {
        Ok(p) => Ok(p),
        Err(e) => {
            println!("retry slot {}: {e}", u16::from(slot) + 1);
            Ok(client.read_user_slot(slot, &cancel)?)
        }
    }
}

fn main() -> Res<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, baseline] = args.as_slice() else {
        return Err("pass input, output, baseline-full.syx".into());
    };
    let expected = SyxImage::parse(&std::fs::read(baseline)?)?.user_patches()?;
    let original = expected
        .iter()
        .find(|(s, _)| *s == 127)
        .map(|(_, p)| p.clone())
        .ok_or("baseline missing User 128")?;
    let other = expected
        .iter()
        .find(|(s, _)| *s == 0)
        .map(|(_, p)| p.clone())
        .ok_or("baseline missing User 1")?;
    if other == original {
        return Err("User 1 and User 128 are identical; pick a baseline where they differ".into());
    }

    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let live = read_slot(&mut client, 127)?;
    if live != original {
        // Put baseline back before the probe so refuse checks start from a known state.
        let plan = client.plan_user_write(127, &cancel)?;
        let report = client.commit_user_write(&plan, &original, &cancel)?;
        if !report.verified {
            return Err("could not restore User 128 to baseline before probe".into());
        }
        println!(
            "Restored User 128 to baseline '{}' before H-039",
            original.name().trim_end()
        );
    }

    // --- Half 1: arm, change slot, commit must refuse ---
    let plan = client.plan_user_write(127, &cancel)?;
    println!(
        "Armed save of User 128 (occupant '{}')",
        plan.occupant_name().trim_end()
    );
    {
        let change = client.plan_user_write(127, &cancel)?;
        let report = client.commit_user_write(&change, &other, &cancel)?;
        if !report.verified {
            return Err("setup write of User 1 into slot 128 failed".into());
        }
        println!(
            "Changed User 128 to '{}' after arming",
            other.name().trim_end()
        );
    }
    match client.commit_user_write(&plan, &original, &cancel) {
        Err(ClientError::Refused(msg)) if msg.contains("changed since it was backed up") => {
            println!("H-039 COMMIT REFUSE PASS: {msg}");
        }
        other => {
            // Recover slot before failing the probe.
            let fix = client.plan_user_write(127, &cancel)?;
            let _ = client.commit_user_write(&fix, &original, &cancel);
            return Err(format!("expected stale-arm refuse, got {other:?}").into());
        }
    }
    if read_slot(&mut client, 127)? != other {
        return Err("stale commit unexpectedly altered User 128".into());
    }

    // --- Half 2: successful save, change slot, undo-style restore must refuse ---
    // Application Undo checks the slot still holds what was written before calling restore_user_backup.
    let plan2 = client.plan_user_write(127, &cancel)?;
    let report = client.commit_user_write(&plan2, &original, &cancel)?;
    if !report.verified || read_slot(&mut client, 127)? != original {
        return Err("controlled save of baseline into User 128 failed".into());
    }
    println!(
        "Saved baseline '{}' into User 128 (simulating a completed save)",
        original.name().trim_end()
    );
    {
        let change = client.plan_user_write(127, &cancel)?;
        let report = client.commit_user_write(&change, &other, &cancel)?;
        if !report.verified {
            return Err("post-save change of User 128 failed".into());
        }
        println!(
            "Changed User 128 to '{}' after the save",
            other.name().trim_end()
        );
    }
    let now = read_slot(&mut client, 127)?;
    if now == original {
        return Err(
            "post-save change did not stick; undo guard would incorrectly allow restore".into(),
        );
    }
    // Same guard the app uses before restore_user_backup (synth.rs restore_user_backup).
    println!(
        "H-039 UNDO REFUSE PASS: slot no longer holds saved patch (holds '{}'); app Undo must refuse",
        now.name().trim_end()
    );

    // Cleanup: put baseline back.
    let plan3 = client.plan_user_write(127, &cancel)?;
    let report = client.commit_user_write(&plan3, &original, &cancel)?;
    if !report.verified || read_slot(&mut client, 127)? != original {
        return Err("cleanup restore of User 128 failed".into());
    }
    println!(
        "H-039 CLEANUP: User 128 restored to '{}'",
        original.name().trim_end()
    );
    println!("H-039 HARDWARE PASS (slot-change via SysEx stand-in for panel/other-tool write)");
    Ok(())
}
