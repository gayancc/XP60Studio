//! Read-only hardware probe: lists ports, finds the device ID and reads the temporary patch.
//!
//! cargo run -p xppatch-midi --example probe            # list ports
//! cargo run -p xppatch-midi --example probe -- <in> <out>   # probe + read temp patch
//!
//! Sends only RQ1 reads. Output is evidence for `artifact/docs/xppatchstudio/07-hardware-acceptance.md` (H-001 to H-012).

use xppatch_midi::hardware::{list_ports, MidirTransport};
use xppatch_midi::{CancelToken, Client, ClientConfig, MidiTransport};

fn main() {
    if let Err(e) = run() {
        eprintln!("error: {e}");
        std::process::exit(1);
    }
}

fn run() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let ports = list_ports()?;
    println!("inputs:");
    ports.inputs.iter().for_each(|p| println!("  {p}"));
    println!("outputs:");
    ports.outputs.iter().for_each(|p| println!("  {p}"));
    let [input, output] = args.as_slice() else {
        println!("\npass an input and an output name (substring) to probe");
        return Ok(());
    };

    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    match client.scan_device_ids(&cancel, &mut |_| {})? {
        Some(info) => println!(
            "device ID {} answered; temp patch: '{}'",
            info.device.display_number(),
            info.patch_name
        ),
        None => {
            println!("no device ID answered");
            return Ok(());
        }
    }
    let started = client.port().now();
    let patch = client.fetch_temp_patch(&cancel, &mut |p| {
        println!("  {} {}/{}", p.what, p.done, p.total);
    })?;
    println!(
        "read temp patch '{}' in {:?}",
        patch.name(),
        client.port().now() - started
    );
    Ok(())
}
