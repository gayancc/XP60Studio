//! Read-only: print byte differences between a saved System .syx and the XP-60's current System data.
use xppatch_core::SyxImage;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, file] = args.as_slice() else {
        return Err("pass input, output and System backup.syx".into());
    };
    let old = SyxImage::parse(&std::fs::read(file)?)?
        .system_document()?
        .ok_or("no System data")?;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let now = client.read_system(&CancelToken::new())?;
    let mut count = 0;
    for (block, (a, b)) in old.blocks().iter().zip(now.blocks()).enumerate() {
        for (byte, (x, y)) in a.bytes().iter().zip(b.bytes()).enumerate() {
            if x != y {
                println!("block {block} byte {byte}: {x} -> {y}");
                count += 1
            }
        }
    }
    println!("SYSTEM DIFFERENCES: {count}");
    Ok(())
}
