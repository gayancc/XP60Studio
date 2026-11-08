//! H-050: measure whether a DT1 gap above the default 20 ms helps reliability on this XP-60.
//! Sends the same temporary patch at 20 ms and at 50 ms (Patch Base's "wait 30" is in that neighbourhood),
//! verifies read-back equality each time, restores the original temp, and prints timings.
//! Usage: gap_probe <input> <output>
use std::time::Duration;
use std::time::Instant;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output] = args.as_slice() else {
        return Err("pass input and output names".into());
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let original = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    println!("baseline temp '{}'", original.name().trim_end());

    for gap_ms in [20u64, 50] {
        let applied = client.set_min_dt1_gap(Duration::from_millis(gap_ms));
        let t0 = Instant::now();
        client.send_temp_patch(&original, true, &cancel)?;
        let send = t0.elapsed();
        let t1 = Instant::now();
        let back = client.fetch_temp_patch(&cancel, &mut |_| {})?;
        let read = t1.elapsed();
        let equal = back == original;
        println!(
            "gap {gap_ms} ms (applied {:?}): send {:?}, read {:?}, equal={equal}",
            applied, send, read
        );
        if !equal {
            let _ = client.set_min_dt1_gap(Duration::from_millis(20));
            client.send_temp_patch(&original, true, &cancel)?;
            return Err(format!("read-back differed at {gap_ms} ms gap").into());
        }
    }
    let _ = client.set_min_dt1_gap(Duration::from_millis(20));
    client.send_temp_patch(&original, true, &cancel)?;
    if client.fetch_temp_patch(&cancel, &mut |_| {})? != original {
        return Err("final restore of temporary patch failed".into());
    }
    println!(
        "H-050 HARDWARE PASS: 20 ms and 50 ms gaps both verified equal; no reliability gain observed for the larger gap on this instrument"
    );
    Ok(())
}
