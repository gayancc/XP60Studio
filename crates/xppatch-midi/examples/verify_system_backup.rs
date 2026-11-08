//! Read-only: compare all System bytes on the XP-60 with a saved System .syx file.
//! On mismatch, print each differing System Common parameter and Scale Tune byte.
use xppatch_core::{DocKind, SyxImage};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, file] = args.as_slice() else {
        return Err("pass input, output and System backup.syx".into());
    };
    let expected = SyxImage::parse(&std::fs::read(file)?)?
        .system_document()?
        .ok_or("backup contains no System settings")?;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let actual = client.read_system(&CancelToken::new())?;
    if actual == expected {
        println!("SYSTEM VERIFICATION PASSED: every System byte matches backup");
        return Ok(());
    }

    println!("SYSTEM DIFFERS from {}", file);
    let exp_common = expected.block(0).ok_or("expected missing System Common")?;
    let act_common = actual.block(0).ok_or("actual missing System Common")?;
    for param in &exp_common.def().params {
        // Prefer named parameters; if a nibble decode fails, fall through to a raw byte listing
        // so later differences are still reported.
        match (exp_common.get(&param.id), act_common.get(&param.id)) {
            (Ok(left), Ok(right)) if left != right => {
                println!("  common.{}: backup={} live={}", param.id, left, right);
            }
            (Ok(_), Ok(_)) => {}
            (left, right) => {
                println!(
                    "  common.{}: could not decode as parameter (backup={left:?}, live={right:?})",
                    param.id
                );
            }
        }
    }
    if exp_common.bytes() != act_common.bytes() {
        for (offset, (a, b)) in exp_common
            .bytes()
            .iter()
            .zip(act_common.bytes())
            .enumerate()
        {
            if a != b {
                println!("  common raw offset {offset}: backup={a} live={b}");
            }
        }
    }
    for i in 1..=DocKind::System.items() {
        let exp = expected.block(i).ok_or("expected missing Scale Tune")?;
        let act = actual.block(i).ok_or("actual missing Scale Tune")?;
        if exp.bytes() == act.bytes() {
            continue;
        }
        for (offset, (a, b)) in exp.bytes().iter().zip(act.bytes()).enumerate() {
            if a != b {
                println!(
                    "  {}: offset {} backup={} live={}",
                    DocKind::System.item_label(i - 1),
                    offset,
                    a,
                    b
                );
            }
        }
    }
    Err("System settings differ from backup".into())
}
