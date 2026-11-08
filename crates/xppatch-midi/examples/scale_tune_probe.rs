//! H-042 Scale Tune half: read the Scale Tune region alone (not only as part of a full System fetch).
//! Read-only. Requests each Scale Tune block separately; size comes from `data/xp60-parameters.json` (12 bytes).
//! Usage: scale_tune_probe <input> <output>
use xppatch_core::DocKind;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig, Expect};
use xppatch_sysex::address::Address;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output] = args.as_slice() else {
        return Err("pass input and output".into());
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let full = client.read_system(&cancel)?;
    let tune_len = full
        .block(1)
        .ok_or("full System missing first Scale Tune block")?
        .bytes()
        .len();
    if tune_len != 12 {
        return Err(format!(
            "unexpected Scale Tune block length {tune_len}; parameter map says 12"
        )
        .into());
    }
    let size = Address::from_compact(u32::try_from(tune_len)?).expect("size fits");
    let mut separate = Vec::new();
    for i in 0..DocKind::System.items() {
        let addr = Address::from_lanes([0x00, 0x00, 0x10 + u8::try_from(i)?, 0x00]);
        let frames = client.fetch_span(
            addr,
            size,
            &[Expect {
                address: addr,
                len: tune_len,
            }],
            &cancel,
        )?;
        if frames.len() != 1 || frames[0].1.len() != tune_len {
            return Err(format!(
                "Scale Tune block {} returned {frames:?}, want one {tune_len}-byte DT1",
                i + 1
            )
            .into());
        }
        separate.push(frames.into_iter().next().unwrap().1);
    }
    for (i, from_separate) in separate.iter().enumerate() {
        let from_full = full
            .block(i + 1)
            .ok_or("full System missing Scale Tune block")?
            .bytes();
        if from_full != from_separate.as_slice() {
            return Err(format!(
                "Scale Tune block {} differs between full System fetch and separate RQ1",
                i + 1
            )
            .into());
        }
    }
    println!(
        "H-042 SCALE TUNE PASS: {} separate RQ1 reads ({tune_len} bytes each at 00 00 10 00..); equal full System Scale Tune blocks",
        DocKind::System.items()
    );
    println!("Panel value comparison remains owner observation.");
    Ok(())
}
