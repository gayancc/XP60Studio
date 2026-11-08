//! Hardware session part 2 (TEMP): writes only to the instrument's temporary patch, which the library refuses to leave. The temporary patch
//! is read first and put back at the end, so the sound the keyboard had is the sound it is left with.
//!
//! cargo run -p xppatch-midi --example temp_session -- <input name> <output name> <file to keep the original temp patch in>
//!
//! Each section prints `== H-nnn ==` followed by what was seen.

use std::time::{Duration, Instant};
use xppatch_core::{Patch, PatchBlock};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig, MidiTransport};
use xppatch_sysex::memory::TEMP_PATCH;
use xppatch_sysex::message::{DeviceId, Message};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn frame_for(patch: &Patch, block: &str, id: &str) -> Res<Vec<u8>> {
    let b = PatchBlock::parse(block).ok_or("block")?;
    let (address, data) = patch.param_dt1(TEMP_PATCH, b, id)?;
    Ok(Message::Dt1 { address, data }.encode(DeviceId::DEFAULT)?)
}

/// Blocks (as Common, Tone 1 ..) whose bytes differ between two patches, and the byte offsets inside them.
fn diff(a: &Patch, b: &Patch) -> Vec<(String, Vec<usize>)> {
    let blocks = [
        PatchBlock::Common,
        PatchBlock::Tone(0),
        PatchBlock::Tone(1),
        PatchBlock::Tone(2),
        PatchBlock::Tone(3),
    ];
    blocks
        .iter()
        .filter_map(|&pb| {
            let (x, y) = (a.block(pb)?.bytes(), b.block(pb)?.bytes());
            let at: Vec<usize> = (0..x.len().min(y.len()))
                .filter(|&i| x[i] != y[i])
                .collect();
            (!at.is_empty()).then(|| (pb.label(), at))
        })
        .collect()
}

fn main() {
    if let Err(e) = run() {
        eprintln!("error: {e}");
        std::process::exit(1);
    }
}

fn run() -> Res<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, keep] = args.as_slice() else {
        return Err(
            "pass an input name, an output name and a file for the original temp patch".into(),
        );
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    client.set_device(DeviceId::DEFAULT);

    // The sound the keyboard has now, kept on disk first and put back at the end.
    let original = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    std::fs::write(keep, original.to_blocks())?;
    println!(
        "original temp patch '{}' kept in {keep}",
        original.name().trim_end()
    );

    let result = steps(&mut client, &cancel, &original);
    // Put the sound back whatever happened above.
    client.set_min_dt1_gap(Duration::from_millis(20));
    match client.send_temp_patch(&original, true, &cancel) {
        Ok(r) => println!(
            "== restore ==\noriginal temp patch written back and verified ({} messages)",
            r.messages
        ),
        Err(e) => {
            println!("== restore ==\nWARNING: could not put the original temp patch back: {e}")
        }
    }
    result
}

fn steps(client: &mut Client<MidirTransport>, cancel: &CancelToken, original: &Patch) -> Res<()> {
    println!("== H-020 ==");
    let t = Instant::now();
    let r = client.send_temp_patch(original, true, cancel)?;
    let back = client.fetch_temp_patch(cancel, &mut |_| {})?;
    println!(
        "wrote the patch just read ({} messages, {} bytes), verified {}, read back equal {}, {:?}",
        r.messages,
        r.bytes,
        r.verified,
        diff(original, &back).is_empty(),
        t.elapsed()
    );

    println!("== H-021 ==");
    for gap in [20u64, 15, 10, 5] {
        client.set_min_dt1_gap(Duration::from_millis(gap));
        let ok = (0..10)
            .filter(|_| client.send_temp_patch(original, true, cancel).is_ok())
            .count();
        println!("gap {gap:>2} ms: {ok} of 10 writes read back equal");
    }
    client.set_min_dt1_gap(Duration::from_millis(20));

    println!("== H-025 ==");
    let probes: [(&str, &str, i32); 4] = [
        ("tone1", "tone_level", 90),
        ("tone1", "lfo1_waveform", 3),
        ("tone1", "coarse_tune", 40),
        ("tone1", "wave_number", 12),
    ];
    for (block, id, value) in probes {
        let b = PatchBlock::parse(block).ok_or("block")?;
        let mut edited = original.clone();
        let before = edited.block(b).ok_or("no block")?.get(id)?;
        let value = if before == value { value + 1 } else { value };
        edited.block_mut(b).ok_or("no block")?.set(id, value)?;
        client.send_temp_frame(&frame_for(&edited, block, id)?, cancel)?;
        client.port_mut().sleep(Duration::from_millis(80));
        let back = client.fetch_temp_patch(cancel, &mut |_| {})?;
        let changed = diff(original, &back);
        let now = back.block(b).ok_or("no block")?.get(id)?;
        println!("{block}.{id}: {before} -> {value}; the keyboard now holds {now}; bytes that differ from the original: {changed:?}; matches the app: {}", diff(&edited, &back).is_empty());
        // put that one value back
        client.send_temp_frame(&frame_for(original, block, id)?, cancel)?;
        client.port_mut().sleep(Duration::from_millis(80));
    }

    println!("== H-026 ==");
    let mut tempo = original.clone();
    let before = tempo
        .block(PatchBlock::Common)
        .ok_or("no block")?
        .get("patch_tempo")?;
    let value = if before == 150 { 151 } else { 150 };
    tempo
        .block_mut(PatchBlock::Common)
        .ok_or("no block")?
        .set("patch_tempo", value)?;
    let frame = frame_for(&tempo, "common", "patch_tempo")?;
    println!("one DT1 of {} bytes: {:02X?}", frame.len(), frame);
    client.send_temp_frame(&frame, cancel)?;
    client.port_mut().sleep(Duration::from_millis(80));
    let back = client.fetch_temp_patch(cancel, &mut |_| {})?;
    println!("patch_tempo {before} -> {value}; the keyboard now holds {}; differs from the app only in tempo: {}", back.block(PatchBlock::Common).ok_or("no block")?.get("patch_tempo")?, diff(&tempo, &back).is_empty());
    client.send_temp_frame(&frame_for(original, "common", "patch_tempo")?, cancel)?;
    client.port_mut().sleep(Duration::from_millis(80));

    println!("== H-029 ==");
    let mut dragged = original.clone();
    let started = Instant::now();
    let mut sent = 0;
    let mut last = 0;
    while started.elapsed() < Duration::from_secs(10) {
        let value = i32::try_from(sent % 128)?;
        dragged
            .block_mut(PatchBlock::Tone(0))
            .ok_or("no block")?
            .set("tone_level", value)?;
        client.send_temp_frame(&frame_for(&dragged, "tone1", "tone_level")?, cancel)?;
        last = value;
        sent += 1;
    }
    client.port_mut().sleep(Duration::from_millis(100));
    let back = client.fetch_temp_patch(cancel, &mut |_| {})?;
    let got = back
        .block(PatchBlock::Tone(0))
        .ok_or("no block")?
        .get("tone_level")?;
    println!("{sent} tone-level edits in 10 s (one every {:?}); last sent {last}, the keyboard holds {got}; any other byte changed: {:?}", started.elapsed() / sent, diff(&dragged, &back));
    Ok(())
}
