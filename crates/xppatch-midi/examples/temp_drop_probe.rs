//! Does closing/reopening the MIDI transport discard edited temporary data? Temporary name only; full original restored.
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, folder] = args.as_slice() else {
        return Err("pass input, output, evidence folder".into());
    };
    std::fs::create_dir_all(folder)?;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let original = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    std::fs::write(
        std::path::Path::new(folder).join("original.blocks"),
        original.to_blocks(),
    )?;
    let mut edited = original.clone();
    edited.set_name("CLOSE PROBE")?;
    client.send_temp_patch(&edited, true, &cancel)?;
    println!(
        "Temporary name changed '{}' -> '{}', verified before closing MIDI",
        original.name().trim_end(),
        edited.name().trim_end()
    );
    drop(client);
    std::thread::sleep(std::time::Duration::from_millis(500));
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let after = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    std::fs::write(
        std::path::Path::new(folder).join("after-reopen.blocks"),
        after.to_blocks(),
    )?;
    println!(
        "Temporary name after reopening MIDI: '{}'; edited bytes retained: {}",
        after.name().trim_end(),
        after == edited
    );
    client.send_temp_patch(&original, true, &cancel)?;
    if client.fetch_temp_patch(&cancel, &mut |_| {})? != original {
        return Err("original temporary patch restore failed".into());
    }
    println!("Original temporary patch restored and independently read back equal");
    if after != edited {
        return Err("closing/reopening MIDI discarded temporary edits".into());
    }
    println!("EDITED TEMP MIDI REOPEN CHECK PASSED");
    Ok(())
}
