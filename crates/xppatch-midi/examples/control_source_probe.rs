//! H-059 half: System Control Source / Pedal Assign accept the hypothesized raw values over SysEx and read back.
//! Does not claim panel LCD labels (CC07, PITCH BEND, …) — those remain owner.
//! Usage: control_source_probe <input> <output>
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
        ("system_control_source_1", 7i32, "hypothesis CC07"),
        ("system_control_source_1", 96, "hypothesis PITCH BEND"),
        ("system_control_source_1", 97, "hypothesis AFTERTOUCH"),
        (
            "system_control_source_1",
            6,
            "unlisted raw (manual has no name)",
        ),
        ("pedal1_assign", 98, "hypothesis PROG-UP"),
        (
            "pedal1_assign",
            104,
            "hypothesis OCT-DOWN (top of pedal range)",
        ),
        ("c1_assign", 40, "unlisted raw in C1 range"),
    ];

    for (id, value, note) in cases {
        let mut doc = client.read_system(&cancel)?;
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
        println!("PASS {id}={value} ({note})");
    }

    let plan_r = client.plan_system_write(&cancel)?;
    let report = client.commit_system_write(&plan_r, &baseline, &cancel)?;
    if !report.verified || client.read_system(&cancel)? != baseline {
        return Err("cleanup restore of System settings failed".into());
    }
    println!("System settings restored to pre-test bytes");
    println!(
        "H-059 PARTIAL PASS: raw control-source/assign values write and read back equal; owner must confirm panel labels before fancy names are shown"
    );
    Ok(())
}
