//! Hardware session, PERMANENT part, protected case (H-031, H-038): with User Memory Protect on, a write to one User slot must be refused
//! (reported as a mismatch) and the slot must still hold what it held. The slot is read before and after, and the original is put back
//! only if the slot changed.
//!
//! cargo run -p xppatch-midi --example protect_probe -- <input name> <output name> <slot 1 to 128>

use xppatch_core::Patch;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig, ClientError};
use xppatch_sysex::message::DeviceId;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, slot] = a.as_slice() else {
        return Err("pass an input name, an output name and a slot (1 to 128)".into());
    };
    let index = slot.parse::<u8>()?.checked_sub(1).ok_or("slot 1 to 128")?;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    client.set_device(DeviceId::DEFAULT);

    let plan = client.plan_user_write(index, &cancel)?;
    println!(
        "slot {slot} holds '{}' before",
        plan.occupant_name().trim_end()
    );
    let mut test = Patch::init();
    test.set_name("HW TEST 128")?;
    match client.commit_user_write(&plan, &test, &cancel) {
        Ok(r) => println!(
            "RESULT: the write was ACCEPTED ({} messages, verified {}): Protect is not on",
            r.messages, r.verified
        ),
        Err(ClientError::Mismatch { blocks }) => {
            println!("RESULT: refused as a MISMATCH after the write, blocks {blocks:?}")
        }
        Err(e) => println!("RESULT: refused: {e}"),
    }
    let after = client.fetch_user_patch(index, &cancel, &mut |_| {})?;
    println!(
        "slot {slot} after: '{}', equal to before: {}",
        after.name().trim_end(),
        after == *plan.backup()
    );
    if after != *plan.backup() {
        let r = client.restore_user_backup(&plan, &cancel)?;
        let again = client.fetch_user_patch(index, &cancel, &mut |_| {})?;
        println!("the slot had changed, so the original was written back ({} messages, verified {}); equal now: {}", r.messages, r.verified, again == *plan.backup());
    }
    Ok(())
}
