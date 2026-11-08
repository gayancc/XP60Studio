//! Read-only full hardware snapshot, including temporary patch. Never sends DT1 writes.
use std::time::Instant;
use xppatch_core::{
    system_document_bytes, user_document_bytes, user_patch_bank_bytes, DocKind, SyxImage,
};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::message::DeviceId;
fn secs() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs()
}
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, folder] = a.as_slice() else {
        return Err("pass input, output, backup folder".into());
    };
    std::fs::create_dir_all(folder)?;
    let dev = DeviceId::DEFAULT;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let temp = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    let temp_path = std::path::Path::new(folder).join("temp-patch.blocks");
    if temp_path.exists() {
        return Err("snapshot already exists; use a new folder".into());
    }
    std::fs::write(&temp_path, temp.to_blocks())?;
    println!(
        "Temporary patch '{}' saved to {}",
        temp.name().trim_end(),
        temp_path.display()
    );
    println!("== backup ==");
    let t = Instant::now();
    let bank = client.fetch_bank(0..128u8, &cancel, &mut |_| {})?;
    if bank.patches.len() != 128 || !bank.failures.is_empty() {
        return Err(format!(
            "the backup is not whole: {} patches, {} failures. Nothing was written.",
            bank.patches.len(),
            bank.failures.len()
        )
        .into());
    }
    let perfs = client.fetch_user_documents(DocKind::Performance, &cancel, &mut |_| {})?;
    let rhythm = client.fetch_user_documents(DocKind::Rhythm, &cancel, &mut |_| {})?;
    let system = client.read_system(&cancel)?;
    let mut file = user_patch_bank_bytes(&bank.patches, dev, Some(128))?;
    file.extend(user_document_bytes(
        DocKind::Performance,
        &perfs,
        dev,
        Some(128),
    )?);
    file.extend(user_document_bytes(
        DocKind::Rhythm,
        &rhythm,
        dev,
        Some(128),
    )?);
    file.extend(system_document_bytes(&system, dev, Some(128))?);
    let full =
        std::path::Path::new(folder).join(format!("{}-hardware-session-full-backup.syx", secs()));
    std::fs::write(&full, &file)?;
    let back = SyxImage::parse(&std::fs::read(&full)?)?;
    println!(
        "saved {} ({} bytes, {} messages) in {:?}: {} patches, {} performances, {} rhythm sets, System settings; the file parses again with {} patches",
        full.display(),
        file.len(),
        back.records.len(),
        t.elapsed(),
        bank.patches.len(),
        perfs.len(),
        rhythm.len(),
        back.user_patches()?.len()
    );
    println!("SNAPSHOT PASSED");
    Ok(())
}
