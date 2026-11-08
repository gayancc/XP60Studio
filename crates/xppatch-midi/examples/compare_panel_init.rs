//! Read-only comparison of the current temporary Patch to the captured XP-60 panel DEFAULT Init.
//! Usage: compare_panel_init <input> <output> <captured.blocks>
use xppatch_core::Patch;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, file] = args.as_slice() else {
        return Err("pass input, output, and new captured blocks path".into());
    };
    if std::path::Path::new(file).exists() {
        return Err("captured file already exists; choose a new path".into());
    }
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let actual = client.fetch_temp_patch(&CancelToken::new(), &mut |_| {})?;
    let expected = Patch::xp60_default();
    let actual_bytes = actual.to_blocks();
    let expected_bytes = expected.to_blocks();
    std::fs::write(file, &actual_bytes)?;
    let diffs: Vec<_> = expected_bytes
        .iter()
        .zip(&actual_bytes)
        .enumerate()
        .filter_map(|(i, (a, b))| (a != b).then_some((i, *a, *b)))
        .collect();
    println!("XP-60 panel Init name: '{}'", actual.name().trim_end());
    println!("XP-60 Init action name: '{}'", expected.name().trim_end());
    println!("Compared {} bytes; differing bytes: {}", actual_bytes.len(), diffs.len());
    for (offset, app, panel) in diffs.iter().take(40) {
        println!("  byte {offset}: XP-60 Init action {app} vs panel {panel}");
    }
    if diffs.len() > 40 {
        println!("  ... {} more differences", diffs.len() - 40);
    }
    println!("Captured actual XP-60 bytes to {file}");
    if diffs.is_empty() {
        println!("H-052 PANEL INIT MATCHES XP-60 INIT ACTION");
    } else {
        println!("H-052 PANEL INIT DIFFERS FROM XP-60 INIT ACTION");
    }
    Ok(())
}
