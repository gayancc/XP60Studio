//! H-057 remainder: Back up all memory / session full backup names equal independent instrument reads
//! (Performance and Rhythm), without requiring owner panel observation.
//! Usage: memory_names_probe <input> <output> <full-backup.syx>
use xppatch_core::{DocKind, SyxImage};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::memory::{user_performance, user_rhythm};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn main() -> Res<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, file] = args.as_slice() else {
        return Err("pass input, output, full-backup.syx".into());
    };
    let image = SyxImage::parse(&std::fs::read(file)?)?;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();

    let saved_p = image.user_documents(DocKind::Performance)?;
    if saved_p.len() != 32 {
        return Err(format!("backup has {} performances, need 32", saved_p.len()).into());
    }
    let mut perf_mismatch = Vec::new();
    for (slot, saved) in &saved_p {
        let live = match client.fetch_document(
            DocKind::Performance,
            user_performance(*slot)?,
            &cancel,
            &mut |_| {},
        ) {
            Ok(d) => d,
            Err(e) => {
                println!("retry perf {}: {e}", u16::from(*slot) + 1);
                client.fetch_document(
                    DocKind::Performance,
                    user_performance(*slot)?,
                    &CancelToken::new(),
                    &mut |_| {},
                )?
            }
        };
        if live != *saved {
            perf_mismatch.push(u16::from(*slot) + 1);
            println!(
                "PERF {} differ: backup='{}' live='{}'",
                u16::from(*slot) + 1,
                saved.name().trim_end(),
                live.name().trim_end()
            );
        }
    }
    println!(
        "Performances: {} compared; mismatches {:?}",
        saved_p.len(),
        perf_mismatch
    );

    let saved_r = image.user_documents(DocKind::Rhythm)?;
    if saved_r.len() != 2 {
        return Err(format!("backup has {} rhythms, need 2", saved_r.len()).into());
    }
    let mut rhythm_mismatch = Vec::new();
    for (slot, saved) in &saved_r {
        let live = client.fetch_document(
            DocKind::Rhythm,
            user_rhythm(*slot)?,
            &CancelToken::new(),
            &mut |_| {},
        )?;
        if live != *saved {
            rhythm_mismatch.push(u16::from(*slot) + 1);
            println!(
                "RHYTHM {} differ: backup='{}' live='{}'",
                u16::from(*slot) + 1,
                saved.name().trim_end(),
                live.name().trim_end()
            );
        } else {
            println!(
                "RHYTHM {}: '{}' equal",
                u16::from(*slot) + 1,
                live.name().trim_end()
            );
        }
    }

    if !perf_mismatch.is_empty() || !rhythm_mismatch.is_empty() {
        return Err(format!(
            "memory names/contents mismatch: perf {perf_mismatch:?} rhythm {rhythm_mismatch:?}"
        )
        .into());
    }
    println!(
        "H-057 PASS: all 32 User Performances and 2 User Rhythm Sets equal the backup file byte-for-byte (names included)"
    );
    Ok(())
}
