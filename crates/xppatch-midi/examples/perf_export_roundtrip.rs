//! H-066: a User Performance (exported shape = what File > Export writes) can be received into a spare User slot.
//! Copies User Performance 1 into User Performance 32, verifies, restores 32 from its pre-write backup.
//! Also copies User Rhythm 1 onto User Rhythm 2 and restores.
//! Usage: perf_export_roundtrip <input> <output>
use std::thread;
use std::time::Duration;
use xppatch_core::{DocKind, Document};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig, ClientError};
use xppatch_sysex::memory::{user_performance, user_rhythm};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn read_one(client: &mut Client<MidirTransport>, kind: DocKind, slot: u8) -> Res<Document> {
    let base = match kind {
        DocKind::Performance => user_performance(slot)?,
        DocKind::Rhythm => user_rhythm(slot)?,
        DocKind::System => return Err("System is not a slotted User document".into()),
    };
    let cancel = CancelToken::new();
    match client.fetch_document(kind, base, &cancel, &mut |_| {}) {
        Ok(d) => Ok(d),
        Err(e) => {
            println!("retry {kind:?} slot {}: {e}", u16::from(slot) + 1);
            thread::sleep(Duration::from_millis(300));
            Ok(client.fetch_document(kind, base, &CancelToken::new(), &mut |_| {})?)
        }
    }
}

fn write_checked(
    client: &mut Client<MidirTransport>,
    kind: DocKind,
    slot: u8,
    doc: &Document,
) -> Res<()> {
    let cancel = CancelToken::new();
    match client.write_user_document(kind, slot, doc, &cancel) {
        Ok(report) if report.verified => Ok(()),
        Ok(_) => Err("write reported unverified".into()),
        Err(ClientError::Mismatch { .. }) | Err(ClientError::Timeout { .. }) => {
            println!("retry write {kind:?} slot {}", u16::from(slot) + 1);
            thread::sleep(Duration::from_millis(300));
            let report = client.write_user_document(kind, slot, doc, &CancelToken::new())?;
            if !report.verified {
                return Err("write retry unverified".into());
            }
            Ok(())
        }
        Err(e) => Err(e.into()),
    }
}

fn main() -> Res<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output] = args.as_slice() else {
        return Err("pass input and output names".into());
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );

    let source = read_one(&mut client, DocKind::Performance, 0)?;
    let spare_slot = 31u8;
    let spare = read_one(&mut client, DocKind::Performance, spare_slot)?;
    if source == spare {
        println!(
            "note: User Performance 1 and 32 are identical ('{}'); write still proves the wire path",
            source.name().trim_end()
        );
    }
    println!(
        "Writing Performance 1 '{}' into User Performance 32 (was '{}')",
        source.name().trim_end(),
        spare.name().trim_end()
    );
    write_checked(&mut client, DocKind::Performance, spare_slot, &source)?;
    let back = read_one(&mut client, DocKind::Performance, spare_slot)?;
    if back != source {
        let _ = write_checked(&mut client, DocKind::Performance, spare_slot, &spare);
        return Err("independent Performance read-back differed from source".into());
    }
    println!(
        "H-066 PERFORMANCE PASS: User Performance 32 equals '{}'",
        back.name().trim_end()
    );

    write_checked(&mut client, DocKind::Performance, spare_slot, &spare)?;
    if read_one(&mut client, DocKind::Performance, spare_slot)? != spare {
        return Err("Performance 32 restore failed".into());
    }
    println!(
        "H-066 CLEANUP: User Performance 32 restored to '{}'",
        spare.name().trim_end()
    );

    let r0 = read_one(&mut client, DocKind::Rhythm, 0)?;
    let r1 = read_one(&mut client, DocKind::Rhythm, 1)?;
    println!(
        "Writing Rhythm 1 '{}' into User Rhythm 2 (was '{}')",
        r0.name().trim_end(),
        r1.name().trim_end()
    );
    write_checked(&mut client, DocKind::Rhythm, 1, &r0)?;
    let back = read_one(&mut client, DocKind::Rhythm, 1)?;
    if back != r0 {
        let _ = write_checked(&mut client, DocKind::Rhythm, 1, &r1);
        return Err("independent Rhythm read-back differed from source".into());
    }
    println!(
        "H-066 RHYTHM PASS: User Rhythm 2 equals '{}'",
        back.name().trim_end()
    );
    write_checked(&mut client, DocKind::Rhythm, 1, &r1)?;
    if read_one(&mut client, DocKind::Rhythm, 1)? != r1 {
        return Err("Rhythm 2 restore failed".into());
    }
    println!(
        "H-066 CLEANUP: User Rhythm 2 restored to '{}'",
        r1.name().trim_end()
    );
    println!("H-066 HARDWARE PASS (User Performance + Rhythm Set export-shaped DT1 receive)");
    Ok(())
}
