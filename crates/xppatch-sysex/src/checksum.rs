//! Roland checksum.
//!
//! `checksum = (128 - (sum mod 128)) mod 128` over the address bytes and the size (RQ1) or data (DT1) bytes. The
//! message is valid when the sum including the checksum is `0 mod 128`.
//!
//! Evidence: the manual's published examples, the Patch Base code (`(-sum) & 0x7F`) and a real XP-60 bank.

/// The checksum byte for `bytes` (address plus size or data bytes).
pub fn checksum(bytes: &[u8]) -> u8 {
    let sum: u64 = bytes.iter().map(|&b| u64::from(b)).sum();
    ((128 - (sum % 128)) % 128) as u8
}

/// True when `bytes_with_checksum` (address, data, then the checksum byte) sums to zero modulo 128.
pub fn is_valid(bytes_with_checksum: &[u8]) -> bool {
    let sum: u64 = bytes_with_checksum.iter().map(|&b| u64::from(b)).sum();
    sum.is_multiple_of(128)
}

#[cfg(test)]
mod tests {
    use super::*;
    use proptest::prelude::*;

    #[test]
    fn manual_rq1_example() {
        // F0 41 10 6A 11 | 01 00 00 00 | 00 00 1F 19 | 47 F7  (temporary performance, 3993 units)
        let body = [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x19];
        assert_eq!(checksum(&body), 0x47);
    }

    #[test]
    fn manual_dt1_example() {
        // F0 41 10 6A 12 | 01 00 00 28 | 06 | 51 F7
        assert_eq!(checksum(&[0x01, 0x00, 0x00, 0x28, 0x06]), 0x51);
    }

    #[test]
    fn empty_and_zero_sum_give_zero() {
        assert_eq!(checksum(&[]), 0);
        assert_eq!(checksum(&[0x40, 0x40]), 0);
    }

    proptest! {
        #[test]
        fn appended_checksum_always_validates(body in proptest::collection::vec(0u8..=0x7F, 0..300)) {
            let mut with = body.clone();
            with.push(checksum(&body));
            prop_assert!(is_valid(&with));
            prop_assert!(checksum(&body) <= 0x7F);
        }

        #[test]
        fn any_single_bit_error_is_detected(body in proptest::collection::vec(0u8..=0x7F, 1..64), i in 0usize..64, bit in 0u8..7) {
            let mut with = body.clone();
            with.push(checksum(&body));
            let idx = i % with.len();
            with[idx] ^= 1 << bit;
            prop_assert!(!is_valid(&with));
        }
    }
}
