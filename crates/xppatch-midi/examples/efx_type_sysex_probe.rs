//! H-073 observation only: does writing efx_type alone via SysEx load that type's starting
//! parameter values the way a panel type change would? Restores the temporary patch.
//! Does not claim panel defaults; records instrument SysEx behaviour.
//! Usage: efx_type_sysex_probe <input> <output>
use xppatch_core::{Patch, PatchBlock};
use xppatch_midi::hardware::MidirTransport;
use xppatch_midi::{CancelToken, Client, ClientConfig};
use xppatch_sysex::memory::TEMP_PATCH;
use xppatch_sysex::message::{DeviceId, Message};

type Res<T> = Result<T, Box<dyn std::error::Error>>;

fn frame_for(patch: &Patch, id: &str) -> Res<Vec<u8>> {
    let (address, data) = patch.param_dt1(TEMP_PATCH, PatchBlock::Common, id)?;
    Ok(Message::Dt1 { address, data }.encode(DeviceId::DEFAULT)?)
}

fn efx_params(patch: &Patch) -> Res<(i32, [i32; 12])> {
    let c = patch.block(PatchBlock::Common).ok_or("missing Common")?;
    let ty = c.get("efx_type")?;
    let mut p = [0i32; 12];
    for i in 0..12 {
        p[i] = c.get(&format!("efx_parameter_{}", i + 1))?;
    }
    Ok((ty, p))
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
    let original = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    let (ty0, p0) = efx_params(&original)?;
    println!(
        "baseline temp '{}' efx_type={} params={p0:?}",
        original.name().trim_end(),
        ty0
    );

    // Pick a different type within 0..39.
    let ty1 = if ty0 == 0 { 8 } else { 0 }; // 8 = ROTARY in app tables
    let mut edited = original.clone();
    edited
        .block_mut(PatchBlock::Common)
        .ok_or("missing Common")?
        .set("efx_type", ty1)?;
    client.send_temp_frame(&frame_for(&edited, "efx_type")?, &cancel)?;
    let after = client.fetch_temp_patch(&cancel, &mut |_| {})?;
    let (ty_back, p_back) = efx_params(&after)?;
    let params_unchanged = p_back == p0;
    println!("wrote efx_type={ty1}; read-back type={ty_back}; params={p_back:?}");
    println!("twelve EFX parameters unchanged after SysEx type-only write: {params_unchanged}");
    if params_unchanged {
        println!(
            "H-073 NOTE: SysEx efx_type write does not load type defaults; panel type walk remains required"
        );
    } else {
        println!(
            "H-073 NOTE: SysEx efx_type write changed parameters; compare against data/efx-defaults.json before claiming panel parity"
        );
        for i in 0..12 {
            if p_back[i] != p0[i] {
                println!("  param {}: {} -> {}", i + 1, p0[i], p_back[i]);
            }
        }
    }

    client.send_temp_patch(&original, true, &cancel)?;
    if client.fetch_temp_patch(&cancel, &mut |_| {})? != original {
        return Err("final restore of temporary patch failed".into());
    }
    println!("temporary patch restored");
    Ok(())
}
