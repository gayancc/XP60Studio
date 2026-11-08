//! Load PR-A Patch 4 (Nice Piano), change Tone 2 live, read it back, play notes before/after.
//! cargo run -p xppatch-midi --example live_tone_hear -- <input> <output>
use std::thread;
use std::time::Duration;
use xppatch_core::PatchBlock;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::memory::TEMP_PATCH;
use xppatch_sysex::message::{DeviceId, Message};

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

    let system = client.read_system(&cancel)?;
    let common = system.block(0).ok_or("no system common")?;
    let rx = common.get("patch_receive_channel")?;
    if !(0..=15).contains(&rx) {
        return Err(format!("patch_receive_channel out of range: {rx}").into());
    }
    let ch = u8::try_from(rx)?;
    println!("patch_receive_channel: MIDI ch {}", ch + 1);
    println!("sound_mode: {}", common.get("sound_mode")?);

    // PR-A Patch 4 = CC0=81 CC32=0 PC=3 (same as H-070 / select_probe).
    println!("selecting PR-A 004 Nice Piano…");
    client.select_sound(ch, 81, 0, 3)?;
    thread::sleep(Duration::from_millis(400));

    let before = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    let name = before.name().trim_end().to_string();
    let t2 = before.block(PatchBlock::Tone(1)).ok_or("tone2")?;
    let wave_before = t2.get("wave_number")?;
    let switch_before = t2.get("tone_switch")?;
    let level_before = t2.get("tone_level")?;
    println!("temp '{name}': tone2 switch={switch_before} level={level_before} wave_number={wave_before}");
    if name != "Nice Piano" {
        println!("NOTE: expected Nice Piano; continuing with whatever Patch 4 selection loaded");
    }

    println!("NOTE before edit (400ms)…");
    for c in 0..16u8 {
        let _ = client.all_notes_off(c);
    }
    client.send_note(ch, 60, 100, true)?;
    thread::sleep(Duration::from_millis(400));
    client.send_note(ch, 60, 0, false)?;
    client.all_notes_off(ch)?;
    thread::sleep(Duration::from_millis(250));

    let mut edited = before.clone();
    let wave_next = (wave_before + 12).min(254);
    {
        let b = edited.block_mut(PatchBlock::Tone(1)).ok_or("tone2 mut")?;
        b.set("tone_switch", 1)?;
        b.set("wave_number", wave_next)?;
        b.set("tone_level", 100)?;
    }
    println!("live edit Tone 2: ON, level 100, wave_number {wave_before} -> {wave_next}");

    for id in ["tone_switch", "wave_number", "tone_level"] {
        let (address, data) = edited.param_dt1(TEMP_PATCH, PatchBlock::Tone(1), id)?;
        let frame = Message::Dt1 { address, data }.encode(DeviceId::DEFAULT)?;
        client.send_temp_frame(&frame, &cancel)?;
        thread::sleep(Duration::from_millis(25));
    }

    let after = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    let t2a = after.block(PatchBlock::Tone(1)).ok_or("tone2 after")?;
    let wave_after = t2a.get("wave_number")?;
    let switch_after = t2a.get("tone_switch")?;
    let level_after = t2a.get("tone_level")?;
    println!(
        "read-back Tone 2: switch={switch_after} level={level_after} wave_number={wave_after}"
    );
    if switch_after != 1 || wave_after != wave_next || level_after != 100 {
        return Err(format!(
            "live Tone 2 edit did not stick (got switch={switch_after} level={level_after} wave={wave_after})"
        )
        .into());
    }
    println!("LIVE EDIT VERIFIED on device");

    println!("NOTE after edit (800ms) — should differ from the first note…");
    client.send_note(ch, 60, 100, true)?;
    thread::sleep(Duration::from_millis(800));
    client.send_note(ch, 60, 0, false)?;
    client.all_notes_off(ch)?;

    client.send_temp_patch(&before, true, &cancel)?;
    let restored = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    if restored != before {
        return Err("restore of original temp failed".into());
    }
    println!(
        "RESTORED '{}'; tone2 wave={}",
        restored.name().trim_end(),
        restored
            .block(PatchBlock::Tone(1))
            .ok_or("t2")?
            .get("wave_number")?
    );
    println!("DONE");
    Ok(())
}
