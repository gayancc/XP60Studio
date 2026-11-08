//! Read only: inspect current mode, selected patch and keyboard/MIDI routing values.
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output] = args.as_slice() else {
        return Err("pass input and output names".into());
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let doc = client.read_system(&CancelToken::new())?;
    let common = doc.block(0).ok_or("missing System Common")?;
    for id in [
        "sound_mode",
        "patch_group_type",
        "patch_group_id",
        "patch_number",
        "patch_receive_channel",
        "patch_transmit_channel",
        "local_switch",
        "receive_program_change",
        "receive_bank_select",
    ] {
        match common.get(id) {
            Ok(value) => println!("{id}: {value}"),
            Err(error) => println!("{id}: {error}"),
        }
    }
    Ok(())
}
