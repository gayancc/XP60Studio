//! Restore System settings from a full or System-only .syx after saving the live settings.
//! Usage: restore_system_file <input> <output> <backup.syx> <before-restore.syx>
use xppatch_core::{system_document_bytes, SyxImage};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::message::DeviceId;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, backup_file, before_file] = args.as_slice() else {
        return Err("pass input, output, backup .syx, new before-restore .syx path".into());
    };
    if std::path::Path::new(before_file).exists() {
        return Err("before-restore file already exists; choose a new path".into());
    }
    let target = SyxImage::parse(&std::fs::read(backup_file)?)?
        .system_document()?
        .ok_or("backup has no System document")?;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let plan = client.plan_system_write(&cancel)?;
    let before = plan.backup();
    std::fs::write(
        before_file,
        system_document_bytes(before, DeviceId::DEFAULT, Some(128))?,
    )?;
    if before == &target {
        println!("SYSTEM RESTORE PASS: live settings already match backup; saved {before_file}");
        return Ok(());
    }
    let report = client.commit_system_write(&plan, &target, &cancel)?;
    if !report.verified || client.read_system(&cancel)? != target {
        return Err("System restore did not read back equal to backup".into());
    }
    println!("SYSTEM RESTORE PASS: all settings match backup; prior settings saved to {before_file}");
    Ok(())
}
