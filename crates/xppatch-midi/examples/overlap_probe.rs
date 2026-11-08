//! Read-only: does the XP-60 drop a second RQ1 that arrives while it is still streaming the replies to a first one (H-011)?
//!
//! cargo run -p xppatch-midi --example overlap_probe -- <input name> <output name>

use std::time::Duration;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::MidiTransport;
use xppatch_sysex::address::Address;
use xppatch_sysex::memory::{TEMP_PATCH, TEMP_PERFORMANCE};
use xppatch_sysex::message::{DeviceId, Message};

fn rq1(address: Address, size: u32) -> Vec<u8> {
    Message::Rq1 {
        address,
        size: Address::from_compact(size).expect("size"),
    }
    .encode(DeviceId::DEFAULT)
    .expect("encode")
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output] = args.as_slice() else {
        return Err("pass an input and an output name".into());
    };
    let mut port = MidirTransport::connect(input, output)?;
    for delay_ms in [0u64, 100, 300] {
        while port.recv(Duration::from_millis(50))?.is_some() {}
        let start = port.now();
        port.send(&rq1(TEMP_PERFORMANCE, 3993))?; // streams 17 frames over about 650 ms
        port.sleep(Duration::from_millis(delay_ms));
        port.send(&rq1(TEMP_PATCH, 12))?; // the patch name
        let (mut frames, mut name_replies) = (0, 0);
        let mut buf: Vec<u8> = Vec::new();
        let mut last = port.now();
        while port.now() - last < Duration::from_millis(800)
            && port.now() - start < Duration::from_secs(6)
        {
            if let Some(chunk) = port.recv(Duration::from_millis(20))? {
                last = port.now();
                for b in chunk.into_iter().filter(|b| *b < 0xF8) {
                    if b == 0xF0 {
                        buf.clear();
                    }
                    buf.push(b);
                    if b == 0xF7 {
                        frames += 1;
                        if buf.get(5..9) == Some(&[0x03, 0x00, 0x00, 0x00]) {
                            name_replies += 1;
                        }
                    }
                }
            }
        }
        println!("second request {delay_ms} ms after the first: {frames} frames in all, {name_replies} reply to the second request");
    }
    Ok(())
}
