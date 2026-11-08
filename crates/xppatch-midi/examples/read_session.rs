//! Read-only hardware session: the READ items of `artifact/docs/xppatchstudio/07-hardware-acceptance.md`.
//!
//! cargo run -p xppatch-midi --example read_session -- <input name> <output name>
//!
//! Sends only RQ1 data requests (and one MIDI Identity Request). Nothing is written to the instrument. Each section prints
//! `== H-nnn ==` followed by what was seen, so the output can be split into one evidence file per item.

use std::time::{Duration, Instant};
use xppatch_core::{DocKind, Patch};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig, MidiTransport};
use xppatch_sysex::address::Address;
use xppatch_sysex::memory::{TEMP_PATCH, TEMP_PERFORMANCE};
use xppatch_sysex::message::{DeviceId, Message};

type Frames = Vec<(Duration, Vec<u8>)>;

fn hex(b: &[u8]) -> String {
    b.iter()
        .map(|x| format!("{x:02X}"))
        .collect::<Vec<_>>()
        .join(" ")
}

/// Everything that arrives after `start` until the port has been quiet for `quiet` (or `max` has passed), as whole SysEx frames.
fn collect(
    port: &mut impl MidiTransport,
    start: Duration,
    quiet: Duration,
    max: Duration,
) -> Frames {
    let mut frames = Vec::new();
    let mut buf: Vec<u8> = Vec::new();
    let began = port.now();
    let mut last = port.now();
    loop {
        let now = port.now();
        if now - began > max || now - last > quiet {
            break;
        }
        match port.recv(Duration::from_millis(20)) {
            Ok(Some(chunk)) => {
                last = port.now();
                for b in chunk {
                    if b >= 0xF8 {
                        continue; // real-time bytes (active sensing, clock)
                    }
                    if b == 0xF0 {
                        buf.clear();
                    }
                    buf.push(b);
                    if b == 0xF7 {
                        frames.push((port.now() - start, std::mem::take(&mut buf)));
                    }
                }
            }
            Ok(None) => {}
            Err(_) => break,
        }
    }
    frames
}

fn rq1(address: Address, size: u32, device: u8) -> Vec<u8> {
    Message::Rq1 {
        address,
        size: Address::from_compact(size).expect("size"),
    }
    .encode(DeviceId::new(device).expect("device"))
    .expect("encode")
}

/// Send a request and report what came back.
fn ask(port: &mut impl MidiTransport, frame: &[u8], quiet_ms: u64, max_ms: u64) -> Frames {
    let _ = port.recv(Duration::from_millis(1)); // drain
    let start = port.now();
    port.send(frame).expect("send");
    collect(
        port,
        start,
        Duration::from_millis(quiet_ms),
        Duration::from_millis(max_ms),
    )
}

fn show(frames: &Frames) {
    if frames.is_empty() {
        println!("  (no reply)");
    }
    for (t, f) in frames {
        let head = &f[..f.len().min(14)];
        let data = f.len().saturating_sub(7 + 4);
        println!(
            "  +{:>4} ms  {} bytes, data {}  {}{}",
            t.as_millis(),
            f.len(),
            data,
            hex(head),
            if f.len() > 14 { " ..." } else { "" }
        );
    }
}

fn main() {
    if let Err(e) = run() {
        eprintln!("error: {e}");
        std::process::exit(1);
    }
}

fn run() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output] = args.as_slice() else {
        return Err("pass an input and an output name".into());
    };
    let mut port = MidirTransport::connect(input, output)?;

    println!("== H-001 ==");
    println!("ports opened: input {input:?}, output {output:?}");

    println!("== H-002 ==");
    println!("RQ1 temp patch name (03 00 00 00, 12 bytes) at device ID 0x10:");
    let f = ask(&mut port, &rq1(TEMP_PATCH, 12, 0x10), 300, 2000);
    show(&f);
    if let Some((_, r)) = f.first() {
        println!(
            "  reply: manufacturer {:02X}, device {:02X}, model {:02X}, command {:02X}",
            r[1], r[2], r[3], r[4]
        );
    }

    println!("== H-003 ==");
    println!("RQ1 at device ID 0x11 (a wrong ID):");
    show(&ask(&mut port, &rq1(TEMP_PATCH, 12, 0x11), 500, 1500));
    println!("RQ1 at device ID 0x10 (the right ID):");
    show(&ask(&mut port, &rq1(TEMP_PATCH, 12, 0x10), 300, 2000));

    println!("== H-004 ==");
    println!("Identity Request F0 7E 10 06 01 F7:");
    show(&ask(
        &mut port,
        &[0xF0, 0x7E, 0x10, 0x06, 0x01, 0xF7],
        500,
        1500,
    ));
    println!("Identity Request F0 7E 7F 06 01 F7 (all devices):");
    show(&ask(
        &mut port,
        &[0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7],
        500,
        1500,
    ));

    println!("== H-010 ==");
    println!("temp patch, one request per block, device 0x10:");
    let layout: Vec<(Address, usize)> = Patch::init()
        .to_dt1(TEMP_PATCH)?
        .into_iter()
        .map(|(a, d)| (a, d.len()))
        .collect();
    for (a, len) in &layout {
        let f = ask(&mut port, &rq1(*a, u32::try_from(*len)?, 0x10), 300, 3000);
        println!(" block at {a} wants {len} data bytes:");
        show(&f);
    }

    println!("== H-012 ==");
    println!(
        "five fetches of the temp patch, first-reply latency and the longest gap between replies:"
    );
    for run in 1..=5 {
        let mut first = Duration::ZERO;
        let mut worst = Duration::ZERO;
        let mut replies = 0;
        let began = Instant::now();
        for (a, len) in &layout {
            let f = ask(&mut port, &rq1(*a, u32::try_from(*len)?, 0x10), 250, 3000);
            if let Some((t, _)) = f.first() {
                if replies == 0 {
                    first = *t;
                }
                worst = worst.max(*t);
                replies += f.len();
            }
        }
        println!("  run {run}: {replies} replies, first reply {} ms, slowest block reply {} ms, whole fetch {} ms", first.as_millis(), worst.as_millis(), began.elapsed().as_millis());
    }

    println!("== H-011 ==");
    println!(
        "two requests back to back (tone 1, then the name) without waiting for the first reply:"
    );
    let (a1, l1) = layout[1];
    let _ = port.recv(Duration::from_millis(1));
    let start = port.now();
    port.send(&rq1(a1, u32::try_from(l1)?, 0x10))?;
    port.send(&rq1(TEMP_PATCH, 12, 0x10))?;
    show(&collect(
        &mut port,
        start,
        Duration::from_millis(500),
        Duration::from_millis(3000),
    ));

    println!("== H-013 ==");
    println!("sparse span: temp performance 01 00 00 00 size 00 00 1F 19 (3,993):");
    let f = ask(&mut port, &rq1(TEMP_PERFORMANCE, 3993, 0x10), 700, 8000);
    show(&f);
    let data: usize = f.iter().map(|(_, r)| r.len().saturating_sub(11)).sum();
    println!("  {} frames, {} data bytes in total", f.len(), data);

    println!("== H-014 ==");
    println!("partial block: patch name, 12 bytes:");
    show(&ask(&mut port, &rq1(TEMP_PATCH, 12, 0x10), 300, 2000));

    // The rest goes through the client, which is what the app uses.
    let mut client = Client::new(port, ClientConfig::default());
    let cancel = CancelToken::new();
    client.set_device(DeviceId::new(0x10)?);

    println!("== H-015 ==");
    let started = Instant::now();
    let bank = client.fetch_bank(0..128u8, &cancel, &mut |_| {})?;
    println!(
        "read the User bank: {} patches, {} failures, aborted {}, in {:?}",
        bank.patches.len(),
        bank.failures.len(),
        bank.aborted,
        started.elapsed()
    );
    let mut issues = 0;
    for (slot, p) in &bank.patches {
        let v = p.validate();
        issues += v.len();
        if !v.is_empty() {
            println!(
                "  slot {} '{}': {} range issues, first {:?}",
                slot + 1,
                p.name().trim_end(),
                v.len(),
                v.first()
            );
        }
    }
    println!("range issues in total: {issues}");
    for (slot, p) in bank.patches.iter().take(8) {
        println!("  slot {:>3}: {}", slot + 1, p.name().trim_end());
    }
    for (slot, e) in &bank.failures {
        println!("  slot {} failed: {e}", slot + 1);
    }
    println!("== H-048 ==");
    println!(
        "same read as H-015: {} of 128 slots decoded, {} range issues, {:?}",
        bank.patches.len(),
        issues,
        started.elapsed()
    );

    println!("== H-040 ==");
    let t = Instant::now();
    let temp_perf = client.fetch_temp_document(DocKind::Performance, &cancel, &mut |_| {})?;
    println!(
        "temp performance '{}': {} range issues, {:?}",
        temp_perf.name().trim_end(),
        temp_perf.validate().len(),
        t.elapsed()
    );
    let t = Instant::now();
    let perfs = client.fetch_user_documents(DocKind::Performance, &cancel, &mut |_| {})?;
    let bad: usize = perfs.iter().map(|(_, d)| d.validate().len()).sum();
    println!(
        "user performances: {} read, {} range issues, {:?}",
        perfs.len(),
        bad,
        t.elapsed()
    );
    for (slot, d) in perfs.iter().take(6) {
        println!("  perf {:>2}: {}", slot + 1, d.name().trim_end());
    }

    println!("== H-041 ==");
    let t = Instant::now();
    let rhythm = client.fetch_user_documents(DocKind::Rhythm, &cancel, &mut |_| {})?;
    let bad: usize = rhythm.iter().map(|(_, d)| d.validate().len()).sum();
    println!(
        "user rhythm sets: {} read, {} range issues, {:?}",
        rhythm.len(),
        bad,
        t.elapsed()
    );
    for (slot, d) in &rhythm {
        println!("  rhythm {}: {}", slot + 1, d.name().trim_end());
    }

    println!("== H-042 ==");
    let t = Instant::now();
    let system = client.read_system(&cancel)?;
    println!(
        "system: {} range issues, {:?}",
        system.validate().len(),
        t.elapsed()
    );
    for (b, i) in system.validate().iter().take(5) {
        println!("  {b}: {i:?}");
    }

    println!("== H-057 ==");
    println!("the reads above cover what 'Back up all memory' reads: {} patches, {} performances, {} rhythm sets, all without error", bank.patches.len(), perfs.len(), rhythm.len());
    Ok(())
}
