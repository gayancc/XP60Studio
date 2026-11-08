//! Restore a saved temporary sound from blocks or a single-patch .syx. Never writes permanent memory.
use xppatch_core::{Patch, SyxImage};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, file, before_file] = args.as_slice() else {
        return Err("pass input, output, saved patch file, new before-restore blocks path".into());
    };
    let bytes = std::fs::read(file)?;
    let saved = if bytes.first() == Some(&0xf0) {
        let patches = SyxImage::parse(&bytes)?.user_patches()?;
        if patches.len() != 1 {
            return Err("restore file must contain exactly one patch".into());
        }
        patches[0].1.clone()
    } else {
        Patch::from_blocks(&bytes)?
    };
    if std::path::Path::new(before_file).exists() {
        return Err("before-restore backup already exists; choose a new file".into());
    }
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let before = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    std::fs::write(before_file, before.to_blocks())?;
    client.send_temp_patch(&saved, true, &cancel)?;
    if client.fetch_temp_patch(&cancel, &mut |_| {})? != saved {
        return Err("temporary restore read-back differs".into());
    }
    println!("TEMP RESTORE PASSED: '{}' -> '{}'; complete independent read-back equal; previous bytes saved to {before_file}",before.name().trim_end(),saved.name().trim_end());
    Ok(())
}
