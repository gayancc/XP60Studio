//! Loading and validating the parameter map.

use crate::schema::{BlockDef, Document, Encoding, Limits};
use std::collections::HashMap;
use std::sync::OnceLock;
use thiserror::Error;

const EMBEDDED_JSON: &str = include_str!("../../../data/xp60-parameters.json");
const SUPPORTED_SCHEMA: u32 = 1;

#[derive(Debug, Error, PartialEq, Eq)]
#[non_exhaustive]
pub enum MapError {
    #[error("parameter map is not valid JSON: {0}")]
    Json(String),
    #[error("unsupported schema version {0}")]
    Schema(u32),
    #[error("duplicate block id {0:?}")]
    DuplicateBlock(String),
    #[error("block {block:?}: duplicate parameter id {param:?}")]
    DuplicateParam { block: String, param: String },
    #[error("block {block:?}: parameter {param:?} has min {min} greater than max {max}")]
    BadRange {
        block: String,
        param: String,
        min: i32,
        max: i32,
    },
    #[error("block {block:?}: parameter {param:?} has a negative minimum")]
    NegativeMin { block: String, param: String },
    #[error("block {block:?}: parameter {param:?} must span 1 or more bytes")]
    NoBytes { block: String, param: String },
    #[error("block {block:?}: parameter {param:?} ({encoding:?}) must use exactly 1 byte")]
    WrongWidth {
        block: String,
        param: String,
        encoding: Encoding,
    },
    #[error("block {block:?}: parameter {param:?} ends at byte {end}, past the block size {size}")]
    PastEnd {
        block: String,
        param: String,
        end: usize,
        size: usize,
    },
    #[error("block {block:?}: parameters {a:?} and {b:?} overlap")]
    Overlap { block: String, a: String, b: String },
    #[error(
        "block {block:?}: parameter {param:?} cannot hold its maximum {max} in {bytes} byte(s)"
    )]
    TooWide {
        block: String,
        param: String,
        max: i32,
        bytes: usize,
    },
    #[error(
        "block {block:?}: parameter {param:?} uses {bytes} nibble bytes (at most 4 are supported)"
    )]
    NibbleTooWide {
        block: String,
        param: String,
        bytes: usize,
    },
    #[error("block {block:?} is {size} bytes (limit 1024)")]
    BlockTooLarge { block: String, size: usize },
    #[error("invalid transfer limits: {0}")]
    BadLimits(String),
}

/// A validated parameter map.
#[derive(Debug)]
pub struct ParameterMap {
    model: String,
    limits: Limits,
    blocks: Vec<BlockDef>,
    index: HashMap<String, usize>,
}

impl ParameterMap {
    /// The map compiled into the binary (`data/xp60-parameters.json`).
    pub fn embedded() -> &'static ParameterMap {
        static MAP: OnceLock<ParameterMap> = OnceLock::new();
        MAP.get_or_init(|| {
            // The embedded file is validated by the test suite; a failure here is a build defect, not a runtime condition.
            ParameterMap::from_json(EMBEDDED_JSON).expect("embedded parameter map is valid")
        })
    }

    /// Parse and validate a map.
    pub fn from_json(json: &str) -> Result<ParameterMap, MapError> {
        let doc: Document =
            serde_json::from_str(json).map_err(|e| MapError::Json(e.to_string()))?;
        if doc.schema != SUPPORTED_SCHEMA {
            return Err(MapError::Schema(doc.schema));
        }
        let l = &doc.limits;
        if !(1..=128).contains(&l.max_dt1_data_to_device) {
            return Err(MapError::BadLimits(format!(
                "max_dt1_data_to_device {} is outside 1..=128",
                l.max_dt1_data_to_device
            )));
        }
        if l.first_response_timeout_ms == 0 {
            return Err(MapError::BadLimits(
                "first_response_timeout_ms must be positive".to_owned(),
            ));
        }
        let mut blocks = doc.blocks;
        let mut index = HashMap::new();
        for (i, block) in blocks.iter_mut().enumerate() {
            if index.insert(block.id.clone(), i).is_some() {
                return Err(MapError::DuplicateBlock(block.id.clone()));
            }
            validate_block(block)?;
        }
        Ok(ParameterMap {
            model: doc.model,
            limits: doc.limits,
            blocks,
            index,
        })
    }

    pub fn model(&self) -> &str {
        &self.model
    }

    pub fn limits(&self) -> &Limits {
        &self.limits
    }

    pub fn blocks(&self) -> &[BlockDef] {
        &self.blocks
    }

    pub fn block(&self, id: &str) -> Option<&BlockDef> {
        self.index.get(id).map(|&i| &self.blocks[i])
    }

    /// Total number of parameters across all blocks.
    pub fn parameter_count(&self) -> usize {
        self.blocks.iter().map(|b| b.params.len()).sum()
    }
}

const MAX_BLOCK_SIZE: usize = 1024;
const MAX_NIBBLE_BYTES: usize = 4;

fn validate_block(block: &mut BlockDef) -> Result<(), MapError> {
    let name = block.id.clone();
    if block.size > MAX_BLOCK_SIZE {
        return Err(MapError::BlockTooLarge {
            block: name,
            size: block.size,
        });
    }
    for (i, p) in block.params.iter().enumerate() {
        if block.index.insert(p.id.clone(), i).is_some() {
            return Err(MapError::DuplicateParam {
                block: name,
                param: p.id.clone(),
            });
        }
        if p.bytes == 0 {
            return Err(MapError::NoBytes {
                block: name,
                param: p.id.clone(),
            });
        }
        if p.min > p.max {
            return Err(MapError::BadRange {
                block: name,
                param: p.id.clone(),
                min: p.min,
                max: p.max,
            });
        }
        if p.min < 0 {
            return Err(MapError::NegativeMin {
                block: name,
                param: p.id.clone(),
            });
        }
        if p.encoding != Encoding::Nibble && p.bytes != 1 {
            return Err(MapError::WrongWidth {
                block: name,
                param: p.id.clone(),
                encoding: p.encoding,
            });
        }
        if p.encoding == Encoding::Nibble && p.bytes > MAX_NIBBLE_BYTES {
            return Err(MapError::NibbleTooWide {
                block: name,
                param: p.id.clone(),
                bytes: p.bytes,
            });
        }
        // A wrapped sum would sneak past the bound, so overflow counts as past the end.
        let end = p.offset.saturating_add(p.bytes);
        if end > block.size {
            return Err(MapError::PastEnd {
                block: name,
                param: p.id.clone(),
                end,
                size: block.size,
            });
        }
        let capacity_bits = match p.encoding {
            Encoding::Seven | Encoding::Ascii => 7,
            Encoding::Nibble => 4 * p.bytes,
        };
        if i64::from(p.max) >= 1i64 << capacity_bits {
            return Err(MapError::TooWide {
                block: name,
                param: p.id.clone(),
                max: p.max,
                bytes: p.bytes,
            });
        }
    }
    // Overlap check on a sorted copy of the byte ranges (every end is already known to be within the block).
    let mut spans: Vec<(usize, usize, &str)> = block
        .params
        .iter()
        .map(|p| (p.offset, p.offset + p.bytes, p.id.as_str()))
        .collect();
    spans.sort_unstable();
    if let Some(pair) = spans.windows(2).find(|w| w[0].1 > w[1].0) {
        return Err(MapError::Overlap {
            block: name,
            a: pair[0].2.to_owned(),
            b: pair[1].2.to_owned(),
        });
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn doc(params: &str, size: usize) -> String {
        format!(
            r#"{{"schema":1,"model":"T","limits":{{"max_dt1_data_to_device":128,"min_dt1_gap_ms":20,"first_response_timeout_ms":1500}},
            "blocks":[{{"id":"b","family":"f","name":"B","size":{size},"params":[{params}]}}]}}"#
        )
    }

    fn p(id: &str, offset: usize, bytes: usize, enc: &str, min: i32, max: i32) -> String {
        format!(
            r#"{{"id":"{id}","name":"{id}","offset":{offset},"bytes":{bytes},"encoding":"{enc}","min":{min},"max":{max},"display":"d"}}"#
        )
    }

    #[test]
    fn embedded_map_has_the_documented_shape() {
        let m = ParameterMap::embedded();
        assert_eq!(m.model(), "XP-60");
        assert_eq!(m.blocks().len(), 8);
        assert_eq!(m.parameter_count(), 464);
        assert_eq!(m.block("patch_common").map(|b| b.size), Some(73));
        assert_eq!(m.block("patch_tone").map(|b| b.size), Some(129));
        assert_eq!(m.block("performance_common").map(|b| b.size), Some(66));
        assert_eq!(m.block("performance_part").map(|b| b.size), Some(25));
        assert_eq!(m.block("rhythm_common").map(|b| b.size), Some(12));
        assert_eq!(m.block("rhythm_note").map(|b| b.size), Some(58));
        assert_eq!(m.block("system_common").map(|b| b.size), Some(96));
        assert_eq!(m.block("scale_tune").map(|b| b.size), Some(12));
        assert_eq!(m.limits().max_dt1_data_to_device, 128);
        assert_eq!(m.limits().min_dt1_gap_ms, 20);
    }

    #[test]
    fn accepts_a_minimal_valid_map() {
        let j = doc(
            &[
                p("a", 0, 1, "seven", 0, 127),
                p("b", 1, 2, "nibble", 20, 250),
            ]
            .join(","),
            3,
        );
        assert!(ParameterMap::from_json(&j).is_ok());
    }

    #[test]
    fn rejects_structural_mistakes() {
        type Check = fn(&MapError) -> bool;
        let cases: Vec<(String, Check)> = vec![
            (
                doc(
                    &[p("a", 0, 1, "seven", 0, 1), p("a", 1, 1, "seven", 0, 1)].join(","),
                    2,
                ),
                |e| matches!(e, MapError::DuplicateParam { .. }),
            ),
            (doc(&p("a", 0, 1, "seven", 5, 1), 1), |e| {
                matches!(e, MapError::BadRange { .. })
            }),
            (doc(&p("a", 0, 1, "seven", -1, 1), 1), |e| {
                matches!(e, MapError::NegativeMin { .. })
            }),
            (doc(&p("a", 0, 0, "seven", 0, 1), 1), |e| {
                matches!(e, MapError::NoBytes { .. })
            }),
            (doc(&p("a", 0, 2, "seven", 0, 1), 2), |e| {
                matches!(e, MapError::WrongWidth { .. })
            }),
            (doc(&p("a", 1, 1, "seven", 0, 1), 1), |e| {
                matches!(e, MapError::PastEnd { .. })
            }),
            (
                doc(
                    &[p("a", 0, 2, "nibble", 0, 9), p("b", 1, 1, "seven", 0, 9)].join(","),
                    3,
                ),
                |e| matches!(e, MapError::Overlap { .. }),
            ),
            (doc(&p("a", 0, 1, "seven", 0, 200), 1), |e| {
                matches!(e, MapError::TooWide { .. })
            }),
            ("{".to_owned(), |e| matches!(e, MapError::Json(_))),
        ];
        for (json, check) in cases {
            let err = ParameterMap::from_json(&json).expect_err(&json);
            assert!(check(&err), "unexpected error {err:?} for {json}");
        }
    }

    #[test]
    fn rejects_a_future_schema() {
        let j = doc(&p("a", 0, 1, "seven", 0, 1), 1).replace(r#""schema":1"#, r#""schema":2"#);
        assert_eq!(
            ParameterMap::from_json(&j).unwrap_err(),
            MapError::Schema(2)
        );
    }

    #[test]
    fn hostile_numbers_in_a_map_are_errors_not_panics() {
        let huge_offset = doc(&p("a", usize::MAX, 2, "nibble", 0, 9), 4);
        assert!(matches!(
            ParameterMap::from_json(&huge_offset),
            Err(MapError::PastEnd { .. })
        ));
        let wide = doc(&p("a", 0, 8, "nibble", 0, 9), 8);
        assert!(matches!(
            ParameterMap::from_json(&wide),
            Err(MapError::NibbleTooWide { .. })
        ));
        let big = doc(&p("a", 0, 1, "seven", 0, 1), 1_000_000);
        assert!(matches!(
            ParameterMap::from_json(&big),
            Err(MapError::BlockTooLarge { .. })
        ));
        let no_limit = doc(&p("a", 0, 1, "seven", 0, 1), 1).replace(
            r#""max_dt1_data_to_device":128"#,
            r#""max_dt1_data_to_device":0"#,
        );
        assert!(matches!(
            ParameterMap::from_json(&no_limit),
            Err(MapError::BadLimits(_))
        ));
        let too_big = doc(&p("a", 0, 1, "seven", 0, 1), 1).replace(
            r#""max_dt1_data_to_device":128"#,
            r#""max_dt1_data_to_device":129"#,
        );
        assert!(matches!(
            ParameterMap::from_json(&too_big),
            Err(MapError::BadLimits(_))
        ));
    }
}
