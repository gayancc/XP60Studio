//! The client against the virtual XP-60 (virtual clock, injected faults).

use std::time::Duration;
use xppatch_core::{Patch, SyxImage};
use xppatch_midi::testing::VirtualXp60;
use xppatch_midi::{
    CancelToken, Client, ClientConfig, ClientError, ControlChange, ControlSource, MidiEvent,
    MidiTransport,
};
use xppatch_sysex::address::Address;
use xppatch_sysex::memory::{user_patch, TEMP_PATCH};
use xppatch_sysex::message::DeviceId;

const BANK: &[u8] = include_bytes!("../../../tests/golden/xp60-user-bank.syx");

fn bank() -> Vec<(u8, Patch)> {
    SyxImage::parse(BANK).unwrap().user_patches().unwrap()
}

fn device() -> VirtualXp60 {
    let mut d = VirtualXp60::new(DeviceId::DEFAULT);
    d.load_image(&SyxImage::parse(BANK).unwrap());
    d
}

fn client(d: VirtualXp60) -> Client<VirtualXp60> {
    Client::new(d, ClientConfig::default())
}

fn named(name: &str) -> Patch {
    let mut p = bank()[0].1.clone();
    p.set_name(name).unwrap();
    p
}

fn no_progress(_: xppatch_midi::Progress) {}

#[test]
fn a_fetched_user_patch_equals_the_bank_patch() {
    let mut c = client(device());
    let got = c
        .fetch_user_patch(5, &CancelToken::new(), &mut no_progress)
        .unwrap();
    assert_eq!(got, bank()[5].1);
}

#[test]
fn requests_are_never_pipelined() {
    let mut c = client(device());
    c.fetch_bank(0..10, &CancelToken::new(), &mut no_progress)
        .unwrap();
    let stats = c.port().stats;
    assert_eq!(stats.pipelined_requests_dropped, 0);
    assert_eq!(stats.requests_served, 50);
}

#[test]
fn a_temp_write_is_paced_split_verified_and_lands_in_the_instrument() {
    let mut d = device();
    d.set_temp_patch(&bank()[0].1).unwrap();
    let mut c = client(d);
    let patch = named("Written");
    let report = c
        .send_temp_patch(&patch, true, &CancelToken::new())
        .unwrap();
    assert!(report.verified);
    // 73 + 4 x (128 + 1) bytes: the 129-byte tones split into 128 + 1.
    assert_eq!(report.messages, 1 + 4 * 2);
    let d = c.into_port();
    assert_eq!(d.stats.fast_writes_dropped, 0);
    assert_eq!(d.stats.oversize_writes_dropped, 0);
    assert_eq!(d.temp_patch().unwrap(), patch);
    // Gaps between consecutive DT1 messages are at least 20 ms.
    let times: Vec<Duration> = d
        .sent
        .iter()
        .filter(|(_, b)| b.get(4) == Some(&0x12))
        .map(|(t, _)| *t)
        .collect();
    assert!(times
        .windows(2)
        .all(|w| w[1] - w[0] >= Duration::from_millis(20)));
}

#[test]
fn a_dropped_request_is_retried() {
    let mut d = device();
    d.faults.drop_next_requests = 2;
    let mut c = client(d);
    let got = c
        .fetch_user_patch(0, &CancelToken::new(), &mut no_progress)
        .unwrap();
    assert_eq!(got, bank()[0].1);
}

#[test]
fn a_silent_instrument_times_out_naming_the_missing_block() {
    let mut d = device();
    d.faults.drop_next_requests = 100;
    let mut c = client(d);
    let err = c
        .fetch_user_patch(0, &CancelToken::new(), &mut no_progress)
        .unwrap_err();
    match err {
        ClientError::Timeout { attempts, missing } => {
            assert_eq!(attempts, 3);
            assert_eq!(missing, vec![user_patch(0).unwrap()]);
        }
        other => panic!("expected timeout, got {other:?}"),
    }
}

#[test]
fn corrupt_checksums_and_noise_are_ignored() {
    let mut d = device();
    d.faults.corrupt_next_replies = 1;
    d.faults.noise_before_each_reply = true;
    let mut c = client(d);
    let got = c
        .fetch_user_patch(3, &CancelToken::new(), &mut no_progress)
        .unwrap();
    assert_eq!(got, bank()[3].1);
    assert!(c.ignored_frames() >= 1);
}

#[test]
fn a_wrong_device_id_is_found_by_the_scan() {
    let mut d = device();
    d.set_device(DeviceId::new(0x13).unwrap());
    let mut c = client(d);
    c.port_mut()
        .set_block(TEMP_PATCH, bank()[0].1.common.bytes().to_vec());
    let info = c
        .scan_device_ids(&CancelToken::new(), &mut no_progress)
        .unwrap()
        .expect("found");
    assert_eq!(info.device, DeviceId::new(0x13).unwrap());
    assert_eq!(c.config().device, DeviceId::new(0x13).unwrap());
}

#[test]
fn a_scan_with_no_answer_restores_the_previous_id() {
    let mut c = client(VirtualXp60::new(DeviceId::DEFAULT));
    let found = c
        .scan_device_ids(&CancelToken::new(), &mut no_progress)
        .unwrap();
    assert!(found.is_none());
    assert_eq!(c.config().device, DeviceId::DEFAULT);
}

#[test]
fn a_sparse_span_returns_only_populated_blocks() {
    let mut c = client(device());
    let base = user_patch(0).unwrap();
    let size = Address::from_compact(0x1800).unwrap();
    let expect: Vec<_> = bank()[0]
        .1
        .to_dt1(base)
        .unwrap()
        .iter()
        .map(|(a, d)| xppatch_midi::Expect {
            address: *a,
            len: d.len(),
        })
        .collect();
    let pieces = c
        .fetch_span(base, size, &expect, &CancelToken::new())
        .unwrap();
    assert_eq!(pieces.len(), 5);
}

#[test]
fn cancelling_a_bank_read_keeps_what_was_read() {
    let mut c = client(device());
    let cancel = CancelToken::new();
    let trigger = cancel.clone();
    let out = c
        .fetch_bank(0..128, &cancel, &mut |p| {
            if p.done == 3 {
                trigger.cancel();
            }
        })
        .unwrap();
    assert!(out.cancelled);
    assert_eq!(out.patches.len(), 3);
}

#[test]
fn a_whole_bank_reads_back_equal() {
    let mut c = client(device());
    let out = c
        .fetch_bank(0..128, &CancelToken::new(), &mut no_progress)
        .unwrap();
    assert!(out.failures.is_empty());
    assert_eq!(out.patches, bank());
}

#[test]
fn a_bank_read_continues_past_a_missing_slot() {
    let mut d = VirtualXp60::new(DeviceId::DEFAULT);
    d.set_user_patch(0, &bank()[0].1).unwrap();
    d.set_user_patch(2, &bank()[2].1).unwrap();
    let mut c = client(d);
    let out = c
        .fetch_bank(0..3, &CancelToken::new(), &mut no_progress)
        .unwrap();
    assert_eq!(out.patches.len(), 2);
    assert_eq!(out.failures.len(), 1);
    assert_eq!(out.failures[0].0, 1);
}

fn blocks_of(docs: &[(u8, xppatch_core::Document)]) -> Vec<(u8, Vec<Vec<u8>>)> {
    docs.iter()
        .map(|(slot, d)| {
            (
                *slot,
                d.blocks().iter().map(|b| b.bytes().to_vec()).collect(),
            )
        })
        .collect()
}

#[test]
fn every_user_performance_and_rhythm_set_reads_back_equal() {
    use xppatch_core::DocKind;
    let image = SyxImage::parse(BANK).unwrap();
    let mut c = client(device());
    for kind in [DocKind::Performance, DocKind::Rhythm] {
        let got = c
            .fetch_user_documents(kind, &CancelToken::new(), &mut no_progress)
            .unwrap();
        let want = image.user_documents(kind).unwrap();
        assert_eq!(got.len(), usize::from(kind.user_slots()));
        assert_eq!(blocks_of(&got), blocks_of(&want), "{kind:?}");
    }
}

#[test]
fn user_document_reads_report_progress_and_there_are_none_for_system() {
    use xppatch_core::DocKind;
    let mut c = client(device());
    let mut seen = Vec::new();
    c.fetch_user_documents(DocKind::Rhythm, &CancelToken::new(), &mut |p| {
        seen.push((p.what, p.done, p.total))
    })
    .unwrap();
    assert_eq!(seen.first(), Some(&("user rhythm sets", 0, 2)));
    assert_eq!(seen.last(), Some(&("user rhythm sets", 2, 2)));
    assert!(c
        .fetch_user_documents(DocKind::System, &CancelToken::new(), &mut no_progress)
        .unwrap()
        .is_empty());
}

#[test]
fn cancelling_a_user_document_read_is_an_error_not_a_short_backup() {
    use xppatch_core::DocKind;
    let mut c = client(device());
    let cancel = CancelToken::new();
    let trigger = cancel.clone();
    let err = c
        .fetch_user_documents(DocKind::Performance, &cancel, &mut |p| {
            if p.done == 3 {
                trigger.cancel();
            }
        })
        .unwrap_err();
    assert!(matches!(err, ClientError::Cancelled), "{err:?}");
}

#[test]
fn an_unreadable_user_performance_ends_the_read_instead_of_leaving_a_hole() {
    use xppatch_core::{DocKind, Document};
    use xppatch_sysex::memory::user_performance;
    let mut d = VirtualXp60::new(DeviceId::DEFAULT);
    d.set_document(
        user_performance(0).unwrap(),
        &Document::init(DocKind::Performance),
    )
    .unwrap();
    let mut c = client(d);
    let err = c
        .fetch_user_documents(DocKind::Performance, &CancelToken::new(), &mut no_progress)
        .unwrap_err();
    assert!(matches!(err, ClientError::Timeout { .. }), "{err:?}");
}

#[test]
fn ignored_writes_are_reported_as_a_mismatch() {
    let mut d = device();
    d.set_temp_patch(&bank()[0].1).unwrap();
    d.faults.ignore_writes = true;
    let mut c = client(d);
    let err = c
        .send_temp_patch(&named("Nope"), true, &CancelToken::new())
        .unwrap_err();
    assert!(matches!(err, ClientError::Mismatch { .. }), "{err:?}");
}

#[test]
fn a_panel_selection_is_reported_as_an_event() {
    let mut d = device();
    d.panel_select(0, 81, 0, 7);
    let mut c = client(d);
    c.poll(Duration::from_millis(100)).unwrap();
    let events = c.take_events();
    assert!(events.contains(&MidiEvent::ProgramChange {
        channel: 0,
        program: 7
    }));
    assert!(c.take_events().is_empty());
}

#[test]
fn controller_moves_are_collected_once_each_with_the_latest_value_and_are_not_a_panel_change() {
    let mut d = device();
    d.control_change(0, 1, 10);
    d.control_change(0, 1, 90);
    d.control_change(0, 7, 20);
    d.control_change(3, 1, 55);
    let mut c = client(d);
    c.poll(Duration::from_millis(100)).unwrap();
    assert_eq!(
        c.take_controls(),
        vec![
            ControlChange {
                channel: 0,
                source: ControlSource::Cc(1),
                value: 90
            },
            ControlChange {
                channel: 0,
                source: ControlSource::Cc(7),
                value: 20
            },
            ControlChange {
                channel: 3,
                source: ControlSource::Cc(1),
                value: 55
            },
        ]
    );
    assert!(c.take_controls().is_empty());
    // A controller is not a Program/Bank change: the cached patch is not stale because of it.
    assert!(c.take_events().is_empty());
}

#[test]
fn an_nrpn_arrives_as_one_14_bit_value_and_its_selector_bytes_are_not_reported_as_controllers() {
    let mut d = device();
    d.control_change(2, 99, 1);
    d.control_change(2, 98, 5);
    d.control_change(2, 6, 0x20);
    d.control_change(2, 38, 0x05);
    let mut c = client(d);
    c.poll(Duration::from_millis(100)).unwrap();
    assert_eq!(
        c.take_controls(),
        vec![ControlChange {
            channel: 2,
            source: ControlSource::Nrpn((1 << 7) | 5),
            value: (0x20 << 7) | 0x05
        }]
    );
}

#[test]
fn a_controller_flood_cannot_grow_the_queue_and_does_not_hide_a_panel_change() {
    let mut d = device();
    for i in 0..5_000u32 {
        d.control_change((i % 16) as u8, (i % 120) as u8 + 1, (i % 128) as u8);
    }
    d.panel_select(0, 81, 0, 9);
    let mut c = client(d);
    c.poll(Duration::from_secs(1)).unwrap();
    assert!(c.take_controls().len() <= 512);
    assert!(c.take_events().contains(&MidiEvent::ProgramChange {
        channel: 0,
        program: 9
    }));
}

#[test]
fn stopping_the_listener_forgets_what_was_collected() {
    let mut d = device();
    d.control_change(0, 1, 10);
    let mut c = client(d);
    c.poll(Duration::from_millis(50)).unwrap();
    c.listen_controllers(false);
    assert!(c.take_controls().is_empty());
}

#[test]
fn a_probe_reads_the_patch_name_once() {
    let mut d = device();
    d.set_temp_patch(&named("Probe Me")).unwrap();
    let mut c = client(d);
    let info = c.probe(&CancelToken::new()).unwrap();
    assert_eq!(info.patch_name, "Probe Me");
    assert_eq!(c.port().stats.requests_served, 1);
}

#[test]
fn a_cancelled_token_stops_a_fetch_before_sending() {
    let mut c = client(device());
    let cancel = CancelToken::new();
    cancel.cancel();
    let err = c
        .fetch_user_patch(0, &cancel, &mut no_progress)
        .unwrap_err();
    assert_eq!(err, ClientError::Cancelled);
    assert!(c.port().sent.is_empty());
    assert_eq!(c.port().now(), Duration::ZERO);
}

#[test]
fn a_bank_read_with_nothing_connected_gives_up_quickly() {
    let mut c = client(VirtualXp60::new(DeviceId::DEFAULT));
    let out = c
        .fetch_bank(0..128, &CancelToken::new(), &mut no_progress)
        .unwrap();
    assert!(out.aborted);
    assert_eq!(out.failures.len(), 3);
}

#[test]
fn nonsense_expectations_are_refused_before_anything_is_sent() {
    let mut c = client(device());
    let base = user_patch(0).unwrap();
    let size = Address::from_compact(16).unwrap();
    for len in [0usize, 17, 5000] {
        let e = [xppatch_midi::Expect { address: base, len }];
        let err = c
            .fetch_span(base, size, &e, &CancelToken::new())
            .unwrap_err();
        assert!(matches!(err, ClientError::Refused(_)), "{len}: {err:?}");
    }
    assert!(c.port().sent.is_empty());
}

#[test]
fn a_panel_change_during_a_read_is_retried_and_the_result_is_consistent() {
    let mut d = device();
    d.panel_select(0, 81, 0, 1);
    let mut c = client(d);
    let got = c
        .fetch_user_patch(2, &CancelToken::new(), &mut no_progress)
        .unwrap();
    assert_eq!(got, bank()[2].1);
}

fn dt1_frame(device: DeviceId, address: Address, data: Vec<u8>) -> Vec<u8> {
    xppatch_sysex::message::Message::Dt1 { address, data }
        .encode(device)
        .unwrap()
}

#[test]
fn a_live_edit_frame_to_the_temporary_patch_is_sent_and_lands() {
    let mut d = device();
    d.set_temp_patch(&bank()[0].1).unwrap();
    let mut c = client(d);
    let tone2_level = Address::from_lanes([0x03, 0x00, 0x12, 0x65]);
    let frame = dt1_frame(DeviceId::DEFAULT, tone2_level, vec![77]);
    c.send_temp_frame(&frame, &CancelToken::new()).unwrap();
    let d = c.into_port();
    assert_eq!(d.stats.writes_applied, 1);
    assert_eq!(
        d.temp_patch().unwrap().tones[1].get("tone_level").unwrap(),
        77
    );
}

#[test]
fn live_edit_frames_are_paced() {
    let mut d = device();
    d.set_temp_patch(&bank()[0].1).unwrap();
    let mut c = client(d);
    for v in 0..5u8 {
        let frame = dt1_frame(
            DeviceId::DEFAULT,
            Address::from_lanes([0x03, 0x00, 0x10, 0x65]),
            vec![v],
        );
        c.send_temp_frame(&frame, &CancelToken::new()).unwrap();
    }
    let d = c.into_port();
    assert_eq!(d.stats.fast_writes_dropped, 0);
    assert_eq!(d.stats.writes_applied, 5);
}

#[test]
fn live_edit_frames_that_could_reach_permanent_memory_are_refused() {
    let mut c = client(device());
    let cancel = CancelToken::new();
    // A User patch, the System area, a frame spanning from temporary into User memory, a read request,
    // another device's frame, and garbage: none may be sent.
    let user = dt1_frame(DeviceId::DEFAULT, user_patch(0).unwrap(), vec![1]);
    let system = dt1_frame(
        DeviceId::DEFAULT,
        Address::from_lanes([0, 0, 0, 0]),
        vec![1],
    );
    let rq1 = xppatch_sysex::message::Message::Rq1 {
        address: TEMP_PATCH,
        size: Address::from_compact(4).unwrap(),
    }
    .encode(DeviceId::DEFAULT)
    .unwrap();
    let other = dt1_frame(DeviceId::new(0x13).unwrap(), TEMP_PATCH, vec![1]);
    let big = dt1_frame(DeviceId::DEFAULT, TEMP_PATCH, vec![0; 129]);
    for (what, frame) in [
        ("user", user),
        ("system", system),
        ("rq1", rq1),
        ("other device", other),
        ("oversize", big),
        ("garbage", vec![1, 2, 3]),
    ] {
        let err = c.send_temp_frame(&frame, &cancel).unwrap_err();
        assert!(matches!(err, ClientError::Refused(_)), "{what}: {err:?}");
    }
    assert!(c.port().sent.is_empty());
}

#[test]
fn live_edits_are_held_to_the_blocks_of_the_temporary_patch() {
    let mut c = client(device());
    let cancel = CancelToken::new();
    // The gap after Common (offset 0x49), Temporary Performance, Temporary Part 1, a write running off the end of
    // Tone 1, and the address just past the last tone: all temporary memory, none inside a known block.
    for (what, addr, len) in [
        (
            "gap after common",
            Address::from_lanes([3, 0, 0, 0x49]),
            1usize,
        ),
        (
            "gap after performance common",
            Address::from_lanes([1, 0, 0, 66]),
            1,
        ),
        ("temp part", Address::from_lanes([2, 0, 0, 0]), 1),
        ("past tone 1", Address::from_lanes([3, 0, 0x10, 0x7F]), 3),
        ("after tone 4", Address::from_lanes([3, 0, 0x18, 0]), 1),
    ] {
        let frame = dt1_frame(DeviceId::DEFAULT, addr, vec![0; len]);
        let err = c.send_temp_frame(&frame, &cancel).unwrap_err();
        assert!(matches!(err, ClientError::Refused(_)), "{what}: {err:?}");
    }
    assert!(c.port().sent.is_empty());
}

#[test]
fn live_edits_may_target_blocks_of_the_temporary_performance_and_rhythm_setup() {
    let mut d = device();
    // Give the virtual instrument blocks at those places so the writes have somewhere to land.
    d.set_block(Address::from_lanes([1, 0, 0x10, 0]), vec![0; 25]);
    d.set_block(Address::from_lanes([2, 9, 0x2C, 0]), vec![0; 58]);
    let mut c = client(d);
    let cancel = CancelToken::new();
    let part1_level = dt1_frame(
        DeviceId::DEFAULT,
        Address::from_lanes([1, 0, 0x10, 6]),
        vec![99],
    );
    let note44_level = dt1_frame(
        DeviceId::DEFAULT,
        Address::from_lanes([2, 9, 0x2C, 41]),
        vec![90],
    );
    c.send_temp_frame(&part1_level, &cancel).unwrap();
    c.send_temp_frame(&note44_level, &cancel).unwrap();
    // Between blocks of the performance and past the last rhythm note: still refused.
    for addr in [
        Address::from_lanes([1, 0, 0x10, 25]),
        Address::from_lanes([1, 0, 0x20, 0]),
        Address::from_lanes([2, 9, 0x63, 0]),
        Address::from_lanes([2, 9, 0x00, 12]),
    ] {
        let frame = dt1_frame(DeviceId::DEFAULT, addr, vec![0]);
        assert!(
            matches!(
                c.send_temp_frame(&frame, &cancel),
                Err(ClientError::Refused(_))
            ),
            "{addr}"
        );
    }
}

#[test]
fn a_performance_and_a_rhythm_set_are_fetched_block_by_block() {
    use xppatch_core::{DocKind, Document};
    let mut perf = Document::init(DocKind::Performance);
    perf.set_name("Perf Test").unwrap();
    perf.block_mut(2).unwrap().set("part_level", 90).unwrap();
    let mut rhythm = Document::init(DocKind::Rhythm);
    rhythm.set_name("Rhythm Test").unwrap();
    rhythm.block_mut(7).unwrap().set("tone_level", 55).unwrap();
    let mut d = device();
    d.set_document(DocKind::Performance.temp_base(), &perf)
        .unwrap();
    d.set_document(DocKind::Rhythm.temp_base(), &rhythm)
        .unwrap();
    let mut c = client(d);
    let cancel = CancelToken::new();
    let got = c
        .fetch_temp_document(DocKind::Performance, &cancel, &mut no_progress)
        .unwrap();
    assert_eq!(got, perf);
    let got = c
        .fetch_temp_document(DocKind::Rhythm, &cancel, &mut no_progress)
        .unwrap();
    assert_eq!(got, rhythm);
    assert_eq!(c.port().stats.pipelined_requests_dropped, 0);
}

#[test]
fn the_rhythm_setup_common_block_and_system_are_not_live_editable() {
    let mut c = client(device());
    let cancel = CancelToken::new();
    for (what, addr) in [
        ("rhythm common", Address::from_lanes([2, 9, 0, 0])),
        ("system", Address::from_lanes([0, 0, 0x10, 0])),
        ("system common", Address::from_lanes([0, 0, 0, 6])),
    ] {
        let frame = dt1_frame(DeviceId::DEFAULT, addr, vec![0]);
        assert!(
            matches!(
                c.send_temp_frame(&frame, &cancel),
                Err(ClientError::Refused(_))
            ),
            "{what}"
        );
    }
    assert!(c.port().sent.is_empty());
}

#[test]
fn a_user_write_is_planned_backed_up_written_and_read_back() {
    let mut c = client(device());
    let cancel = CancelToken::new();
    let plan = c.plan_user_write(7, &cancel).unwrap();
    assert_eq!(plan.slot(), 7);
    assert_eq!(plan.backup(), &bank()[7].1);
    assert_eq!(plan.occupant_name(), bank()[7].1.name());
    // Planning only reads: every message sent so far is an RQ1 (command byte 0x11).
    assert!(c.port().sent.iter().all(|(_, f)| f.get(4) == Some(&0x11)));

    let new = named("Replacement");
    let report = c.commit_user_write(&plan, &new, &cancel).unwrap();
    assert!(report.verified);
    assert_eq!(report.messages, 9);
    let d = c.into_port();
    assert_eq!(d.stats.fast_writes_dropped, 0);
    assert_eq!(d.stats.oversize_writes_dropped, 0);
    assert_eq!(d.patch_at(user_patch(7).unwrap()).unwrap(), new);
    assert_eq!(
        d.patch_at(user_patch(6).unwrap()).unwrap(),
        bank()[6].1,
        "neighbours untouched"
    );
    assert_eq!(d.patch_at(user_patch(8).unwrap()).unwrap(), bank()[8].1);
}

#[test]
fn a_user_write_that_the_instrument_ignores_is_reported_and_can_be_undone() {
    let mut d = device();
    d.faults.ignore_writes = true; // what User Memory Protect looks like on the wire
    let mut c = client(d);
    let cancel = CancelToken::new();
    let plan = c.plan_user_write(2, &cancel).unwrap();
    let err = c
        .commit_user_write(&plan, &named("Nope"), &cancel)
        .unwrap_err();
    assert!(matches!(err, ClientError::Mismatch { .. }), "{err:?}");
    c.port_mut().faults.ignore_writes = false;
    let restored = c.restore_user_backup(&plan, &cancel).unwrap();
    assert!(restored.verified);
    assert_eq!(
        c.into_port().patch_at(user_patch(2).unwrap()).unwrap(),
        bank()[2].1
    );
}

#[test]
fn a_user_write_needs_a_valid_slot_and_a_valid_patch_and_sends_nothing_otherwise() {
    let mut c = client(device());
    let cancel = CancelToken::new();
    assert!(c.plan_user_write(128, &cancel).is_err());
    let plan = c.plan_user_write(0, &cancel).unwrap();
    let sent_before = c.port().sent.len();
    let mut bad = bank()[0].1.clone();
    bad.tones[0] = xppatch_core::BlockBuf::from_bytes(bad.tones[0].def(), &{
        let mut raw = bad.tones[0].bytes().to_vec();
        raw[0] = 9;
        raw
    })
    .unwrap();
    assert!(matches!(
        c.commit_user_write(&plan, &bad, &cancel),
        Err(ClientError::Patch(_))
    ));
    assert_eq!(c.port().sent.len(), sent_before);
}

#[test]
fn cancelling_before_a_user_write_leaves_the_slot_alone() {
    let mut c = client(device());
    let plan = c.plan_user_write(4, &CancelToken::new()).unwrap();
    let cancel = CancelToken::new();
    cancel.cancel();
    let sent_before = c.port().sent.len();
    assert_eq!(
        c.commit_user_write(&plan, &named("X"), &cancel)
            .unwrap_err(),
        ClientError::Cancelled
    );
    assert_eq!(c.port().sent.len(), sent_before);
}

#[test]
fn an_echo_of_our_own_writes_cannot_make_an_ignored_write_look_verified() {
    let mut d = device();
    d.faults.ignore_writes = true;
    d.faults.echo_writes = true;
    let mut c = client(d);
    let cancel = CancelToken::new();
    let plan = c.plan_user_write(6, &cancel).unwrap();
    let err = c
        .commit_user_write(&plan, &named("Echoed"), &cancel)
        .unwrap_err();
    assert!(matches!(err, ClientError::Mismatch { .. }), "{err:?}");
}

#[test]
fn a_slot_that_changed_after_planning_is_not_overwritten() {
    let mut c = client(device());
    let cancel = CancelToken::new();
    let plan = c.plan_user_write(3, &cancel).unwrap();
    // Someone changes the slot from the panel after the backup was taken.
    c.port_mut()
        .set_user_patch(3, &named("Panel Edit"))
        .unwrap();
    let err = c
        .commit_user_write(&plan, &named("Mine"), &cancel)
        .unwrap_err();
    assert!(matches!(err, ClientError::Refused(_)), "{err:?}");
    let d = c.into_port();
    assert_eq!(
        d.patch_at(user_patch(3).unwrap()).unwrap(),
        named("Panel Edit")
    );
    assert_eq!(d.stats.writes_applied, 0);
}

#[test]
fn a_whole_bank_is_planned_and_a_subset_written_with_every_safeguard() {
    let mut c = client(device());
    let cancel = CancelToken::new();
    let plans = c.plan_user_bank(&cancel, &mut no_progress).unwrap();
    assert_eq!(plans.len(), 128);
    assert_eq!(plans[5].slot(), 5);
    assert_eq!(plans[5].backup(), &bank()[5].1);

    let writes = vec![(5u8, named("Five")), (6u8, named("Six"))];
    let report = c
        .commit_user_bank(&plans, &writes, &cancel, &mut no_progress)
        .unwrap();
    assert_eq!(report.written, vec![5, 6]);
    assert!(report.stopped_at.is_none() && !report.cancelled);
    let d = c.into_port();
    assert_eq!(d.patch_at(user_patch(5).unwrap()).unwrap(), named("Five"));
    assert_eq!(d.patch_at(user_patch(6).unwrap()).unwrap(), named("Six"));
    assert_eq!(d.patch_at(user_patch(7).unwrap()).unwrap(), bank()[7].1);
}

#[test]
fn a_bank_write_stops_at_the_first_failure_and_names_the_slot() {
    let mut d = device();
    d.faults.ignore_writes = true;
    let mut c = client(d);
    let cancel = CancelToken::new();
    let plans = c.plan_user_bank(&cancel, &mut no_progress).unwrap();
    let writes: Vec<(u8, Patch)> = (0..4).map(|s| (s, named("X"))).collect();
    let report = c
        .commit_user_bank(&plans, &writes, &cancel, &mut no_progress)
        .unwrap();
    assert!(report.written.is_empty());
    let (slot, why) = report.stopped_at.unwrap();
    assert_eq!(slot, 0);
    assert!(why.contains("did not match what was sent"), "{why}");
}

#[test]
fn a_bank_is_not_planned_when_part_of_it_cannot_be_read() {
    let mut d = VirtualXp60::new(DeviceId::DEFAULT);
    for slot in 0..127u8 {
        d.set_user_patch(slot, &bank()[usize::from(slot)].1)
            .unwrap();
    }
    let mut c = client(d); // slot 127 is silent
    let err = c
        .plan_user_bank(&CancelToken::new(), &mut no_progress)
        .unwrap_err();
    assert!(
        matches!(&err, ClientError::Refused(m) if m.contains("128")),
        "{err:?}"
    );
}

#[test]
fn cancelling_a_bank_write_keeps_what_was_written_and_stops() {
    let mut c = client(device());
    let plans = c
        .plan_user_bank(&CancelToken::new(), &mut no_progress)
        .unwrap();
    let cancel = CancelToken::new();
    let trigger = cancel.clone();
    let writes: Vec<(u8, Patch)> = (0..10).map(|s| (s, named("Y"))).collect();
    let report = c
        .commit_user_bank(&plans, &writes, &cancel, &mut |p| {
            if p.done == 3 {
                trigger.cancel();
            }
        })
        .unwrap();
    assert!(report.cancelled);
    assert_eq!(report.written, vec![0, 1, 2]);
}

#[test]
fn a_bank_write_for_a_slot_without_a_backup_or_with_a_repeated_slot_is_refused_before_any_write() {
    let mut c = client(device());
    let cancel = CancelToken::new();
    let plans = c.plan_user_bank(&cancel, &mut no_progress).unwrap();
    let sent_before = c.port().sent.len();
    let few = &plans[..10];
    let err = c
        .commit_user_bank(few, &[(50u8, named("Z"))], &cancel, &mut no_progress)
        .unwrap_err();
    assert!(matches!(err, ClientError::Refused(_)), "{err:?}");
    let err = c
        .commit_user_bank(
            &plans,
            &[(3u8, named("A")), (3u8, named("B"))],
            &cancel,
            &mut no_progress,
        )
        .unwrap_err();
    assert!(matches!(err, ClientError::Refused(m) if m.contains("twice")));
    assert_eq!(c.port().sent.len(), sent_before, "nothing was sent");
}

#[test]
fn selecting_a_sound_sends_bank_select_msb_lsb_then_program_change() {
    let mut c = client(device());
    // PR-B patch 10 on channel 1 (index 0): the manual's table gives 81 / 1 and program 10 (wire value 9).
    c.select_sound(0, 81, 1, 9).unwrap();
    let sent: Vec<Vec<u8>> = c.port().sent.iter().map(|(_, b)| b.clone()).collect();
    assert_eq!(
        sent,
        vec![vec![0xB0, 0, 81], vec![0xB0, 32, 1], vec![0xC0, 9]]
    );
    assert!(c.select_sound(16, 80, 0, 0).is_err());
    assert!(c.select_sound(0, 128, 0, 0).is_err());
    assert!(c.select_sound(0, 80, 128, 0).is_err());
    assert!(c.select_sound(0, 80, 0, 128).is_err());
    assert_eq!(c.port().sent.len(), 3, "a refused request sends nothing");
}

#[test]
fn notes_are_sent_as_note_on_and_off_and_bad_values_are_refused() {
    let mut c = client(device());
    c.send_note(2, 60, 100, true).unwrap();
    c.send_note(2, 60, 0, false).unwrap();
    c.send_note(0, 36, 0, true).unwrap(); // a zero velocity must not become a note-off
    c.all_notes_off(2).unwrap();
    let sent: Vec<Vec<u8>> = c.port().sent.iter().map(|(_, b)| b.clone()).collect();
    assert_eq!(
        sent,
        vec![
            vec![0x92, 60, 100],
            vec![0x82, 60, 0],
            vec![0x90, 36, 1],
            vec![0xB2, 123, 0]
        ]
    );
    assert!(c.send_note(16, 60, 100, true).is_err());
    assert!(c.send_note(0, 128, 100, true).is_err());
    assert!(c.send_note(0, 60, 200, true).is_err());
    assert!(c.all_notes_off(16).is_err());
    assert_eq!(c.port().sent.len(), 4);
}

#[test]
fn the_write_gap_can_be_lengthened_but_never_below_the_documented_minimum() {
    let mut c = client(device());
    assert_eq!(
        c.set_min_dt1_gap(Duration::from_millis(5)),
        Duration::from_millis(20)
    );
    assert_eq!(
        c.set_min_dt1_gap(Duration::from_millis(80)),
        Duration::from_millis(80)
    );
    assert_eq!(
        c.set_min_dt1_gap(Duration::from_secs(60)),
        Duration::from_millis(250)
    );
    c.set_min_dt1_gap(Duration::from_millis(80));
    let cancel = CancelToken::new();
    for v in 0..4u8 {
        let f = dt1_frame(
            DeviceId::DEFAULT,
            Address::from_lanes([0x03, 0x00, 0x10, 0x65]),
            vec![v],
        );
        c.send_temp_frame(&f, &cancel).unwrap();
    }
    let d = c.into_port();
    let times: Vec<Duration> = d.sent.iter().map(|(t, _)| *t).collect();
    assert!(times
        .windows(2)
        .all(|w| w[1] - w[0] >= Duration::from_millis(80)));
}

#[test]
fn a_cancel_inside_a_slot_is_ignored_until_that_slot_is_finished_and_verified() {
    let mut c = client(device());
    let plans = c
        .plan_user_bank(&CancelToken::new(), &mut no_progress)
        .unwrap();
    let cancel = CancelToken::new();
    let trigger = cancel.clone();
    let writes: Vec<(u8, Patch)> = (0..6).map(|s| (s, named("Z"))).collect();
    // Cancel arrives while slot 1 is being worked on (progress says 1 slots are done).
    let report = c
        .commit_user_bank(&plans, &writes, &cancel, &mut |p| {
            if p.done == 1 {
                trigger.cancel();
            }
        })
        .unwrap();
    assert!(report.cancelled);
    assert_eq!(report.written, vec![0]);
    let d = c.into_port();
    // Slot 1 was not started, and slot 0 is whole.
    assert_eq!(d.patch_at(user_patch(1).unwrap()).unwrap(), bank()[1].1);
    assert_eq!(d.patch_at(user_patch(0).unwrap()).unwrap(), named("Z"));
}

// ---- writing the System settings ----

fn system_doc(tune: i32) -> xppatch_core::Document {
    let mut d = xppatch_core::Document::init(xppatch_core::DocKind::System);
    d.block_mut(0).unwrap().set("master_tune", tune).unwrap();
    d
}

fn device_with_system(doc: &xppatch_core::Document) -> VirtualXp60 {
    let mut d = device();
    d.set_document(xppatch_sysex::memory::SYSTEM, doc).unwrap();
    d
}

#[test]
fn the_system_settings_are_planned_written_and_read_back() {
    let old = system_doc(10);
    let mut c = client(device_with_system(&old));
    let cancel = CancelToken::new();
    let plan = c.plan_system_write(&cancel).unwrap();
    assert_eq!(plan.backup(), &old);
    // Planning only reads: every message sent so far is an RQ1 (command byte 0x11).
    assert!(c.port().sent.iter().all(|(_, f)| f.get(4) == Some(&0x11)));

    let new = system_doc(90);
    let report = c.commit_system_write(&plan, &new, &cancel).unwrap();
    assert!(report.verified);
    let d = c.into_port();
    assert_eq!(d.stats.fast_writes_dropped, 0);
    assert_eq!(d.stats.oversize_writes_dropped, 0);
    let mut again = client(d);
    assert_eq!(again.read_system(&cancel).unwrap(), new);
    // nothing outside the System region was touched
    assert_eq!(
        again.into_port().patch_at(user_patch(3).unwrap()).unwrap(),
        bank()[3].1
    );
}

#[test]
fn system_settings_the_instrument_ignores_are_reported_and_can_be_put_back() {
    let old = system_doc(20);
    let mut d = device_with_system(&old);
    d.faults.ignore_writes = true; // what User Memory Protect looks like on the wire
    let mut c = client(d);
    let cancel = CancelToken::new();
    let plan = c.plan_system_write(&cancel).unwrap();
    let err = c
        .commit_system_write(&plan, &system_doc(99), &cancel)
        .unwrap_err();
    assert!(matches!(err, ClientError::Mismatch { .. }), "{err:?}");
    c.port_mut().faults.ignore_writes = false;
    let restored = c.restore_system_backup(&plan, &cancel).unwrap();
    assert!(restored.verified);
    assert_eq!(c.read_system(&cancel).unwrap(), old);
}

#[test]
fn a_system_write_is_refused_when_the_settings_changed_since_they_were_backed_up() {
    let mut c = client(device_with_system(&system_doc(30)));
    let cancel = CancelToken::new();
    let plan = c.plan_system_write(&cancel).unwrap();
    // someone turns a knob on the instrument between arming and writing
    c.port_mut()
        .set_document(xppatch_sysex::memory::SYSTEM, &system_doc(31))
        .unwrap();
    let before = c.port().stats.writes_applied;
    let err = c
        .commit_system_write(&plan, &system_doc(50), &cancel)
        .unwrap_err();
    assert!(matches!(err, ClientError::Refused(_)), "{err:?}");
    assert_eq!(c.port().stats.writes_applied, before);
}

#[test]
fn a_system_write_refuses_other_kinds_and_cancels_before_the_first_message() {
    let mut c = client(device_with_system(&system_doc(5)));
    let cancel = CancelToken::new();
    let plan = c.plan_system_write(&cancel).unwrap();
    let sent = c.port().sent.len();
    let perf = xppatch_core::Document::init(xppatch_core::DocKind::Performance);
    assert!(matches!(
        c.commit_system_write(&plan, &perf, &cancel),
        Err(ClientError::Refused(_))
    ));
    assert_eq!(
        c.port().sent.len(),
        sent,
        "nothing was sent for a wrong kind"
    );
    let stop = CancelToken::new();
    stop.cancel();
    let sent = c.port().sent.len();
    // the re-read before writing is skipped by a cancelled token only if it checks; either way no DT1 is sent
    let r = c.commit_system_write(&plan, &system_doc(6), &stop);
    assert!(r.is_err());
    assert_eq!(c.port().stats.writes_applied, 0, "{sent}");
}
