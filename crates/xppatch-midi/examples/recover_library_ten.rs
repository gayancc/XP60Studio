//! Recover User slots 1-10 after a full-bank read timeout interrupted the Library export roundtrip.
//! Reads each target slot and writes its backed-up original only when it still matches the exported test patch.
//! Usage: recover_library_ten <input> <output> <prewrite-full.syx> <export-ten.syx>
use xppatch_core::SyxImage;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, backup_file, export_file] = args.as_slice() else {
        return Err("pass input, output, prewrite-full.syx, export-ten.syx".into());
    };
    let originals = SyxImage::parse(&std::fs::read(backup_file)?)?.user_patches()?;
    let exported = SyxImage::parse(&std::fs::read(export_file)?)?.user_patches()?;
    if originals.len() != 128 || exported.len() != 10 {
        return Err("backup must have 128 patches and export must have ten".into());
    }
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    for slot in 0..10u8 {
        let original = &originals
            .iter()
            .find(|(s, _)| *s == slot)
            .ok_or("backup slot missing")?
            .1;
        let test = &exported
            .iter()
            .find(|(s, _)| *s == slot)
            .ok_or("export slot missing")?
            .1;
        let plan = client.plan_user_write(slot, &cancel)?;
        if plan.backup() == original {
            println!("User slot {} already original", slot + 1);
            continue;
        }
        if plan.backup() != test {
            return Err(format!("User slot {} has unexpected content; stopped", slot + 1).into());
        }
        let report = client.commit_user_write(&plan, original, &cancel)?;
        if !report.verified {
            return Err(format!("User slot {} restore was not verified", slot + 1).into());
        }
        println!("Restored User slot {} from full-bank backup", slot + 1);
    }
    for slot in 0..10u8 {
        let original = &originals
            .iter()
            .find(|(s, _)| *s == slot)
            .ok_or("backup slot missing")?
            .1;
        if client.read_user_slot(slot, &cancel)? != *original {
            return Err(format!("User slot {} differs after restore", slot + 1).into());
        }
    }
    println!("TARGETED RESTORE VERIFIED: all ten slots equal the full-bank backup");
    Ok(())
}
