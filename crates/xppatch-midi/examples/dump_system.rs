//! Read-only System export for before/after hardware checks.
//! Usage: cargo run -p xppatch-midi --example dump_system -- <input> <output> <file.syx>
use xppatch_core::system_document_bytes;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::DeviceId;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, file] = args.as_slice() else {
        return Err("pass input, output and file.syx".into());
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let system = client.read_system(&CancelToken::new())?;
    std::fs::write(
        file,
        system_document_bytes(&system, DeviceId::DEFAULT, Some(128))?,
    )?;
    println!("System read-only export written to {file}");
    Ok(())
}
