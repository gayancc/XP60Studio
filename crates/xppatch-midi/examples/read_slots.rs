//! Read-only: the names of some User patch slots (1 to 128).
//!
//! cargo run -p xppatch-midi --example read_slots -- <input name> <output name> <first slot> <last slot>

use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, first, last] = a.as_slice() else {
        return Err(
            "pass an input name, an output name, a first slot and a last slot (1 to 128)".into(),
        );
    };
    let (first, last): (u8, u8) = (first.parse()?, last.parse()?);
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    for slot in first..=last {
        let p = client.read_user_slot(slot - 1, &cancel)?;
        println!("slot {slot:>3}: '{}'", p.name().trim_end());
    }
    Ok(())
}
