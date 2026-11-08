//! Hardware session, H-017: listen (send nothing) for a number of seconds and print the MIDI events the keyboard announces, such as Bank Select and Program Change
//! when a patch is chosen on the panel.
//!
//! cargo run -p xppatch-midi --example listen -- <input name> <output name> <seconds>

use std::time::{Duration, Instant};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, secs] = a.as_slice() else {
        return Err("pass an input name, an output name and a number of seconds".into());
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let began = Instant::now();
    let mut seen = 0;
    println!("listening for {secs} s: choose patches on the keyboard's panel now");
    while began.elapsed() < Duration::from_secs(secs.parse()?) {
        client.poll(Duration::from_millis(100))?;
        for e in client.take_events() {
            seen += 1;
            println!("  +{:>5} ms  {e:?}", began.elapsed().as_millis());
        }
    }
    println!(
        "{seen} event(s) in {secs} s; frames ignored (not ours): {}",
        client.ignored_frames()
    );
    Ok(())
}
