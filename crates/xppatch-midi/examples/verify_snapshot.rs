//! Compare a full snapshot with the instrument. Optional recovery only restores User slot 128 and the temporary patch.
//! Usage: verify_snapshot <input> <output> <full.syx> <temp.blocks> [--restore]
use xppatch_core::{DocKind, Patch, SyxImage};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if !(args.len() == 4 || (args.len() == 5 && args[4] == "--restore")) {
        return Err("pass input, output, full.syx, temp.blocks, optionally --restore".into());
    }
    let restore = args.len() == 5;
    let image = SyxImage::parse(&std::fs::read(&args[2])?)?;
    let expected = image.user_patches()?;
    if expected.len() != 128 {
        return Err("snapshot must contain all 128 patches".into());
    }
    let temp = Patch::from_blocks(&std::fs::read(&args[3])?)?;
    let mut client = Client::new(
        MidirTransport::connect(&args[0], &args[1])?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let mut bank = client.fetch_bank(0..128, &cancel, &mut |_| {})?;
    if bank.cancelled {
        return Err("bank read cancelled".into());
    }
    for (slot, first_error) in bank.failures {
        println!(
            "Initial bank read missed User patch {} ({first_error}); retrying that slot independently",
            u16::from(slot) + 1
        );
        bank.patches
            .push((slot, client.read_user_slot(slot, &cancel)?));
    }
    if bank.patches.len() != 128 {
        return Err(format!("incomplete bank read: {} of 128", bank.patches.len()).into());
    }
    let differ: Vec<u8> = expected
        .iter()
        .filter(|(s, p)| bank.patches.iter().find(|(n, _)| n == s).map(|(_, q)| q) != Some(p))
        .map(|(s, _)| *s)
        .collect();
    println!("User slots different from snapshot (zero-based): {differ:?}");
    if !differ.is_empty() {
        if !restore || differ != [127] {
            return Err("unexpected User memory differences; stopped".into());
        }
        let original = &expected
            .iter()
            .find(|(s, _)| *s == 127)
            .ok_or("slot 128 missing")?
            .1;
        let plan = client.plan_user_write(127, &cancel)?;
        let report = client.commit_user_write(&plan, original, &cancel)?;
        if !report.verified || client.read_user_slot(127, &cancel)? != *original {
            return Err("slot 128 restore failed".into());
        }
        println!("User slot 128 restored and independently read back equal");
    }
    for kind in [DocKind::Performance, DocKind::Rhythm] {
        let actual = client.fetch_user_documents(kind, &cancel, &mut |_| {})?;
        let saved = image.user_documents(kind)?;
        if actual != saved {
            return Err(format!("{kind:?} User memory differs").into());
        }
        println!("{kind:?}: {} User documents match snapshot", actual.len());
    }
    if client.read_system(&cancel)?
        != image
            .system_document()?
            .ok_or("System settings missing from snapshot")?
    {
        return Err("System settings differ".into());
    }
    println!("System settings match snapshot");
    if client.fetch_temp_patch(&cancel, &mut |_| {})? != temp {
        if !restore {
            return Err("temporary patch differs".into());
        }
        client.send_temp_patch(&temp, true, &cancel)?;
        if client.fetch_temp_patch(&cancel, &mut |_| {})? != temp {
            return Err("temporary restore failed".into());
        }
        println!("Temporary patch restored and independently read back equal");
    }
    println!("SNAPSHOT VERIFICATION PASSED");
    Ok(())
}
