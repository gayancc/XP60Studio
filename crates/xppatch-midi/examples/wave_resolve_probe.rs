//! H-058 half: read wave_group_type / wave_group_id / wave_number from the temporary patch tones and
//! classify them with the same INT-A/INT-B / EXP board rules as ui/src/model/factory.ts.
//! The SysEx payload carries numeric wave IDs, not the LCD's waveform text.
//! Usage: wave_resolve_probe <input> <output>
use xppatch_core::PatchBlock;
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
    let patch = client.fetch_temp_patch(&CancelToken::new(), &mut |_| {})?;
    println!("temp '{}'", patch.name().trim_end());
    for tone in 0..4u8 {
        let b = patch.block(PatchBlock::Tone(tone)).ok_or("missing tone")?;
        let ty = b.get("wave_group_type")?;
        let id = b.get("wave_group_id")?;
        let raw = b.get("wave_number")?;
        let num = raw + 1; // SysEx stores a zero-based index; the XP-60 LCD numbers waves from 001.
        let label = match (ty, id) {
            (0, 1) => format!("INT-A wave {num}"),
            (0, 2) => format!("INT-B wave {num}"),
            (2, n) => format!("EXP board {n} wave {num}"),
            _ => format!("unmapped type={ty} id={id} wave={num}"),
        };
        println!("tone{}: type={ty} id={id} raw={raw} display={num:03} -> {label}", tone + 1);
    }
    println!(
        "H-058: live SysEx wave IDs classified (INT-A id=1, INT-B id=2, EXP type=2); LCD waveform text requires separate panel observation."
    );
    Ok(())
}
