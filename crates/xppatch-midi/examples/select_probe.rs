//! H-070 (MIDI-tool half): Bank Select + Program Change selects the temporary sound; System selection fields move.
//! Restores Nice Piano (PR-A 004) at the end. Requires Receive Program Change and Bank Select ON.
//! Usage: select_probe <input> <output>
use std::thread;
use std::time::Duration;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn system_sel(client: &mut Client<MidirTransport>) -> Res<(i32, i32, i32)> {
    let doc = client.read_system(&CancelToken::new())?;
    let c = doc.block(0).ok_or("missing System Common")?;
    Ok((
        c.get("patch_group_type")?,
        c.get("patch_group_id")?,
        c.get("patch_number")?,
    ))
}

fn temp_name(client: &mut Client<MidirTransport>) -> Res<String> {
    Ok(client
        .fetch_temp_patch(&CancelToken::new(), &mut |_| {})?
        .name()
        .trim_end()
        .to_string())
}

fn select(
    client: &mut Client<MidirTransport>,
    channel: u8,
    msb: u8,
    lsb: u8,
    program: u8,
    label: &str,
) -> Res<()> {
    client.select_sound(channel, msb, lsb, program)?;
    thread::sleep(Duration::from_millis(400));
    let name = temp_name(client)?;
    let (ty, id, num) = system_sel(client)?;
    println!(
        "select {label}: CC0={msb} CC32={lsb} PC={program} -> temp '{name}', system type={ty} id={id} number={num}"
    );
    Ok(())
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
    let sys = client.read_system(&cancel)?;
    let common = sys.block(0).ok_or("missing System Common")?;
    let recv_pc = common.get("receive_program_change")?;
    let recv_bs = common.get("receive_bank_select")?;
    let ch_raw = common.get("patch_receive_channel")?;
    if recv_pc != 1 || recv_bs != 1 {
        return Err(
            format!("need Receive PC and Bank Select ON (got PC={recv_pc} BS={recv_bs})").into(),
        );
    }
    if !(0..=15).contains(&ch_raw) {
        return Err(format!("patch_receive_channel out of range: {ch_raw}").into());
    }
    let ch = u8::try_from(ch_raw)?;
    println!(
        "Patch receive channel {ch}; starting temp '{}'",
        temp_name(&mut client)?
    );

    // USER 1, PR-A 1, PR-B 10, PR-C 128, GM 1 (manual p.181); channel is 0-based.
    select(&mut client, ch, 80, 0, 0, "USER 1")?;
    let user1 = temp_name(&mut client)?;
    select(&mut client, ch, 81, 0, 0, "PR-A 1")?;
    let pra1 = temp_name(&mut client)?;
    select(&mut client, ch, 81, 1, 9, "PR-B 10")?;
    let prb10 = temp_name(&mut client)?;
    select(&mut client, ch, 81, 2, 127, "PR-C 128")?;
    let prc128 = temp_name(&mut client)?;
    select(&mut client, ch, 81, 3, 0, "GM 1")?;
    let gm1 = temp_name(&mut client)?;

    // Restore the session's known temporary (PR-A 004 Nice Piano).
    select(&mut client, ch, 81, 0, 3, "PR-A 004 Nice Piano")?;
    let restored = temp_name(&mut client)?;
    if restored != "Nice Piano" {
        return Err(format!("restore expected Nice Piano, got '{restored}'").into());
    }

    if user1 == pra1 && pra1 == prb10 {
        return Err("selections did not change the temporary name; Receive may be ignored".into());
    }
    println!(
        "H-070 MIDI-TOOL PASS: USER1='{user1}' PR-A1='{pra1}' PR-B10='{prb10}' PR-C128='{prc128}' GM1='{gm1}'; temp restored to Nice Piano"
    );
    println!("Panel LCD confirmation of each name remains owner.");
    Ok(())
}
