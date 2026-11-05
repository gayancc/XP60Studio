//! Golden tests against a real XP-60 User Patch bank (128 patches, 1,314 messages), supplied by the owner.
//!
//! These prove the transcribed map and the codec against data the instrument itself produced. They do **not** prove
//! what any parameter means, nor how the instrument behaves on the wire.

use std::collections::HashMap;
use xppatch_core::{user_patch_bank_bytes, Patch, SyxImage};
use xppatch_sysex::address::Address;
use xppatch_sysex::memory::{classify, user_patch, WriteKind};
use xppatch_sysex::message::DeviceId;

const BANK: &[u8] = include_bytes!("../../../tests/golden/xp60-user-bank.syx");

fn image() -> SyxImage {
    SyxImage::parse(BANK).expect("the golden bank parses strictly")
}

#[test]
fn the_whole_file_parses_and_reserialises_byte_for_byte() {
    let image = image();
    assert_eq!(image.records.len(), 1314);
    assert!(image.records.iter().all(|r| r.device == DeviceId::DEFAULT));
    assert_eq!(
        image.to_bytes().expect("encodes"),
        BANK,
        "checksums and framing reproduce exactly"
    );
}

#[test]
fn block_shapes_match_the_documented_sizes() {
    let image = image();
    let mut patch_common = 0;
    let mut patch_tone = 0;
    let mut other = 0;
    for r in &image.records {
        let lanes = r.address.bytes();
        if lanes[0] == 0x11 {
            match (lanes[2], lanes[3], r.data.len()) {
                (0, 0, 73) => patch_common += 1,
                (0x10 | 0x12 | 0x14 | 0x16, 0, 129) => patch_tone += 1,
                other_shape => panic!(
                    "unexpected user-patch message {:?} at {}",
                    other_shape, r.address
                ),
            }
        } else {
            other += 1;
            assert_eq!(
                classify(r.address),
                WriteKind::Permanent,
                "{} should be a User region",
                r.address
            );
        }
    }
    assert_eq!((patch_common, patch_tone, other), (128, 512, 674));
}

#[test]
fn all_128_patches_decode_with_every_value_in_range() {
    let patches = image().user_patches().expect("every patch is complete");
    assert_eq!(patches.len(), 128);
    let mut values = 0usize;
    for (n, (slot, patch)) in patches.iter().enumerate() {
        assert_eq!(usize::from(*slot), n, "slots are contiguous from 0");
        let issues = patch.validate();
        assert!(issues.is_empty(), "slot {slot}: {issues:?}");
        values += patch.common.def().params.len() + 4 * patch.tones[0].def().params.len();
        let name = patch.name();
        assert!(
            name.len() <= 12 && name.chars().all(|c| (' '..='~').contains(&c)),
            "slot {slot}: {name:?}"
        );
    }
    // 128 patches x (72 + 4 x 128) values.
    assert_eq!(values, 74_752);
}

#[test]
fn every_patch_reencodes_to_the_original_bytes() {
    let image = image();
    let originals: HashMap<Address, &Vec<u8>> =
        image.records.iter().map(|r| (r.address, &r.data)).collect();
    for (slot, patch) in image.user_patches().expect("complete") {
        let base = user_patch(slot).expect("slot");
        let pieces = patch.to_dt1(base).expect("encodes");
        assert_eq!(pieces.len(), 5);
        for (address, data) in pieces {
            assert_eq!(
                originals.get(&address).copied(),
                Some(&data),
                "slot {slot} block at {address}"
            );
        }
    }
}

#[test]
fn exporting_the_patches_reproduces_the_patch_messages_of_the_file() {
    let image = image();
    let patches = image.user_patches().expect("complete");
    let exported = user_patch_bank_bytes(&patches, DeviceId::DEFAULT, None).expect("exports");
    let original_patch_bytes: Vec<u8> = SyxImage {
        records: image
            .records
            .iter()
            .filter(|r| r.address.bytes()[0] == 0x11)
            .cloned()
            .collect(),
    }
    .to_bytes()
    .expect("encodes");
    // Same messages; the file may order blocks differently, so compare as sorted message sets.
    let as_set = |bytes: &[u8]| {
        let mut v: Vec<Vec<u8>> = xppatch_sysex::stream::frames(bytes)
            .map(std::borrow::Cow::into_owned)
            .collect();
        v.sort();
        v
    };
    assert_eq!(as_set(&exported), as_set(&original_patch_bytes));
}

#[test]
fn a_split_export_for_sending_is_equivalent_and_reads_back_identically() {
    let patches = image().user_patches().expect("complete");
    let to_send =
        user_patch_bank_bytes(&patches, DeviceId::DEFAULT, Some(128)).expect("exports split");
    let reread = SyxImage::parse(&to_send).expect("parses");
    assert!(
        reread.records.iter().all(|r| r.data.len() <= 128),
        "no piece exceeds the 128-byte limit"
    );
    assert_eq!(reread.user_patches().expect("complete"), patches);
}

#[test]
fn a_written_patch_matches_what_a_dump_reports() {
    // The write path (edit, split, send) and the read path (dump, reassemble) agree for a real patch.
    let (slot, original) = image().user_patches().expect("complete").remove(17);
    let mut edited = original.clone();
    edited.set_name("Edited").expect("valid name");
    let bytes = user_patch_bank_bytes(&[(slot, edited.clone())], DeviceId::DEFAULT, Some(128))
        .expect("exports");
    let back: Vec<(u8, Patch)> = SyxImage::parse(&bytes)
        .expect("parses")
        .user_patches()
        .expect("complete");
    assert_eq!(back, vec![(slot, edited)]);
    assert_ne!(original.name(), "Edited");
}
