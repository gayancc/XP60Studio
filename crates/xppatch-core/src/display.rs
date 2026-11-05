//! How a raw parameter value is shown to the musician ("-12 semitones", "L20", "TRI", "C4").
//!
//! The text in the map's `display` column comes from the Roland manual. This module turns it into a typed [`Display`]
//! once, so the editor never parses strings and never shows a bare number where the manual gives a meaning.
//! The footnote enumerations below are transcribed from the manual (documentation-derived, not hardware-verified).

use crate::schema::ParamDef;
use thiserror::Error;

/// Enumerations the map refers to as "see Tone footnote N".
const FOOTNOTES: &[(u32, &[&str])] = &[
    // Controller destinations. Roland's abbreviations (OFF, PCH, CUT, RES, LEV, PAN, MIX, CHO, REV, PL1, PL2, FL1, FL2, AL1, AL2, pL1, pL2, L1R, L2R) are spelled out
    // with the meanings the Owner's Manual gives them (pp.60 and 61): Pitch, Cutoff, Resonance, Tone Level, Tone Pan, Mix/EFX, Chorus and Reverb send, then the depth
    // of LFO 1 or 2 on pitch (vibrato), filter (wah), level (tremolo) and pan, and the rate of LFO 1 and 2.
    (
        3,
        &[
            "Off",
            "Pitch",
            "Cutoff",
            "Resonance",
            "Level",
            "Pan",
            "Mix/EFX",
            "Chorus",
            "Reverb",
            "Pitch LFO1",
            "Pitch LFO2",
            "Filter LFO1",
            "Filter LFO2",
            "Level LFO1",
            "Level LFO2",
            "Pan LFO1",
            "Pan LFO2",
            "LFO1 Rate",
            "LFO2 Rate",
        ],
    ),
    (4, &["TRI", "SIN", "SAW", "SQR", "TRP", "S&H", "RND", "CHS"]),
    (5, &["-100", "-50", "0", "+50", "+100"]),
    (6, &["ON-IN", "ON-OUT", "OFF-IN", "OFF-OUT"]),
    (7, &["OFF", "CLOCK", "TAP"]),
    (
        8,
        &[
            "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "20", "30", "40", "50", "60",
            "70", "80", "90", "100", "200", "300", "400", "500", "600", "700", "800", "900",
            "1000", "1100", "1200",
        ],
    ),
    (
        9,
        &[
            "-100", "-70", "-50", "-30", "-10", "0", "+10", "+20", "+30", "+40", "+50", "+70",
            "+100", "+120", "+150", "+200",
        ],
    ),
    (
        10,
        &[
            "-100", "-70", "-50", "-40", "-30", "-20", "-10", "0", "+10", "+20", "+30", "+40",
            "+50", "+70", "+100",
        ],
    ),
    (11, &["OFF", "LPF", "BPF", "HPF", "PKG"]),
    (12, &["MIX", "EFX", "DIR", "OUTPUT-2"]),
    (13, &["LOWER", "UPPER", "LOW&UP", "ALL"]),
];

const NOTE_NAMES: [&str; 12] = [
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
];

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Display {
    /// The raw number is the meaning.
    Raw,
    /// An ASCII character (patch name).
    Character,
    /// `shown = first + step * (raw - min)`, zero-padded to `pad` digits and signed when `signed`.
    Numeric {
        first: i32,
        step: i32,
        min: i32,
        pad: usize,
        signed: bool,
    },
    /// Left/right pan around a centre raw value: `L20`, `0`, `R20`.
    Pan { centre: i32 },
    /// A MIDI note number shown as a name (`C4`).
    Note,
    /// One label per raw value, starting at the parameter's minimum.
    Options { min: i32, labels: Vec<String> },
}

#[derive(Debug, Error, PartialEq, Eq)]
#[non_exhaustive]
pub enum DisplayError {
    #[error("unrecognised display text '{0}'")]
    Unrecognised(String),
    #[error("{count} option label(s) for {expected} value(s) in '{text}'")]
    OptionCount {
        text: String,
        count: usize,
        expected: usize,
    },
    #[error("numeric display '{text}' does not match raw range {min}..{max}")]
    NumericRange { text: String, min: i32, max: i32 },
    #[error("unknown footnote {0}")]
    Footnote(u32),
}

impl Display {
    /// Decide how `def` is shown, from its `display` text and raw range.
    pub fn resolve(def: &ParamDef) -> Result<Display, DisplayError> {
        let text = def.display.trim();
        let count = usize::try_from(def.max - def.min + 1).unwrap_or(0);
        if text.starts_with("same as raw") || text.starts_with("effect-specific") {
            return Ok(Display::Raw);
        }
        if text == "character" {
            return Ok(Display::Character);
        }
        if let Some(n) = text.strip_prefix("see Tone footnote ") {
            let n: u32 = n
                .trim()
                .parse()
                .map_err(|_| DisplayError::Unrecognised(text.to_owned()))?;
            let labels = FOOTNOTES
                .iter()
                .find(|(k, _)| *k == n)
                .ok_or(DisplayError::Footnote(n))?
                .1;
            return options(
                text,
                def.min,
                count,
                labels.iter().map(|s| (*s).to_owned()).collect(),
            );
        }
        if text.starts_with('L') && text.ends_with('R') && text.contains("..") {
            // "L64..63R" and "L63..63R": the centre is the raw value that sits between the two ends.
            let end = text.find("..").unwrap_or(1);
            let left: i32 = text[1..end]
                .parse()
                .map_err(|_| DisplayError::Unrecognised(text.to_owned()))?;
            return Ok(Display::Pan {
                centre: def.min + left,
            });
        }
        if matches!(text, "C-1..G9" | "C-1..Upper" | "Lower..G9") {
            return Ok(Display::Note);
        }
        if text == "1..Upper" || text == "Lower..127" {
            return Ok(Display::Raw);
        }
        if text.contains(',') {
            return options(text, def.min, count, expand_list(text));
        }
        if text.contains("..") {
            return numeric(text, def);
        }
        Err(DisplayError::Unrecognised(text.to_owned()))
    }

    /// The text for `raw`. Out-of-range values are shown as the raw number rather than guessed at.
    pub fn format(&self, raw: i32) -> String {
        match self {
            Display::Raw => raw.to_string(),
            Display::Character => u8::try_from(raw)
                .ok()
                .filter(|b| (0x20..0x7F).contains(b))
                .map_or_else(|| "?".to_owned(), |b| char::from(b).to_string()),
            Display::Numeric {
                first,
                step,
                min,
                pad,
                signed,
            } => {
                // Wide arithmetic: a hostile or buggy raw value must never overflow.
                let v = i64::from(*first) + i64::from(*step) * (i64::from(raw) - i64::from(*min));
                if *signed && v > 0 {
                    format!("+{v}")
                } else if *pad > 0 {
                    format!("{v:0pad$}")
                } else {
                    v.to_string()
                }
            }
            Display::Pan { centre } => {
                let d = i64::from(raw) - i64::from(*centre);
                match d.cmp(&0) {
                    std::cmp::Ordering::Less => format!("L{}", -d),
                    std::cmp::Ordering::Equal => "0".to_owned(),
                    std::cmp::Ordering::Greater => format!("R{d}"),
                }
            }
            Display::Note if (0..=127).contains(&raw) => note_name(raw),
            Display::Note => raw.to_string(),
            Display::Options { min, labels } => usize::try_from(raw - min)
                .ok()
                .and_then(|i| labels.get(i))
                .cloned()
                .unwrap_or_else(|| raw.to_string()),
        }
    }

    /// Short machine name of the kind, for the UI to choose a control.
    pub fn kind(&self) -> &'static str {
        match self {
            Display::Raw => "raw",
            Display::Character => "character",
            Display::Numeric { .. } => "numeric",
            Display::Pan { .. } => "pan",
            Display::Note => "note",
            Display::Options { .. } => "options",
        }
    }
}

pub fn note_name(note: i32) -> String {
    let n = note.clamp(0, 127);
    format!("{}{}", NOTE_NAMES[(n % 12) as usize], n / 12 - 1)
}

/// Split "a, b, c" and expand an embedded range such as "OFF, 1..3" into "OFF, 1, 2, 3".
fn expand_list(text: &str) -> Vec<String> {
    let mut out = Vec::new();
    for part in text.split(',').map(|p| p.trim().replace('`', "")) {
        let part = part.trim_matches(|c| c == '<' || c == '>').to_owned();
        match part.split_once("..") {
            Some((a, b)) => match (a.parse::<i32>(), b.parse::<i32>()) {
                (Ok(a), Ok(b)) if a <= b => out.extend((a..=b).map(|n| n.to_string())),
                _ => out.push(part),
            },
            None => out.push(part),
        }
    }
    out
}

fn options(
    text: &str,
    min: i32,
    expected: usize,
    labels: Vec<String>,
) -> Result<Display, DisplayError> {
    if labels.len() == expected {
        Ok(Display::Options { min, labels })
    } else {
        Err(DisplayError::OptionCount {
            text: text.to_owned(),
            count: labels.len(),
            expected,
        })
    }
}

/// "-63..+63", "001..255", "20..250", "0..-48", "-100..+150": `first` and `last` are what the musician sees at the
/// raw minimum and maximum; the step is whatever makes the two ends line up.
fn numeric(text: &str, def: &ParamDef) -> Result<Display, DisplayError> {
    let bad = || DisplayError::NumericRange {
        text: text.to_owned(),
        min: def.min,
        max: def.max,
    };
    let (a, b) = text.split_once("..").ok_or_else(bad)?;
    let first: i32 = a.trim_start_matches('+').parse().map_err(|_| bad())?;
    let last: i32 = b.trim_start_matches('+').parse().map_err(|_| bad())?;
    let span = def.max - def.min;
    if span <= 0 || (last - first) % span != 0 {
        return Err(bad());
    }
    let step = (last - first) / span;
    if step == 0 {
        return Err(bad());
    }
    let pad = if a.len() > 1 && a.starts_with('0') && !a.starts_with("0.") {
        a.len()
    } else {
        0
    };
    Ok(Display::Numeric {
        first,
        step,
        min: def.min,
        pad,
        signed: a.starts_with('-') || a.starts_with('+') || b.starts_with('+'),
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::map::ParameterMap;

    fn def(display: &str, min: i32, max: i32) -> ParamDef {
        serde_json::from_value(serde_json::json!({
            "id": "x", "name": "X", "offset": 0, "bytes": 1, "encoding": "seven",
            "min": min, "max": max, "display": display
        }))
        .unwrap()
    }

    #[test]
    fn controller_destinations_are_spelled_out_as_the_manual_describes_them() {
        // Tone footnote 3: PCH, CUT, RES, LEV, PAN, MIX, CHO, REV, PL1, PL2, FL1, FL2, AL1, AL2, pL1, pL2, L1R, L2R.
        let d = Display::resolve(&def("see Tone footnote 3", 0, 18)).unwrap();
        assert_eq!(d.format(0), "Off");
        assert_eq!(d.format(1), "Pitch");
        assert_eq!(d.format(2), "Cutoff");
        assert_eq!(d.format(5), "Pan");
        assert_eq!(d.format(9), "Pitch LFO1"); // PL1: vibrato depth
        assert_eq!(d.format(11), "Filter LFO1"); // FL1: wah depth
        assert_eq!(d.format(13), "Level LFO1"); // AL1: tremolo depth
        assert_eq!(d.format(15), "Pan LFO1"); // pL1: lower case in the manual
        assert_eq!(d.format(18), "LFO2 Rate");
        // nineteen destinations for the range 0 to 18, no more, no fewer
        assert!(Display::resolve(&def("see Tone footnote 3", 0, 17)).is_err());
    }

    #[test]
    fn signed_ranges_show_a_sign() {
        let d = Display::resolve(&def("-63..+63", 0, 126)).unwrap();
        assert_eq!(d.format(0), "-63");
        assert_eq!(d.format(63), "0");
        assert_eq!(d.format(126), "+63");
    }

    #[test]
    fn a_stepped_range_uses_its_step() {
        let d = Display::resolve(&def("-100..+150", 0, 125)).unwrap();
        assert_eq!(d.format(0), "-100");
        assert_eq!(d.format(125), "+150");
        assert_eq!(d.format(50), "0");
    }

    #[test]
    fn a_descending_range_counts_down() {
        let d = Display::resolve(&def("0..-48", 0, 48)).unwrap();
        assert_eq!(d.format(0), "0");
        assert_eq!(d.format(48), "-48");
    }

    #[test]
    fn padded_and_offset_numbers() {
        let wave = Display::resolve(&def("001..255", 0, 254)).unwrap();
        assert_eq!(wave.format(0), "001");
        assert_eq!(wave.format(254), "255");
        let efx = Display::resolve(&def("1..40", 0, 39)).unwrap();
        assert_eq!(efx.format(0), "1");
        let tempo = Display::resolve(&def("20..250", 20, 250)).unwrap();
        assert_eq!(tempo.format(120), "120");
    }

    #[test]
    fn pan_is_left_centre_right() {
        let d = Display::resolve(&def("L64..63R", 0, 127)).unwrap();
        assert_eq!(d.format(0), "L64");
        assert_eq!(d.format(64), "0");
        assert_eq!(d.format(127), "R63");
        let alt = Display::resolve(&def("L63..63R", 1, 127)).unwrap();
        assert_eq!(alt.format(1), "L63");
        assert_eq!(alt.format(127), "R63");
    }

    #[test]
    fn notes_and_options() {
        let n = Display::resolve(&def("C-1..G9", 0, 127)).unwrap();
        assert_eq!(n.format(60), "C4");
        assert_eq!(n.format(0), "C-1");
        assert_eq!(n.format(127), "G9");
        let o = Display::resolve(&def("OFF, ON", 0, 1)).unwrap();
        assert_eq!(o.format(1), "ON");
        let s = Display::resolve(&def("OFF, 1..3", 0, 3)).unwrap();
        assert_eq!(s.format(0), "OFF");
        assert_eq!(s.format(3), "3");
    }

    #[test]
    fn footnotes_and_bad_text() {
        let w = Display::resolve(&def("see Tone footnote 4", 0, 7)).unwrap();
        assert_eq!(w.format(0), "TRI");
        assert_eq!(w.format(7), "CHS");
        assert!(matches!(
            Display::resolve(&def("see Tone footnote 99", 0, 7)),
            Err(DisplayError::Footnote(99))
        ));
        assert!(matches!(
            Display::resolve(&def("OFF, ON", 0, 5)),
            Err(DisplayError::OptionCount { .. })
        ));
        assert!(matches!(
            Display::resolve(&def("gibberish", 0, 5)),
            Err(DisplayError::Unrecognised(_))
        ));
    }

    #[test]
    fn out_of_range_values_fall_back_to_the_raw_number() {
        let d = Display::resolve(&def("OFF, ON", 0, 1)).unwrap();
        assert_eq!(d.format(9), "9");
        assert_eq!(d.format(-1), "-1");
        assert_eq!(Display::Character.format(500), "?");
    }

    #[test]
    fn every_patch_parameter_has_a_display() {
        let map = ParameterMap::embedded();
        let mut failures = Vec::new();
        for block in map.blocks().iter().filter(|b| b.family == "patch") {
            for p in &block.params {
                if let Err(e) = Display::resolve(p) {
                    failures.push(format!("{}.{}: {e}", block.id, p.id));
                }
            }
        }
        assert!(failures.is_empty(), "{failures:#?}");
    }

    #[test]
    fn displays_agree_with_the_raw_range_at_both_ends() {
        // The first and last raw values must format to the first and last text of the manual's range.
        let map = ParameterMap::embedded();
        for block in map.blocks().iter().filter(|b| b.family == "patch") {
            for p in &block.params {
                let Ok(d) = Display::resolve(p) else { continue };
                let (lo, hi) = (d.format(p.min), d.format(p.max));
                assert!(!lo.is_empty() && !hi.is_empty(), "{}", p.id);
                if let Display::Options { labels, .. } = &d {
                    assert_eq!(
                        labels.first().map(String::as_str),
                        Some(lo.as_str()),
                        "{}",
                        p.id
                    );
                    assert_eq!(
                        labels.last().map(String::as_str),
                        Some(hi.as_str()),
                        "{}",
                        p.id
                    );
                }
            }
        }
    }
}

#[cfg(test)]
mod all_blocks {
    use super::*;
    use crate::map::ParameterMap;

    #[test]
    fn every_parameter_of_every_block_has_a_display() {
        let map = ParameterMap::embedded();
        let mut out = Vec::new();
        for block in map.blocks() {
            for p in &block.params {
                if let Err(e) = Display::resolve(p) {
                    out.push(format!("{}.{}: {e}", block.id, p.id));
                }
            }
        }
        assert!(out.is_empty(), "{out:#?}");
    }
}
