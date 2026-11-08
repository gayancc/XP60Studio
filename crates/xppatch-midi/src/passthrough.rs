//! Secondary MIDI controller pass-through: which channel-voice messages may be forwarded to the
//! instrument, and how their channel is remapped.
//!
//! This module is pure (no I/O). The hardware listener that opens a second input and forwards onto
//! the shared XP-60 output lives in [`crate::hardware`] behind the `hardware` feature.

/// How channel-voice messages from a secondary controller are routed onto the instrument.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PassThroughConfig {
    /// `None` keeps the controller's channel; `Some(0..=15)` rewrites every channel-voice status
    /// nibble to that audition / Part channel.
    pub target_channel: Option<u8>,
    /// Drop Active Sensing, MIDI Clock, Start/Stop/Continue and other system real-time bytes
    /// (`0xF8..=0xFF`). Default on so a chatty controller cannot flood a Bluetooth WIDI link.
    pub filter_realtime: bool,
}

impl Default for PassThroughConfig {
    fn default() -> Self {
        Self::new()
    }
}

impl PassThroughConfig {
    pub const fn new() -> Self {
        Self {
            target_channel: None,
            filter_realtime: true,
        }
    }

    /// Direct pass-through on the original channel, real-time filtered.
    pub const fn direct() -> Self {
        Self::new()
    }

    /// Remap every channel-voice message onto `channel` (0..=15).
    pub fn remapped(channel: u8) -> Option<Self> {
        if channel > 15 {
            return None;
        }
        Some(Self {
            target_channel: Some(channel),
            filter_realtime: true,
        })
    }
}

/// True for Note Off/On, Poly Pressure, CC, Program Change, Channel Pressure and Pitch Bend.
#[inline]
pub fn is_channel_voice(status: u8) -> bool {
    (0x80..0xF0).contains(&status)
}

/// System real-time (`0xF8` Active Sensing through `0xFF` Reset), including MIDI Clock.
#[inline]
pub fn is_system_realtime(status: u8) -> bool {
    status >= 0xF8
}

/// Expected MIDI message length from a status byte, for complete channel-voice and real-time messages.
pub fn expected_message_len(status: u8) -> Option<usize> {
    if is_system_realtime(status) {
        return Some(1);
    }
    match status & 0xF0 {
        0xC0 | 0xD0 => Some(2),
        0x80 | 0x90 | 0xA0 | 0xB0 | 0xE0 => Some(3),
        _ => None,
    }
}

/// Whether this complete MIDI chunk may leave the secondary controller toward the XP-60.
///
/// SysEx and system-common messages are never forwarded: they must not contend with the primary
/// instrument connection's DT1/RQ1 traffic. Incomplete or running-status fragments are refused.
pub fn should_forward(bytes: &[u8], filter_realtime: bool) -> bool {
    let Some(&status) = bytes.first() else {
        return false;
    };
    if is_system_realtime(status) {
        return !filter_realtime && bytes.len() == 1;
    }
    if !is_channel_voice(status) {
        return false;
    }
    match expected_message_len(status) {
        Some(n) => bytes.len() == n,
        None => false,
    }
}

/// Apply channel remapping when configured. Returns `None` when `target_channel` is out of range.
pub fn remap_channel(bytes: &[u8], target_channel: Option<u8>) -> Option<Vec<u8>> {
    let Some(&status) = bytes.first() else {
        return Some(Vec::new());
    };
    if !is_channel_voice(status) {
        return Some(bytes.to_vec());
    }
    match target_channel {
        None => Some(bytes.to_vec()),
        Some(ch) if ch <= 15 => {
            let mut out = bytes.to_vec();
            out[0] = (status & 0xF0) | ch;
            Some(out)
        }
        Some(_) => None,
    }
}

/// Filter and optionally remap in one step. `None` means drop the chunk.
pub fn prepare_forward(bytes: &[u8], config: &PassThroughConfig) -> Option<Vec<u8>> {
    if !should_forward(bytes, config.filter_realtime) {
        return None;
    }
    remap_channel(bytes, config.target_channel)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn notes_cc_pitch_bend_and_program_change_pass() {
        let cfg = PassThroughConfig::direct();
        assert_eq!(
            prepare_forward(&[0x90, 60, 100], &cfg).as_deref(),
            Some(&[0x90, 60, 100][..])
        );
        assert_eq!(
            prepare_forward(&[0x80, 60, 0], &cfg).as_deref(),
            Some(&[0x80, 60, 0][..])
        );
        assert_eq!(
            prepare_forward(&[0xB0, 1, 64], &cfg).as_deref(),
            Some(&[0xB0, 1, 64][..])
        );
        assert_eq!(
            prepare_forward(&[0xE0, 0x00, 0x40], &cfg).as_deref(),
            Some(&[0xE0, 0x00, 0x40][..])
        );
        assert_eq!(
            prepare_forward(&[0xC3, 12], &cfg).as_deref(),
            Some(&[0xC3, 12][..])
        );
        assert_eq!(
            prepare_forward(&[0xD2, 40], &cfg).as_deref(),
            Some(&[0xD2, 40][..])
        );
        assert_eq!(
            prepare_forward(&[0xA1, 60, 30], &cfg).as_deref(),
            Some(&[0xA1, 60, 30][..])
        );
    }

    #[test]
    fn sysex_and_system_common_are_dropped() {
        let cfg = PassThroughConfig::direct();
        assert!(prepare_forward(&[0xF0, 0x41, 0xF7], &cfg).is_none());
        assert!(prepare_forward(&[0xF1, 0x00], &cfg).is_none());
        assert!(prepare_forward(&[0xF2, 0x00, 0x00], &cfg).is_none());
        assert!(prepare_forward(&[0xF3, 0x01], &cfg).is_none());
        assert!(prepare_forward(&[0xF6], &cfg).is_none());
    }

    #[test]
    fn realtime_is_filtered_by_default_and_optional() {
        let filtered = PassThroughConfig::default();
        assert!(filtered.filter_realtime);
        assert!(prepare_forward(&[0xF8], &filtered).is_none());
        assert!(prepare_forward(&[0xFE], &filtered).is_none());
        assert!(prepare_forward(&[0xFA], &filtered).is_none());
        let open = PassThroughConfig {
            filter_realtime: false,
            ..PassThroughConfig::direct()
        };
        assert_eq!(
            prepare_forward(&[0xF8], &open).as_deref(),
            Some(&[0xF8][..])
        );
        assert_eq!(
            prepare_forward(&[0xFE], &open).as_deref(),
            Some(&[0xFE][..])
        );
    }

    #[test]
    fn remaps_channel_nibble_only() {
        let cfg = PassThroughConfig::remapped(4).unwrap();
        assert_eq!(
            prepare_forward(&[0x93, 60, 100], &cfg).as_deref(),
            Some(&[0x94, 60, 100][..])
        );
        assert_eq!(
            prepare_forward(&[0xB7, 1, 10], &cfg).as_deref(),
            Some(&[0xB4, 1, 10][..])
        );
        assert_eq!(
            prepare_forward(&[0xE0, 0, 64], &cfg).as_deref(),
            Some(&[0xE4, 0, 64][..])
        );
        assert!(PassThroughConfig::remapped(16).is_none());
    }

    #[test]
    fn truncated_and_oversized_channel_voice_are_dropped() {
        let cfg = PassThroughConfig::direct();
        assert!(prepare_forward(&[0x90, 60], &cfg).is_none());
        assert!(prepare_forward(&[0x90], &cfg).is_none());
        assert!(prepare_forward(&[0x90, 60, 100, 1], &cfg).is_none());
        assert!(prepare_forward(&[0xC0], &cfg).is_none());
        // Running-status fragment (no status byte).
        assert!(prepare_forward(&[60, 100], &cfg).is_none());
    }
}
