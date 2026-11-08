//! Diagnose why the XP-60 may be silent: mode, receive channel, temp Tone switches/levels/waves,
//! then send a short audition note on the patch receive channel.
//!
//! cargo run -p xppatch-midi --example silence_probe -- <input> <output>

use std::thread;
use std::time::Duration;
use xppatch_core::PatchBlock;
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

    let system = client.read_system(&cancel)?;
    let common = system.block(0).ok_or("missing System Common")?;
    let sound_mode = common.get("sound_mode")?;
    let rx = common.get("patch_receive_channel")?;
    // System Common display: PERFORMANCE, PATCH, GM (Owner's Manual / parameter map).
    let mode_name = match sound_mode {
        0 => "PERFORMANCE",
        1 => "PATCH",
        2 => "GM",
        _ => "unknown",
    };
    println!("sound_mode: {sound_mode} ({mode_name})");
    println!("patch_receive_channel: {rx} (audition notes use this as MIDI channel index 0..15)");
    for id in [
        "patch_group_type",
        "patch_group_id",
        "patch_number",
        "receive_program_change",
        "receive_bank_select",
    ] {
        match common.get(id) {
            Ok(v) => println!("{id}: {v}"),
            Err(e) => println!("{id}: {e}"),
        }
    }

    let patch = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    println!("temp patch: '{}'", patch.name().trim_end());
    let mut any_on = false;
    for tone in 0..4u8 {
        let b = patch.block(PatchBlock::Tone(tone)).ok_or("missing tone")?;
        let sw = b.get("tone_switch")?;
        let level = b.get("tone_level")?;
        let ty = b.get("wave_group_type")?;
        let id = b.get("wave_group_id")?;
        let num = b.get("wave_number")?;
        let out = b.get("output_assign").unwrap_or(255);
        if sw == 1 && level > 0 {
            any_on = true;
        }
        let wave = match (ty, id) {
            (0, 1) => format!("INT-A#{num}"),
            (0, 2) => format!("INT-B#{num}"),
            (2, n) => format!("EXP-{n}#{num}"),
            _ => format!("type={ty}/id={id}#{num}"),
        };
        println!(
            "tone{}: switch={sw} level={level} wave={wave} output_assign={out}",
            tone + 1
        );
    }
    if let Some(c) = patch.block(PatchBlock::Common) {
        println!(
            "common: patch_level={} structure_1_2={} structure_3_4={}",
            c.get("patch_level").unwrap_or(255),
            c.get("structure_type_1_2").unwrap_or(255),
            c.get("structure_type_3_4").unwrap_or(255),
        );
    }
    if !any_on {
        println!("DIAG: no Tone is both ON and level>0 — temporary patch itself is silent");
    }
    if sound_mode != 1 {
        println!(
            "DIAG: sound_mode is not PATCH (1). Temporary Patch edits may not be what MIDI/keys are playing."
        );
    }

    let ch = if rx <= 15 { rx as u8 } else { 0 };
    for c in 0..16u8 {
        let _ = client.all_notes_off(c);
    }
    println!("sending Note On ch={} note=60 vel=100 for 400ms", ch + 1);
    client.send_note(ch, 60, 100, true)?;
    thread::sleep(Duration::from_millis(400));
    client.send_note(ch, 60, 0, false)?;
    client.all_notes_off(ch)?;
    println!("NOTE BURST SENT — owner should hear a short middle-C if Patch mode + temp can sound");
    Ok(())
}
