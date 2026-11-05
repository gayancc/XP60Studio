//! Round-trip every parameter of every block, across its entire range, with no cross-talk between parameters.

use proptest::prelude::*;
use xppatch_core::{BlockBuf, CodecError, ParameterMap};

#[test]
fn every_value_of_every_parameter_roundtrips_without_touching_other_bytes() {
    let map = ParameterMap::embedded();
    let mut sets = 0usize;
    for def in map.blocks() {
        let baseline = BlockBuf::init(def);
        for p in &def.params {
            let own = p.offset..p.offset + p.bytes;
            for value in p.min..=p.max {
                let mut b = baseline.clone();
                b.set_param(p, value).expect("in-range value is accepted");
                assert_eq!(
                    b.get_param(p).expect("decodes"),
                    value,
                    "{}.{} = {value}",
                    def.id,
                    p.id
                );
                let (a, c) = (b.bytes(), baseline.bytes());
                assert_eq!(
                    &a[..own.start],
                    &c[..own.start],
                    "{}.{} changed earlier bytes",
                    def.id,
                    p.id
                );
                assert_eq!(
                    &a[own.end..],
                    &c[own.end..],
                    "{}.{} changed later bytes",
                    def.id,
                    p.id
                );
                sets += 1;
            }
        }
    }
    // A floor, not an exact count: it trips if the map shrinks or the loop stops exercising ranges.
    assert!(sets > 20_000, "only {sets} values exercised");
}

#[test]
fn values_just_outside_every_range_are_rejected_and_leave_the_block_unchanged() {
    for def in ParameterMap::embedded().blocks() {
        for p in &def.params {
            let mut b = BlockBuf::init(def);
            let before = b.bytes().to_vec();
            for bad in [p.min - 1, p.max + 1] {
                assert!(
                    matches!(b.set_param(p, bad), Err(CodecError::OutOfRange { .. })),
                    "{}.{} accepted {bad}",
                    def.id,
                    p.id
                );
            }
            assert_eq!(b.bytes(), before.as_slice());
        }
    }
}

#[test]
fn encoded_bytes_are_always_7_bit_and_decode_through_from_bytes() {
    for def in ParameterMap::embedded().blocks() {
        for p in &def.params {
            for value in [p.min, p.max] {
                let mut b = BlockBuf::init(def);
                b.set_param(p, value).expect("in range");
                assert!(b.bytes().iter().all(|&x| x <= 0x7F));
                let again = BlockBuf::from_bytes(def, b.bytes()).expect("re-wrapped");
                assert_eq!(again, b);
                assert!(again.validate().is_empty());
            }
        }
    }
}

fn pick(seed: u32, min: i32, max: i32) -> i32 {
    min + i32::try_from(seed % u32::try_from(max - min + 1).expect("positive span")).expect("fits")
}

proptest! {
    #[test]
    fn random_blocks_roundtrip(seeds in proptest::collection::vec(any::<u32>(), 130)) {
        for def in ParameterMap::embedded().blocks() {
            let mut b = BlockBuf::init(def);
            let mut expected = Vec::new();
            for (p, &seed) in def.params.iter().zip(seeds.iter().cycle()) {
                let v = pick(seed, p.min, p.max);
                b.set_param(p, v).expect("in range");
                expected.push((p, v));
            }
            for (p, v) in expected {
                prop_assert_eq!(b.get_param(p).expect("decodes"), v);
            }
            prop_assert!(b.validate().is_empty());
        }
    }
}
