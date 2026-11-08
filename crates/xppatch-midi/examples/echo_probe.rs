//! Soft Thru / loopback check for H-018: measure whether this MIDI path echoes our own DT1
//! writes back onto the input before (or instead of) the instrument's reply.
//! Does not invent an XP-60 Soft Thru switch; reports only what this cable/interface does.
//! Restores the temporary patch afterward.
//! Usage: echo_probe <input> <output>
use std::time::{Duration, Instant};
use xppatch_core::PatchBlock;
use xppatch_midi::assembler::{Incoming, SysExAssembler};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig, MidiTransport};
use xppatch_sysex::memory::TEMP_PATCH;
use xppatch_sysex::message::{DeviceId, Message};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn main() -> Res<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output] = args.as_slice() else {
        return Err("pass input and output names".into());
    };

    // Use Client only to build a real DT1 and to restore afterward.
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let original = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    println!("baseline temp '{}'", original.name().trim_end());

    let mut edited = original.clone();
    let before = edited
        .block(PatchBlock::Tone(0))
        .ok_or("missing Tone 1")?
        .get("tone_level")?;
    let value = if before < 127 { before + 1 } else { before - 1 };
    edited
        .block_mut(PatchBlock::Tone(0))
        .ok_or("missing Tone 1")?
        .set("tone_level", value)?;
    let (address, data) = edited.param_dt1(TEMP_PATCH, PatchBlock::Tone(0), "tone_level")?;
    let frame = Message::Dt1 { address, data }.encode(DeviceId::DEFAULT)?;
    drop(client); // release ports before opening a raw transport

    let mut port = MidirTransport::connect(input, output)?;
    let mut asm = SysExAssembler::new();
    // Drain pending.
    while let Some(chunk) = port.recv(Duration::from_millis(20))? {
        let _ = asm.feed(&chunk);
    }
    asm.reset();

    let t0 = Instant::now();
    port.send(&frame)?;
    let window = Duration::from_millis(80) + Duration::from_micros(400 * frame.len() as u64);
    let mut frames: Vec<Vec<u8>> = Vec::new();
    while t0.elapsed() < window {
        if let Some(chunk) = port.recv(Duration::from_millis(10))? {
            for item in asm.feed(&chunk) {
                if let Incoming::SysEx(bytes) = item {
                    frames.push(bytes);
                }
            }
        }
    }
    let exact_echo = frames.iter().any(|f| f == &frame);
    let other = frames.len().saturating_sub(usize::from(exact_echo));
    println!(
        "sent {} byte DT1; listened {:?}; inbound SysEx frames={}; exact outbound echo={exact_echo}; other frames={other}",
        frame.len(),
        t0.elapsed(),
        frames.len(),
    );
    for (i, f) in frames.iter().enumerate() {
        let kind = if f == &frame { "ECHO" } else { "other" };
        println!(
            "  [{i}] {kind} len={} head={:02X?}",
            f.len(),
            &f[..f.len().min(8)]
        );
    }
    drop(port);

    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    client.send_temp_patch(&original, true, &cancel)?;
    if client.fetch_temp_patch(&cancel, &mut |_| {})? != original {
        return Err("final restore of temporary patch failed".into());
    }
    println!("temporary patch restored");
    if exact_echo {
        println!(
            "H-018 PATH NOTE: this interface path echoed our DT1 exactly; Soft Thru ON behaviour may already apply here — owner should confirm intended Soft Thru source"
        );
    } else {
        println!(
            "H-018 PATH NOTE: no exact DT1 echo on FCB1010 path in the echo window; Soft Thru ON case still blocked until owner names a Soft Thru source"
        );
    }
    Ok(())
}
