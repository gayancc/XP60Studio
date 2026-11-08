//! H-044 remaining: in Performance mode, live-edit Part MIDI Channel and Key Range on the temporary Performance.
//! Switches Sound Mode to PERFORMANCE via SysEx (stand-in for the panel mode button), edits, verifies, restores
//! the temporary Performance and the prior System settings (including Patch mode).
//! Usage: perf_live_params_probe <input> <output>
use std::thread;
use std::time::Duration;
use xppatch_core::DocKind;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::chunk::{split_dt1, MAX_DT1_DATA_TO_DEVICE};
use xppatch_sysex::message::Message;

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn send_param(
    client: &mut Client<MidirTransport>,
    doc: &xppatch_core::Document,
    part_index: usize,
    id: &str,
) -> Res<()> {
    let base = DocKind::Performance.temp_base();
    let (address, data) = doc.param_dt1(base, part_index, id)?;
    let frame = Message::Dt1 { address, data }.encode(client.config().device)?;
    client.send_temp_frame(&frame, &CancelToken::new())?;
    Ok(())
}

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

    let sys_plan = client.plan_system_write(&cancel)?;
    let sys_baseline = sys_plan.backup().clone();
    let mode0 = sys_baseline
        .block(0)
        .ok_or("missing System Common")?
        .get("sound_mode")?;

    let mut sys_perf = sys_baseline.clone();
    sys_perf
        .block_mut(0)
        .ok_or("missing System Common")?
        .set("sound_mode", 0)?;
    let plan = client.plan_system_write(&cancel)?;
    let report = client.commit_system_write(&plan, &sys_perf, &cancel)?;
    if !report.verified {
        let _ = client.restore_system_backup(&sys_plan, &cancel);
        return Err("switching Sound Mode to PERFORMANCE did not verify".into());
    }
    thread::sleep(Duration::from_millis(400));
    println!("Sound Mode set to PERFORMANCE (was {mode0})");

    let original = client.fetch_temp_document(DocKind::Performance, &cancel, &mut |_| {})?;
    println!(
        "Temporary Performance '{}', {} blocks",
        original.name().trim_end(),
        original.blocks().len()
    );

    let result = (|| -> Res<()> {
        let mut edited = original.clone();
        let ch0 = edited
            .block(1)
            .ok_or("missing part1")?
            .get("midi_channel")?;
        let ch1 = if ch0 < 15 { ch0 + 1 } else { ch0 - 1 };
        edited
            .block_mut(1)
            .ok_or("missing part1")?
            .set("midi_channel", ch1)?;
        let before = original.to_block_bytes();
        let after = edited.to_block_bytes();
        let diffs = before.iter().zip(&after).filter(|(a, b)| a != b).count();
        if diffs != 1 {
            return Err(format!("MIDI Channel edit should change 1 byte, changed {diffs}").into());
        }
        send_param(&mut client, &edited, 1, "midi_channel")?;
        thread::sleep(Duration::from_millis(80));
        let mid = client.fetch_temp_document(DocKind::Performance, &cancel, &mut |_| {})?;
        if mid.block(1).ok_or("missing part1")?.get("midi_channel")? != ch1 {
            return Err("MIDI Channel live edit did not read back".into());
        }
        println!("H-044 MIDI Channel PASS: part1 {ch0} -> {ch1}");

        let mut edited2 = mid.clone();
        let k0 = edited2
            .block(1)
            .ok_or("missing part1")?
            .get("keyboard_range_lower")?;
        let k1 = if k0 < 127 { k0 + 1 } else { k0 - 1 };
        edited2
            .block_mut(1)
            .ok_or("missing part1")?
            .set("keyboard_range_lower", k1)?;
        send_param(&mut client, &edited2, 1, "keyboard_range_lower")?;
        thread::sleep(Duration::from_millis(80));
        let after = client.fetch_temp_document(DocKind::Performance, &cancel, &mut |_| {})?;
        if after
            .block(1)
            .ok_or("missing part1")?
            .get("keyboard_range_lower")?
            != k1
        {
            return Err("keyboard_range_lower live edit did not read back".into());
        }
        println!("H-044 Key Range PASS: part1 keyboard_range_lower {k0} -> {k1}");
        Ok(())
    })();

    // Restore temporary Performance.
    let base = DocKind::Performance.temp_base();
    for (address, data) in original.to_dt1(base)? {
        for piece in split_dt1(address, &data, MAX_DT1_DATA_TO_DEVICE)? {
            let frame = piece.encode(client.config().device)?;
            client.send_temp_frame(&frame, &cancel)?;
        }
    }
    thread::sleep(Duration::from_millis(300));
    if client.fetch_temp_document(DocKind::Performance, &cancel, &mut |_| {})? != original {
        eprintln!("WARNING: temporary Performance restore did not fully match");
    } else {
        println!("Temporary Performance restored");
    }

    let plan = client.plan_system_write(&cancel)?;
    let report = client.commit_system_write(&plan, &sys_baseline, &cancel)?;
    if !report.verified || client.read_system(&cancel)? != sys_baseline {
        return Err("System restore after Performance-mode probe failed".into());
    }
    println!("System settings restored (Sound Mode back to {mode0})");

    result?;
    println!("H-044 HARDWARE PASS (MIDI Channel + Key Range live edits; audible still owner)");
    Ok(())
}
