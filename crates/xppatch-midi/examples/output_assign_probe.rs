//! H-068: write Tone Output Assign raw 3 and EFX Output Assign raw 2 to the temporary patch;
//! record whether the XP-60 accepts them on read-back. Also scan a full-bank `.syx` for those values.
//! Restores the original temporary patch.
//! Usage: output_assign_probe <input> <output> [full-backup.syx]
use xppatch_core::{Patch, PatchBlock, SyxImage};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::memory::TEMP_PATCH;
use xppatch_sysex::message::{DeviceId, Message};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn frame_for(patch: &Patch, block: &str, id: &str) -> Res<Vec<u8>> {
    let b = PatchBlock::parse(block).ok_or("block")?;
    let (address, data) = patch.param_dt1(TEMP_PATCH, b, id)?;
    Ok(Message::Dt1 { address, data }.encode(DeviceId::DEFAULT)?)
}

fn main() -> Res<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args.len() < 2 || args.len() > 3 {
        return Err("pass input, output, and optionally a full-backup.syx".into());
    }
    let input = &args[0];
    let output = &args[1];
    let backup = args.get(2);

    if let Some(path) = backup {
        let image = SyxImage::parse(&std::fs::read(path)?)?;
        let mut tone3 = 0usize;
        let mut efx2 = 0usize;
        for (_, patch) in image.user_patches()? {
            if let Some(t) = patch.block(PatchBlock::Tone(0)) {
                if t.get("output_assign").ok() == Some(3) {
                    tone3 += 1;
                }
            }
            if let Some(c) = patch.block(PatchBlock::Common) {
                if c.get("efx_output_assign").ok() == Some(2) {
                    efx2 += 1;
                }
            }
        }
        println!(
            "bank scan {path}: Tone1 output_assign=3 in {tone3}/128; efx_output_assign=2 in {efx2}/128"
        );
    }

    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let original = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    println!("baseline temp '{}'", original.name().trim_end());
    let tone0 = original
        .block(PatchBlock::Tone(0))
        .ok_or("missing Tone 1")?
        .get("output_assign")?;
    let efx0 = original
        .block(PatchBlock::Common)
        .ok_or("missing Common")?
        .get("efx_output_assign")?;
    println!("baseline Tone1 output_assign={tone0}; efx_output_assign={efx0}");

    let mut edited = original.clone();
    edited
        .block_mut(PatchBlock::Tone(0))
        .ok_or("missing Tone 1")?
        .set("output_assign", 3)?;
    client.send_temp_frame(&frame_for(&edited, "tone1", "output_assign")?, &cancel)?;
    let back = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    let tone_back = back
        .block(PatchBlock::Tone(0))
        .ok_or("missing Tone 1")?
        .get("output_assign")?;
    println!(
        "after Tone1 output_assign=3 write: read-back={tone_back} ({})",
        if tone_back == 3 {
            "ACCEPTED"
        } else {
            "NOT KEPT"
        }
    );

    edited
        .block_mut(PatchBlock::Common)
        .ok_or("missing Common")?
        .set("efx_output_assign", 2)?;
    client.send_temp_frame(&frame_for(&edited, "common", "efx_output_assign")?, &cancel)?;
    let back2 = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    let efx_back = back2
        .block(PatchBlock::Common)
        .ok_or("missing Common")?
        .get("efx_output_assign")?;
    println!(
        "after efx_output_assign=2 write: read-back={efx_back} ({})",
        if efx_back == 2 {
            "ACCEPTED"
        } else {
            "NOT KEPT"
        }
    );

    client.send_temp_patch(&original, true, &cancel)?;
    if client.fetch_temp_patch(&cancel, &mut |_| {})? != original {
        return Err("final restore of temporary patch failed".into());
    }
    println!("temporary patch restored");
    println!(
        "H-068 PARTIAL: SysEx accept/reject recorded; owner audible/output routing still open if values were kept"
    );
    Ok(())
}
