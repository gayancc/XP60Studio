//! H-046 (instrument side of the address map): each Rhythm Note block's third address byte is its MIDI note.
//! Read-only. Fetches temporary or User Rhythm and checks DT1 addresses for notes 35..98.
//! Usage: rhythm_address_probe <input> <output> [user-slot-one-based]
//! Without a slot, reads the temporary Rhythm Setup (Part 10 area).
use xppatch_core::DocKind;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::address::Address;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let (input, output, slot) = match args.as_slice() {
        [input, output] => (input.as_str(), output.as_str(), None),
        [input, output, slot] => (input.as_str(), output.as_str(), Some(slot.parse::<u8>()?)),
        _ => return Err("pass input, output, optionally User Rhythm slot 1 or 2".into()),
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let (base, doc) = if let Some(number) = slot {
        if !(1..=2).contains(&number) {
            return Err("User Rhythm slot must be 1 or 2".into());
        }
        let docs = client.fetch_user_documents(DocKind::Rhythm, &cancel, &mut |_| {})?;
        let doc = docs
            .into_iter()
            .find(|(s, _)| *s + 1 == number)
            .map(|(_, d)| d)
            .ok_or("User Rhythm slot missing")?;
        // User Rhythm Set bases: 10 40 00 00 and 10 41 00 00.
        (
            Address::from_lanes([0x10, 0x40 + (number - 1), 0x00, 0x00]),
            doc,
        )
    } else {
        (
            DocKind::Rhythm.temp_base(),
            client.fetch_temp_document(DocKind::Rhythm, &cancel, &mut |_| {})?,
        )
    };

    let mut mismatches = Vec::new();
    let frames = doc.to_dt1(base)?;
    let note_frames: Vec<_> = frames
        .iter()
        .filter(|(a, _)| (0x23..=0x62).contains(&a.bytes()[2]))
        .collect();
    if note_frames.len() != DocKind::Rhythm.items() {
        mismatches.push(format!(
            "expected {} note DT1 frames, got {}",
            DocKind::Rhythm.items(),
            note_frames.len()
        ));
    }
    for (i, (address, data)) in note_frames.iter().enumerate() {
        let want = 0x23u8 + u8::try_from(i)?;
        if address.bytes()[2] != want {
            mismatches.push(format!(
                "note frame {i}: third lane {} != MIDI note {want}",
                address.bytes()[2]
            ));
        }
        if data.len() != 58 {
            mismatches.push(format!(
                "note {want}: expected 58 data bytes, got {}",
                data.len()
            ));
        }
        let label = DocKind::Rhythm.item_label(i);
        if label != format!("note{want}") {
            mismatches.push(format!("label {label} != note{want}"));
        }
    }

    if !mismatches.is_empty() {
        for m in &mismatches {
            println!("MISMATCH: {m}");
        }
        return Err(format!("{} rhythm address mismatch(es)", mismatches.len()).into());
    }
    println!(
        "H-046 ADDRESS PASS: {} note blocks at MIDI notes 35..98 (base {base}, name '{}')",
        DocKind::Rhythm.items(),
        doc.name().trim_end()
    );
    println!("Panel key-name comparison remains owner observation (ledger H-046 panel half).");
    Ok(())
}
