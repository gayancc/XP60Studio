//! The parameter map against the Owner's Manual.
//!
//! `tests/golden/manual-verified-map.json` holds the offset, byte count and range of every parameter and the size of every block, as read from
//! the page images of the XP-60/XP-80 Owner's Manual (Parameter Address Map) and compared row by row with the map. This test keeps the two
//! from drifting apart: a change to the map must be a change to the manual reading too, and that is a decision, not an accident.

use serde::Deserialize;
use std::collections::BTreeMap;
use xppatch_core::ParameterMap;

#[derive(Deserialize)]
struct Fixture {
    blocks: BTreeMap<String, FixtureBlock>,
}

#[derive(Deserialize)]
struct FixtureBlock {
    size: usize,
    rows: Vec<[i64; 4]>,
}

const FIXTURE: &str = include_str!("../../../tests/golden/manual-verified-map.json");

fn fixture() -> Fixture {
    serde_json::from_str(FIXTURE).expect("the manual fixture is valid JSON")
}

#[test]
fn every_block_has_the_size_the_manual_gives() {
    let map = ParameterMap::embedded();
    let fx = fixture();
    assert_eq!(
        map.blocks().len(),
        fx.blocks.len(),
        "the map and the manual reading list different blocks"
    );
    for (id, expected) in &fx.blocks {
        let block = map
            .block(id)
            .unwrap_or_else(|| panic!("block {id} is in the manual reading but not in the map"));
        assert_eq!(block.size, expected.size, "size of {id}");
    }
}

#[test]
fn every_parameter_has_the_offset_size_and_range_the_manual_gives() {
    let map = ParameterMap::embedded();
    for (id, expected) in &fixture().blocks {
        let block = map.block(id).expect("block exists");
        assert_eq!(
            block.params.len(),
            expected.rows.len(),
            "number of parameters in {id}"
        );
        for (param, row) in block.params.iter().zip(&expected.rows) {
            let got = [
                param.offset as i64,
                param.bytes as i64,
                i64::from(param.min),
                i64::from(param.max),
            ];
            assert_eq!(&got, row, "{id} / {} (offset, bytes, min, max)", param.name);
        }
    }
}

#[test]
fn the_two_errors_found_when_the_manual_was_read_stay_fixed() {
    let map = ParameterMap::embedded();
    let beat = map
        .block("system_common")
        .and_then(|b| b.param("arpeggio_beat_pattern"))
        .expect("beat pattern");
    assert_eq!(
        (beat.min, beat.max),
        (0, 60),
        "Owner's Manual p.222: Arpeggio Beat Pattern 0 - 60 (1 - 61)"
    );
    let mute = map
        .block("rhythm_note")
        .and_then(|b| b.param("mute_group"))
        .expect("mute group");
    assert_eq!(
        (mute.min, mute.max),
        (0, 31),
        "Owner's Manual p.225: Mute Group 0 - 31 (OFF, 1 - 31); a five-bit field cannot hold 32"
    );
}

#[test]
fn the_system_common_block_is_96_bytes_as_the_manual_prints_it() {
    let map = ParameterMap::embedded();
    assert_eq!(
        map.block("system_common").map(|b| b.size),
        Some(0x60),
        "Owner's Manual p.222: Total size 00 00 00 60"
    );
}
