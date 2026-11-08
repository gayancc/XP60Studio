//! Hardware session diagnostic for H-050b: which changes to a stored patch's Common block does the instrument keep when the patch is rewritten? Each variant
//! rewrites one User slot, reads it back, reports whether the Common block took the change, and puts the original back.
//!
//! cargo run -p xppatch-midi --example common_probe -- <input name> <output name> <slot 1 to 128>

use xppatch_core::{Patch, PatchBlock};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::message::DeviceId;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let a: Vec<String> = std::env::args().skip(1).collect();
    let [input, output, slot] = a.as_slice() else {
        return Err("pass an input name, an output name and a slot".into());
    };
    let index = slot.parse::<u8>()?.checked_sub(1).ok_or("slot 1 to 128")?;
    let mut client = Client::new(
        MidirTransport::connect(input, output)?,
        ClientConfig::default(),
    );
    let cancel = CancelToken::new();
    client.set_device(DeviceId::DEFAULT);
    let original = client.plan_user_write(index, &cancel)?.backup().clone();
    let init_common = Patch::init()
        .block(PatchBlock::Common)
        .cloned()
        .ok_or("init common")?;

    type Edit = Box<dyn Fn(&mut Patch) -> Result<(), Box<dyn std::error::Error>>>;
    let variants: Vec<(&str, Edit)> = vec![
        ("name only", Box::new(|p| Ok(p.set_name("PROBE NAME")?))),
        (
            "one EFX parameter (efx_parameter_1 + 1)",
            Box::new(|p| {
                let b = p.block_mut(PatchBlock::Common).ok_or("common")?;
                let v = b.get("efx_parameter_1")?;
                Ok(b.set("efx_parameter_1", if v < 127 { v + 1 } else { v - 1 })?)
            }),
        ),
        (
            "chorus level + 1",
            Box::new(|p| {
                let b = p.block_mut(PatchBlock::Common).ok_or("common")?;
                let v = b.get("chorus_level")?;
                Ok(b.set("chorus_level", if v < 127 { v + 1 } else { v - 1 })?)
            }),
        ),
        ("Common replaced by the initial Common, with a name", {
            let init = init_common.clone();
            Box::new(move |p| {
                *p.block_mut(PatchBlock::Common).ok_or("common")? = init.clone();
                Ok(p.set_name("INIT COMMON")?)
            })
        }),
    ];
    for (label, edit) in variants {
        let plan = client.plan_user_write(index, &cancel)?;
        let mut changed = plan.backup().clone();
        edit(&mut changed)?;
        let result = client.commit_user_write(&plan, &changed, &cancel);
        let back = client.fetch_user_patch(index, &cancel, &mut |_| {})?;
        let common = |p: &Patch| {
            p.block(PatchBlock::Common)
                .map(|b| b.bytes().to_vec())
                .unwrap_or_default()
        };
        let (sent, got, was) = (common(&changed), common(&back), common(&original));
        let kept = (0..sent.len())
            .filter(|&i| sent[i] != was[i] && got[i] == sent[i])
            .count();
        let wanted = (0..sent.len()).filter(|&i| sent[i] != was[i]).count();
        println!(
            "{label}: {wanted} byte(s) to change; the instrument kept {kept}; result: {}",
            result.map_or_else(
                |e| e.to_string(),
                |r| format!("ok, verified {}", r.verified)
            )
        );
        let r = client.restore_user_backup(&plan, &cancel)?;
        let again = client.fetch_user_patch(index, &cancel, &mut |_| {})?;
        println!(
            "   restored ({} messages, verified {}); the slot equals the original: {}",
            r.messages,
            r.verified,
            again == original
        );
    }
    Ok(())
}
