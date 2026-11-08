//! Read-only inventory of wave references in a saved XP-60 User bank.
//! Usage: cargo run -p xppatch-midi --example backup_wave_audit -- <full-backup.syx> [--details]
use std::collections::BTreeMap;
use xppatch_core::{PatchBlock, SyxImage};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let ([file] | [file, _]) = args.as_slice() else {
        return Err("pass full-backup.syx [--details]".into());
    };
    if args.len() == 2 && args[1] != "--details" {
        return Err("second argument must be --details".into());
    }
    let details = args.len() == 2;
    let image = SyxImage::parse(&std::fs::read(file)?)?;
    let patches = image.user_patches()?;
    if patches.len() != 128 {
        return Err(format!("expected 128 User patches, got {}", patches.len()).into());
    }
    let mut groups: BTreeMap<(i32, i32), (usize, i32, i32)> = BTreeMap::new();
    for (slot, patch) in &patches {
        for tone in 0..4u8 {
            let block = patch.block(PatchBlock::Tone(tone)).ok_or("missing tone")?;
            let key = (block.get("wave_group_type")?, block.get("wave_group_id")?);
            let wave = block.get("wave_number")? + 1;
            if details {
                println!(
                    "REF,{},{},{},{},{}",
                    u16::from(*slot) + 1,
                    tone + 1,
                    key.0,
                    key.1,
                    wave
                );
            }
            let entry = groups.entry(key).or_insert((0, wave, wave));
            entry.0 += 1;
            entry.1 = entry.1.min(wave);
            entry.2 = entry.2.max(wave);
        }
    }
    println!(
        "{} User patches, {} tone wave references",
        patches.len(),
        patches.len() * 4
    );
    for ((ty, id), (count, min, max)) in groups {
        println!("type={ty} id={id}: {count} references, display wave {min:03}..{max:03}");
    }
    Ok(())
}
