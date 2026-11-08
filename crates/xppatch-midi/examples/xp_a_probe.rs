//! H-072: Bank Select XP-A (84/0) + Program Change 1 — observe whether an expansion board answers.
//! Restores PR-A 004 Nice Piano afterward. Does not invent board contents.
//! Usage: xp_a_probe <input> <output>
use std::thread;
use std::time::Duration;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn temp_name(client: &mut Client<MidirTransport>) -> Res<String> {
    Ok(client
        .fetch_temp_patch(&CancelToken::new(), &mut |_| {})?
        .name()
        .trim_end()
        .to_string())
}

fn sel(client: &mut Client<MidirTransport>) -> Res<(i32, i32, i32)> {
    let doc = client.read_system(&CancelToken::new())?;
    let c = doc.block(0).ok_or("missing System Common")?;
    Ok((
        c.get("patch_group_type")?,
        c.get("patch_group_id")?,
        c.get("patch_number")?,
    ))
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
    let ch = {
        let c = client.read_system(&cancel)?;
        let ch = c
            .block(0)
            .ok_or("missing System Common")?
            .get("patch_receive_channel")?;
        if !(0..=15).contains(&ch) {
            return Err(format!("bad patch_receive_channel {ch}").into());
        }
        u8::try_from(ch)?
    };
    let before_name = temp_name(&mut client)?;
    let before_sel = sel(&mut client)?;
    println!("before: temp='{before_name}' system={before_sel:?}");

    // XP-A bank select MSB/LSB from Owner's Manual; program 0 = patch 001.
    client.select_sound(ch, 84, 0, 0)?;
    thread::sleep(Duration::from_millis(500));
    let after_name = temp_name(&mut client)?;
    let after_sel = sel(&mut client)?;
    println!("after XP-A 001 (84/0 PC=0): temp='{after_name}' system={after_sel:?}");

    let changed = after_name != before_name || after_sel != before_sel;
    if changed {
        println!(
            "H-072 PARTIAL PASS: instrument accepted XP-A selection (temp/system changed). Owner: confirm board in EXP-A and panel name."
        );
    } else {
        println!(
            "H-072 REMAINING: XP-A selection left temp/system unchanged (board missing, Receive ignored, or no audible/panel change without owner). Recorded as no-op on this instrument."
        );
    }

    client.select_sound(ch, 81, 0, 3)?;
    thread::sleep(Duration::from_millis(400));
    let restored = temp_name(&mut client)?;
    if restored != "Nice Piano" {
        eprintln!("WARNING: expected Nice Piano after restore, got '{restored}'");
    } else {
        println!("Temporary restored to Nice Piano");
    }
    Ok(())
}
