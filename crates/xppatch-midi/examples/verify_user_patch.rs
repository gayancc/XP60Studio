//! Read-only: compare the one User patch in a backup .syx with the instrument's corresponding slot.
use xppatch_core::SyxImage;
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let ([input, output, file], requested) = match args.as_slice() {
        [input, output, file] => ([input, output, file], None),
        [input, output, file, slot] => ([input, output, file], Some(slot.parse::<u8>()?)),
        _ => return Err("pass input, output, backup.syx, optionally one-based slot".into()),
    };
    let patches = SyxImage::parse(&std::fs::read(file)?)?.user_patches()?;
    let (slot, expected) = if let Some(number) = requested {
        if !(1..=128).contains(&number) {
            return Err("slot must be 1 to 128".into());
        }
        patches
            .iter()
            .find(|(slot, _)| *slot == number - 1)
            .map(|(slot, expected)| (slot, expected))
            .ok_or("requested slot missing from backup")?
    } else {
        let [(slot, expected)] = patches.as_slice() else {
            return Err("backup must contain exactly one User patch unless slot is given".into());
        };
        (slot, expected)
    };
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let actual = client.read_user_slot(*slot, &CancelToken::new())?;
    if actual != *expected {
        return Err(format!("User slot {} differs from its backup", u16::from(*slot) + 1).into());
    }
    println!(
        "USER SLOT VERIFICATION PASSED: slot {} '{}', every byte equals backup",
        u16::from(*slot) + 1,
        actual.name().trim_end()
    );
    Ok(())
}
