//! Read-only: write the instrument's temporary patch (590 bytes of blocks) to a file, to compare before and after a test.
//!
//! cargo run -p xppatch-midi --example dump_temp -- <input name> <output name> <file>

use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, file] = a.as_slice() else {
        return Err("pass an input name, an output name and a file".into());
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let p = client.fetch_temp_patch(&CancelToken::new(), &mut |_| {})?;
    std::fs::write(file, p.to_blocks())?;
    println!("temp patch '{}' written to {file}", p.name().trim_end());
    Ok(())
}
