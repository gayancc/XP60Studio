//! Emergency note release on every MIDI channel. No patch or System data writes.
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{Client, ClientConfig};
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output] = args.as_slice() else {
        return Err("pass input and output names".into());
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    for channel in 0..16 {
        client.all_notes_off(channel)?;
    }
    println!("PANIC SENT: All Notes Off on channels 1-16");
    Ok(())
}
