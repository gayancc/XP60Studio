//! Copy one User slot onto another (PERM). Requires Protect OFF. Verifies the destination.
//! Usage: write_slot_copy <input> <output> <from-one-based> <to-one-based>
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, from_text, to_text] = args.as_slice() else {
        return Err("pass input, output, from-slot, to-slot (one-based)".into());
    };
    let from: u8 = from_text.parse::<u8>()?.checked_sub(1).ok_or("from")?;
    let to: u8 = to_text.parse::<u8>()?.checked_sub(1).ok_or("to")?;
    if from > 127 || to > 127 {
        return Err("slots must be 1..128".into());
    }
    if from == to {
        return Err("from and to must differ".into());
    }
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let source = client.read_user_slot(from, &cancel)?;
    let plan = client.plan_user_write(to, &cancel)?;
    if plan.backup() == &source {
        println!(
            "User slot {} already equals slot {}; nothing written",
            to + 1,
            from + 1
        );
        return Ok(());
    }
    let before_name = plan.backup().name().trim_end().to_owned();
    let report = client.commit_user_write(&plan, &source, &cancel)?;
    if !report.verified || client.read_user_slot(to, &cancel)? != source {
        return Err("destination write was not verified".into());
    }
    println!(
        "Copied User slot {} '{}' -> slot {} (was '{}')",
        from + 1,
        source.name().trim_end(),
        to + 1,
        before_name
    );
    Ok(())
}
