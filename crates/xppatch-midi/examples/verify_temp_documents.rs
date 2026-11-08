//! Read-only independent comparison of temporary Performance/Rhythm documents with saved block files.
use xppatch_core::{DocKind, Document};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, folder] = args.as_slice() else {
        return Err("pass input, output, document backup folder".into());
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    for (kind, name) in [
        (DocKind::Performance, "performance"),
        (DocKind::Rhythm, "rhythm"),
    ] {
        let saved = Document::from_block_bytes(
            kind,
            &std::fs::read(std::path::Path::new(folder).join(format!("{name}-original.blocks")))?,
        )?;
        let current = client.fetch_temp_document(kind, &cancel, &mut |_| {})?;
        if current != saved {
            return Err(format!("temporary {name} differs from its pre-test backup").into());
        }
        println!(
            "PASS temporary {name}: all {} blocks equal pre-test backup",
            current.blocks().len()
        );
    }
    println!("TEMP DOCUMENT RESTORATION PASSED");
    Ok(())
}
