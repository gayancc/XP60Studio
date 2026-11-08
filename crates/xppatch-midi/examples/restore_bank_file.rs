//! H-049: send a whole-bank `.syx` backup to the XP-60 with this MIDI tool, then prove every User slot matches.
//! Requires User Memory Protect OFF. Writes only slots that differ from the file.
//! Usage: restore_bank_file <input> <output> <bank.syx>
use xppatch_core::{Patch, SyxImage};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn read_all(client: &mut Client<MidirTransport>) -> Res<Vec<(u8, Patch)>> {
    let cancel = CancelToken::new();
    let mut bank = client.fetch_bank(0..128, &cancel, &mut |_| {})?;
    if bank.cancelled {
        return Err("bank read cancelled".into());
    }
    let mut have: std::collections::BTreeSet<u8> = bank.patches.iter().map(|(s, _)| *s).collect();
    for (slot, err) in bank.failures {
        println!("missed User {}; retrying ({err})", u16::from(slot) + 1);
        bank.patches
            .push((slot, client.read_user_slot(slot, &cancel)?));
        have.insert(slot);
    }
    if bank.aborted {
        for slot in 0..128u8 {
            if have.contains(&slot) {
                continue;
            }
            println!(
                "aborted before User {}; reading independently",
                u16::from(slot) + 1
            );
            bank.patches
                .push((slot, client.read_user_slot(slot, &cancel)?));
        }
    }
    if bank.patches.len() != 128 {
        return Err(format!("incomplete bank: {} of 128", bank.patches.len()).into());
    }
    bank.patches.sort_by_key(|(s, _)| *s);
    Ok(bank.patches)
}

fn main() -> Res<()> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, file] = args.as_slice() else {
        return Err("pass input, output, bank.syx".into());
    };
    let expected = SyxImage::parse(&std::fs::read(file)?)?.user_patches()?;
    if expected.len() != 128 {
        return Err("bank file must hold all 128 User patches".into());
    }
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let before = read_all(&mut client)?;
    let changed: Vec<(u8, Patch)> = expected
        .iter()
        .filter(|(s, p)| before.iter().find(|(n, _)| n == s).map(|(_, q)| q) != Some(p))
        .map(|(s, p)| (*s, p.clone()))
        .collect();
    println!(
        "Slots that differ from {} (one-based): {:?}",
        file,
        changed
            .iter()
            .map(|(s, _)| u16::from(*s) + 1)
            .collect::<Vec<_>>()
    );
    if changed.is_empty() {
        println!("BANK FILE ALREADY MATCHES INSTRUMENT: nothing written");
        return Ok(());
    }
    let cancel = CancelToken::new();
    let mut plans = Vec::with_capacity(changed.len());
    for (slot, _) in &changed {
        plans.push(client.plan_user_write(*slot, &cancel)?);
    }
    let report = client.commit_user_bank(&plans, &changed, &cancel, &mut |_| {})?;
    println!("Bank file send report: {report:?}");
    if report.cancelled || report.stopped_at.is_some() || report.written.len() != changed.len() {
        return Err(format!("bank file send incomplete: {report:?}").into());
    }
    let after = read_all(&mut client)?;
    let remaining: Vec<u16> = expected
        .iter()
        .filter(|(s, p)| after.iter().find(|(n, _)| n == s).map(|(_, q)| q) != Some(p))
        .map(|(s, _)| u16::from(*s) + 1)
        .collect();
    if !remaining.is_empty() {
        return Err(format!("after send, slots still differ: {remaining:?}").into());
    }
    println!(
        "H-049 MIDI TOOL PASSED: wrote {} differing slot(s) from {}; all 128 equal the file",
        changed.len(),
        file
    );
    Ok(())
}
