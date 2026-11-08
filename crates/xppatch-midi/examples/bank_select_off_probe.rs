//! H-071 (SysEx stand-in for panel Receive Bank Select OFF): with Bank Select receive off, Program Change alone
//! changes the number inside the current group; a wrong MIDI channel selects nothing. Restores System settings after.
//! Usage: bank_select_off_probe <input> <output>
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

fn sel_fields(client: &mut Client<MidirTransport>) -> Res<(i32, i32)> {
    let doc = client.read_system(&CancelToken::new())?;
    let c = doc.block(0).ok_or("missing System Common")?;
    Ok((c.get("patch_group_id")?, c.get("patch_number")?))
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
    let plan = client.plan_system_write(&cancel)?;
    let ch = {
        let c = plan.backup().block(0).ok_or("missing System Common")?;
        let ch = c.get("patch_receive_channel")?;
        if !(0..=15).contains(&ch) {
            return Err(format!("patch_receive_channel out of range: {ch}").into());
        }
        u8::try_from(ch)?
    };

    let mut edited = plan.backup().clone();
    edited
        .block_mut(0)
        .ok_or("missing System Common")?
        .set("receive_bank_select", 0)?;
    let report = client.commit_system_write(&plan, &edited, &cancel)?;
    if !report.verified {
        return Err("turning Receive Bank Select OFF did not verify".into());
    }
    println!("Receive Bank Select set OFF (SysEx stand-in)");

    let result = (|| -> Res<()> {
        // Establish PR-A group.
        client.select_sound(ch, 81, 0, 0)?;
        thread::sleep(Duration::from_millis(400));
        let (id_a, num_a) = sel_fields(&mut client)?;
        let name_a = temp_name(&mut client)?;
        println!("after PR-A 1 (BS ignored if off): id={id_a} number={num_a} temp='{name_a}'");

        // Ask for PR-B 10 with full Bank Select; with BS off only PC=9 should apply inside current group.
        client.select_sound(ch, 81, 1, 9)?;
        thread::sleep(Duration::from_millis(400));
        let (id_b, num_b) = sel_fields(&mut client)?;
        let name_b = temp_name(&mut client)?;
        println!("after PR-B 10 messages with BS OFF: id={id_b} number={num_b} temp='{name_b}'");
        if id_b != id_a {
            return Err(format!(
                "group id changed with Bank Select OFF ({id_a} -> {id_b}); expected same group"
            )
            .into());
        }
        if num_b != 9 {
            return Err(
                format!("expected program number 9 inside current group, got {num_b}").into(),
            );
        }
        if name_b == name_a {
            return Err("temporary name did not change when PC alone selected patch 10".into());
        }
        println!("H-071 BS-OFF PASS: group stayed {id_a}, number became 9, temp '{name_a}' -> '{name_b}'");

        // Wrong channel: should not change selection.
        let wrong = if ch == 15 { 0 } else { ch + 1 };
        client.select_sound(wrong, 80, 0, 0)?;
        thread::sleep(Duration::from_millis(400));
        let (id_w, num_w) = sel_fields(&mut client)?;
        let name_w = temp_name(&mut client)?;
        if (id_w, num_w) != (id_b, num_b) || name_w != name_b {
            return Err(format!(
                "wrong-channel select changed state to id={id_w} number={num_w} temp='{name_w}'"
            )
            .into());
        }
        println!("H-071 WRONG-CHANNEL PASS: channel {wrong} left id={id_w} number={num_w} temp='{name_w}'");
        Ok(())
    })();

    // Always restore System (Receive Bank Select ON again) and Nice Piano temp.
    let restore = client.restore_system_backup(&plan, &cancel)?;
    if !restore.verified {
        eprintln!("WARNING: System restore after H-071 did not verify");
    } else {
        println!("System settings restored (Receive Bank Select back ON)");
    }
    client.select_sound(ch, 81, 0, 3)?;
    thread::sleep(Duration::from_millis(400));
    let nice = temp_name(&mut client)?;
    if nice != "Nice Piano" {
        eprintln!("WARNING: temp restore expected Nice Piano, got '{nice}'");
    } else {
        println!("Temporary restored to Nice Piano");
    }

    result?;
    println!("H-071 HARDWARE PASS (SysEx stand-in for panel Receive Bank Select OFF)");
    Ok(())
}
