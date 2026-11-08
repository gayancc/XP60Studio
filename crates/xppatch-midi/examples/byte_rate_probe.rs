//! H-023: chunk 32 + 3,125 B/s byte-rate throttle vs unthrottled chunk 128; both must verify equal.
//! Restores the original temporary patch and default pacing afterward.
//! Usage: byte_rate_probe <input> <output>
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

    // Unthrottled path (defaults): chunk 128, no byte-rate.
    client.set_max_dt1_data(128);
    client.set_byte_rate(None);
    let t0 = Instant::now();
    let r0 = client.send_temp_patch(&original, true, &cancel)?;
    let unthrottled = t0.elapsed();
    println!(
        "chunk 128, throttle off: {:?}, {} messages, {} bytes, verified={}",
        unthrottled, r0.messages, r0.bytes, r0.verified
    );
    if !r0.verified || client.fetch_temp_patch(&cancel, &mut |_| {})? != original {
        return Err("unthrottled send did not verify".into());
    }

    // Patch Base B-05 defaults when throttle is on: chunk 32, 3125 B/s.
    client.set_max_dt1_data(32);
    client.set_byte_rate(Some(3125));
    let t1 = Instant::now();
    let r1 = client.send_temp_patch(&original, true, &cancel)?;
    let throttled = t1.elapsed();
    println!(
        "chunk 32, 3125 B/s: {:?}, {} messages, {} bytes, verified={}",
        throttled, r1.messages, r1.bytes, r1.verified
    );
    if !r1.verified || client.fetch_temp_patch(&cancel, &mut |_| {})? != original {
        client.set_byte_rate(None);
        client.set_max_dt1_data(128);
        let _ = client.send_temp_patch(&original, true, &cancel);
        return Err("throttled send did not verify".into());
    }
    if r1.messages <= r0.messages {
        return Err(format!(
            "expected more DT1 messages at chunk 32 (got {} vs {})",
            r1.messages, r0.messages
        )
        .into());
    }
    if throttled <= unthrottled {
        println!(
            "note: throttled send was not slower ({throttled:?} vs {unthrottled:?}); still verified equal"
        );
    }

    client.set_byte_rate(None);
    client.set_max_dt1_data(128);
    client.send_temp_patch(&original, true, &cancel)?;
    if client.fetch_temp_patch(&cancel, &mut |_| {})? != original {
        return Err("final restore failed".into());
    }
    println!(
        "H-023 HARDWARE PASS: chunk 32 + 3125 B/s and chunk 128 unthrottled both verified equal; pacing defaults restored"
    );
    Ok(())
}
