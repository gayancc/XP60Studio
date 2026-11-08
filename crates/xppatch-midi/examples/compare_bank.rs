//! Read-only: read all 128 User patches and compare them with a bank `.syx` file (for example a backup made before a test).
//!
//! cargo run -p xppatch-midi --example compare_bank -- <input name> <output name> <bank file>

use xppatch_core::SyxImage;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, file] = a.as_slice() else {
        return Err("pass an input name, an output name and a bank file".into());
    };
    let expected = SyxImage::parse(&std::fs::read(file)?)?.user_patches()?;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let bank = client.fetch_bank(0..128u8, &CancelToken::new(), &mut |_| {})?;
    let differ: Vec<u16> = bank
        .patches
        .iter()
        .filter(|(s, p)| expected.iter().find(|(e, _)| e == s).map(|(_, q)| q) != Some(p))
        .map(|(s, _)| u16::from(*s) + 1)
        .collect();
    println!(
        "file has {} patches, the instrument gave {}; slots that differ: {differ:?}",
        expected.len(),
        bank.patches.len()
    );
    Ok(())
}
