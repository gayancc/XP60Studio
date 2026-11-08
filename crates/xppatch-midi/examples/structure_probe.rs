//! G1 / H-075 SysEx half: Structure Type 1&2 and 3&4 accept writes and read back; restore temp.
//! Audible wiring confirmation remains owner.
//! Usage: structure_probe <input> <output>
use xppatch_core::{Patch, PatchBlock};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::memory::TEMP_PATCH;
use xppatch_sysex::message::{DeviceId, Message};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn frame_for(patch: &Patch, id: &str) -> Res<Vec<u8>> {
    let (address, data) = patch.param_dt1(TEMP_PATCH, PatchBlock::Common, id)?;
    Ok(Message::Dt1 { address, data }.encode(DeviceId::DEFAULT)?)
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
    let cancel = CancelToken::new();
    let original = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    println!("baseline temp '{}'", original.name().trim_end());

    let cases = [
        ("structure_type_1_2", 2i32), // type 3 (0-based)
        ("structure_type_1_2", 5),    // type 6
        ("structure_type_1_2", 7),    // type 8
        ("structure_type_3_4", 3),
        ("booster_1_2", 2), // +12; meaningful for types 3/4
    ];

    for (id, value) in cases {
        let mut edited = original.clone();
        let before = edited
            .block(PatchBlock::Common)
            .ok_or("missing Common")?
            .get(id)?;
        let value = if before == value {
            (value + 1) % 10
        } else {
            value
        };
        edited
            .block_mut(PatchBlock::Common)
            .ok_or("missing Common")?
            .set(id, value)?;
        client.send_temp_frame(&frame_for(&edited, id)?, &cancel)?;
        let back = client
            .fetch_temp_patch(&cancel, &mut |_| {})?
            .block(PatchBlock::Common)
            .ok_or("missing Common")?
            .get(id)?;
        if back != value {
            client.send_temp_patch(&original, true, &cancel)?;
            return Err(format!("read-back {id} expected {value}, got {back}").into());
        }
        println!("PASS {id}={value}");
    }

    client.send_temp_patch(&original, true, &cancel)?;
    if client.fetch_temp_patch(&cancel, &mut |_| {})? != original {
        return Err("final restore of temporary patch failed".into());
    }
    println!("temporary patch restored");
    println!(
        "G1/H-075 SysEx PARTIAL PASS: Structure Type and Booster write/read-back equal; audible wiring remains owner"
    );
    Ok(())
}
