//! H-063 second half: after a verified System write, a further System change makes Undo's guard refuse restore.
//! SysEx stand-in for a panel edit after the save. Restores the pre-test System settings at the end.
//! Usage: stale_system_undo_probe <input> <output>
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn main() -> Res<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output] = args.as_slice() else {
        return Err("pass input and output names".into());
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();

    // Snapshot the live settings so cleanup can put them back regardless of Chorus/selection drift.
    let baseline_plan = client.plan_system_write(&cancel)?;
    let baseline = baseline_plan.backup().clone();
    let tune0 = baseline
        .block(0)
        .ok_or("missing System Common")?
        .get("master_tune")?;

    // Controlled save: Master Tune +1 (same as H-062).
    let mut wrote = baseline.clone();
    let tune1 = if tune0 < 127 { tune0 + 1 } else { tune0 - 1 };
    wrote
        .block_mut(0)
        .ok_or("missing System Common")?
        .set("master_tune", tune1)?;
    let plan = client.plan_system_write(&cancel)?;
    let report = client.commit_system_write(&plan, &wrote, &cancel)?;
    if !report.verified || client.read_system(&cancel)? != wrote {
        let _ = client.restore_system_backup(&baseline_plan, &cancel);
        return Err("controlled System write of Master Tune did not verify".into());
    }
    println!("Wrote Master Tune {tune0} -> {tune1} (simulating a completed System save)");

    // Post-save change (panel stand-in): flip Master Tune the other way.
    let mut changed = wrote.clone();
    let tune2 = if tune1 < 127 { tune1 + 1 } else { tune1 - 1 };
    if tune2 == tune0 {
        // Keep it different from both the pre-save and the saved value when possible.
        changed
            .block_mut(0)
            .ok_or("missing System Common")?
            .set("master_tune", if tune0 > 0 { tune0 - 1 } else { 2 })?;
    } else {
        changed
            .block_mut(0)
            .ok_or("missing System Common")?
            .set("master_tune", tune2)?;
    }
    let plan2 = client.plan_system_write(&cancel)?;
    let report = client.commit_system_write(&plan2, &changed, &cancel)?;
    if !report.verified {
        let _ = client.restore_system_backup(&baseline_plan, &cancel);
        return Err("post-save System change did not verify".into());
    }
    let now = client.read_system(&cancel)?;
    if now == wrote {
        let _ = client.restore_system_backup(&baseline_plan, &cancel);
        return Err(
            "post-save change did not stick; Undo guard would incorrectly allow restore".into(),
        );
    }
    println!(
        "H-063 STALE UNDO REFUSE PASS: System no longer holds the saved settings; app Undo must refuse (SynthError::SystemChanged)"
    );

    // Cleanup: put the pre-test System back (not the mid-test wrote value).
    let plan3 = client.plan_system_write(&cancel)?;
    let report = client.commit_system_write(&plan3, &baseline, &cancel)?;
    if !report.verified || client.read_system(&cancel)? != baseline {
        return Err("cleanup restore of pre-test System settings failed".into());
    }
    println!("H-063 CLEANUP: System settings restored to pre-test bytes");
    println!("H-063 HARDWARE PASS (stale-undo half; SysEx stand-in for panel change after save)");
    Ok(())
}
