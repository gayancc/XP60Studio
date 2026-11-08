//! Read-only diagnostic: a User-slot request must not change the temporary patch.
//! The transport refuses every outgoing message except a Roland RQ1 and logs every transmitted frame.
use std::time::Duration;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig, MidiTransport, TransportError};

struct ReadOnly(MidirTransport);
impl MidiTransport for ReadOnly {
    fn send(&mut self, bytes: &[u8]) -> Result<(), TransportError> {
        if bytes.len() < 11
            || bytes[0] != 0xf0
            || bytes[1] != 0x41
            || bytes[3] != 0x6a
            || bytes[4] != 0x11
        {
            return Err(TransportError::Send(
                "diagnostic refused a non-RQ1 message".into(),
            ));
        }
        println!(
            "TX RQ1: {}",
            bytes
                .iter()
                .map(|b| format!("{b:02X}"))
                .collect::<Vec<_>>()
                .join(" ")
        );
        self.0.send(bytes)
    }
    fn recv(&mut self, timeout: Duration) -> Result<Option<Vec<u8>>, TransportError> {
        self.0.recv(timeout)
    }
    fn now(&self) -> Duration {
        self.0.now()
    }
    fn sleep(&mut self, duration: Duration) {
        self.0.sleep(duration);
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, folder] = args.as_slice() else {
        return Err("pass input, output, evidence folder".into());
    };
    std::fs::create_dir_all(folder)?;
    let mut client = Client::new(
        ReadOnly(MidirTransport::connect(input, output)?),
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    let before = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    std::fs::write(
        std::path::Path::new(folder).join("before-user-read.blocks"),
        before.to_blocks(),
    )?;
    println!("Temporary patch before: '{}'", before.name().trim_end());
    let user = client.read_user_slot(0, &cancel)?;
    println!("Read User slot 1: '{}'", user.name().trim_end());
    let after = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    std::fs::write(
        std::path::Path::new(folder).join("after-user-read.blocks"),
        after.to_blocks(),
    )?;
    println!(
        "Temporary patch after: '{}'; bytes equal: {}",
        after.name().trim_end(),
        after == before
    );
    if before != after {
        return Err("User read changed the temporary patch; no writes were sent".into());
    }
    println!("READ SIDE EFFECT CHECK PASSED");
    Ok(())
}
