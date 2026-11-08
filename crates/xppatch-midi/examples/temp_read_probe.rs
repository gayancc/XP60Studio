//! Controlled temporary-name probe: does reading User memory or System settings discard edited temporary data?
//! Saves and restores the complete starting temporary patch. Never writes User or System memory.
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, kind, folder] = args.as_slice() else {
        return Err("pass input, output, slot|bank|system, evidence folder".into());
    };
    if !matches!(kind.as_str(), "slot" | "bank" | "system") {
        return Err("unknown read kind".into());
    }
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
    edited.set_name(if original.name().trim_end() == "READ PROBE" {
        "READ PROBE 2"
    } else {
        "READ PROBE"
    })?;
    let test = (|| -> Result<(), Box<dyn std::error::Error>> {
        client.send_temp_patch(&edited, true, &cancel)?;
        println!(
            "Changed temporary name '{}' -> '{}'; complete read-back verified",
            original.name().trim_end(),
            edited.name().trim_end()
        );
        match kind.as_str() {
            "slot" => {
                let p = client.read_user_slot(0, &cancel)?;
                println!("Read User slot 1: '{}'", p.name().trim_end());
            }
            "bank" => {
                let b = client.fetch_bank(0..128, &cancel, &mut |_| {})?;
                println!(
                    "Read bank: {} patches, {} failures",
                    b.patches.len(),
                    b.failures.len()
                );
                if b.patches.len() != 128 || !b.failures.is_empty() {
                    return Err("bank incomplete".into());
                }
            }
            "system" => {
                client.read_system(&cancel)?;
                println!("Read System settings");
            }
            _ => unreachable!(),
        }
        let after = client.fetch_temp_patch(&cancel, &mut |_| {})?;
        std::fs::write(
            std::path::Path::new(folder).join("after-read.blocks"),
            after.to_blocks(),
        )?;
        println!(
            "Temporary name after {kind} read: '{}'; edited bytes retained: {}",
            after.name().trim_end(),
            after == edited
        );
        if after != edited {
            return Err("read discarded edited temporary data".into());
        }
        Ok(())
    })();
    // Restore before returning any diagnostic failure.
    client.send_temp_patch(&original, true, &cancel)?;
    if client.fetch_temp_patch(&cancel, &mut |_| {})? != original {
        return Err("original temporary patch could not be restored".into());
    }
    println!("Original temporary patch restored and independently read back equal");
    test?;
    println!("EDITED TEMP READ CHECK PASSED ({kind})");
    Ok(())
}
