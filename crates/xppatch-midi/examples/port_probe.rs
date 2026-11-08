//! Hardware session, H-019 (absent or wrong port): a bank read with nothing answering must give up after 3 silent slots, and ambiguous or empty port names
//! must be refused with a message that lists the candidates. Nothing is sent to the instrument: the output used for the silent case is the PC's own software synth.
//!
//! cargo run -p xppatch-midi --example port_probe -- <real input name> <real output name> <other output name>

use std::time::Instant;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, other] = a.as_slice() else {
        return Err("pass a real input name, a real output name and another output name".into());
    };
    println!("== silent output ==");
    let mut client = Client::new(
        MidirTransport::connect(input, other)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let t = Instant::now();
    let bank = client.fetch_bank(0..128u8, &cancel, &mut |_| {})?;
    println!("bank read with input {input:?} and output {other:?} (nothing answers): {} patches, {} failures, aborted {}, after {:?}", bank.patches.len(), bank.failures.len(), bank.aborted, t.elapsed());
    drop(client);

    println!("== port names ==");
    for (i, o) in [
        ("", output.as_str()),
        (input.as_str(), ""),
        ("no such port", output.as_str()),
        (input.as_str(), "t"),
    ] {
        match MidirTransport::connect(i, o) {
            Ok(_) => println!("input {i:?}, output {o:?}: connected"),
            Err(e) => println!("input {i:?}, output {o:?}: refused: {e}"),
        }
    }
    Ok(())
}
