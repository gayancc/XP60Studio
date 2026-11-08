//! B16 / System arpeggio: write a few Arpeggio parameters via SysEx, read back equal, restore System.
//! Does not claim audible arpeggio behaviour (owner).
//! Usage: arpeggio_probe <input> <output>
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn main() -> Res<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output] = args.as_slice() else {
        return Err("pass input and output names".into());
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let plan = client.plan_system_write(&cancel)?;
    let baseline = plan.backup().clone();

    let cases = [
        ("arpeggio_style", 5i32),
        ("arpeggio_motif", 2),
        ("arpeggio_beat_pattern", 10),
        ("arpeggio_accent_rate", 40),
        ("arpeggio_octave_range", 4), // display -3..+3 mid-ish
        ("arpeggio_part_number", 9),  // PART10
    ];

    for (id, value) in cases {
        let mut doc = client.read_system(&cancel)?;
        let cur = doc.block(0).ok_or("missing System Common")?.get(id)?;
        let value = if cur == value {
            if value + 1 <= 32 {
                value + 1
            } else {
                value.saturating_sub(1)
            }
        } else {
            value
        };
        doc.block_mut(0)
            .ok_or("missing System Common")?
            .set(id, value)?;
        let plan_w = client.plan_system_write(&cancel)?;
        let report = client.commit_system_write(&plan_w, &doc, &cancel)?;
        if !report.verified {
            let _ = client
                .plan_system_write(&cancel)
                .and_then(|p| client.commit_system_write(&p, &baseline, &CancelToken::new()));
            return Err(format!("write {id}={value} did not verify").into());
        }
        let back = client
            .read_system(&cancel)?
            .block(0)
            .ok_or("missing System Common")?
            .get(id)?;
        if back != value {
            let _ = client
                .plan_system_write(&cancel)
                .and_then(|p| client.commit_system_write(&p, &baseline, &CancelToken::new()));
            return Err(format!("read-back {id} expected {value}, got {back}").into());
        }
        println!("PASS {id}={value}");
    }

    let plan_r = client.plan_system_write(&cancel)?;
    let report = client.commit_system_write(&plan_r, &baseline, &cancel)?;
    if !report.verified || client.read_system(&cancel)? != baseline {
        return Err("cleanup restore of System settings failed".into());
    }
    println!("System settings restored");
    println!("B16 SysEx PASS: Arpeggio System parameters write/read-back equal; audible arpeggio remains owner");
    Ok(())
}
