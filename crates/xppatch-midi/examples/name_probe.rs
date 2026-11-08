//! Hardware session diagnostic for H-050b: a User patch is rewritten with only its name changed and the read-back is compared byte by byte. The original is put back afterwards.
//!
//! cargo run -p xppatch-midi --example name_probe -- <input name> <output name> <slot 1 to 128> <new name>

use xppatch_core::{Patch, PatchBlock};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::message::DeviceId;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, slot, name] = a.as_slice() else {
        return Err("pass an input name, an output name, a slot and a new name".into());
    };
    let index = slot.parse::<u8>()?.checked_sub(1).ok_or("slot 1 to 128")?;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    client.set_device(DeviceId::DEFAULT);
    let plan = client.plan_user_write(index, &cancel)?;
    let original = plan.backup().clone();
    let mut changed: Patch = original.clone();
    changed.set_name(name)?;
    let common = |p: &Patch| {
        p.block(PatchBlock::Common)
            .map(|b| b.bytes().to_vec())
            .unwrap_or_default()
    };
    let (sent, orig) = (common(&changed), common(&original));
    println!(
        "original name {:?}; sent name {:?}",
        original.name(),
        changed.name()
    );
    println!(
        "sent common differs from original common at offsets: {:?}",
        (0..sent.len())
            .filter(|&i| sent[i] != orig[i])
            .collect::<Vec<_>>()
    );
    match client.commit_user_write(&plan, &changed, &cancel) {
        Ok(r) => println!("write ok: {} messages, verified {}", r.messages, r.verified),
        Err(e) => println!("write result: {e}"),
    }
    let back = client.fetch_user_patch(index, &cancel, &mut |_| {})?;
    let got = common(&back);
    let diffs: Vec<(usize, u8, u8)> = (0..sent.len())
        .filter(|&i| sent[i] != got[i])
        .map(|i| (i, sent[i], got[i]))
        .collect();
    println!("read back name {:?}; common bytes that differ from what was sent (offset, sent, read): {diffs:?}", back.name());
    for pb in [
        PatchBlock::Tone(0),
        PatchBlock::Tone(1),
        PatchBlock::Tone(2),
        PatchBlock::Tone(3),
    ] {
        let same = back.block(pb).map(|b| b.bytes().to_vec())
            == changed.block(pb).map(|b| b.bytes().to_vec());
        println!("{} equals what was sent: {same}", pb.label());
    }
    let r = client.restore_user_backup(&plan, &cancel)?;
    let again = client.fetch_user_patch(index, &cancel, &mut |_| {})?;
    println!(
        "restored the original ({} messages, verified {}); the slot equals the original: {}",
        r.messages,
        r.verified,
        again == original
    );
    Ok(())
}
