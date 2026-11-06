use super::*;
use crate::Filter;
use xppatch_core::SyxImage;

const BANK: &[u8] = include_bytes!("../../../tests/golden/xp60-user-bank.syx");

fn library() -> Library {
    Library::open_in_memory().unwrap()
}

fn blocks_of(slot: usize) -> Vec<u8> {
    SyxImage::parse(BANK)
        .unwrap()
        .user_patches()
        .unwrap()
        .remove(slot)
        .1
        .to_blocks()
}

fn imported() -> (Library, i64) {
    let mut lib = library();
    let r = lib.import_syx(BANK, "bank.syx", 100).unwrap();
    (lib, r.bulk_id)
}

fn at(lib: &Library, bulk: i64, pos: u8) -> Option<i64> {
    lib.bulk_contents(bulk)
        .unwrap()
        .into_iter()
        .find(|s| s.position == pos)
        .map(|s| s.patch_id)
}

#[test]
fn importing_a_bank_makes_a_bulk_named_after_the_file_with_every_slot_in_order() {
    let (lib, id) = imported();
    let bulks = lib.list_bulks().unwrap();
    assert_eq!(bulks.len(), 1);
    assert_eq!((bulks[0].name.as_str(), bulks[0].count), ("bank", 128));
    let contents = lib.bulk_contents(id).unwrap();
    assert_eq!(contents.len(), 128);
    assert!(contents
        .iter()
        .enumerate()
        .all(|(i, s)| usize::from(s.position) == i));
    let original = SyxImage::parse(BANK).unwrap().user_patches().unwrap();
    for (slot, patch) in &original {
        assert_eq!(contents[usize::from(*slot)].name, patch.name());
    }
}

#[test]
fn a_second_import_of_the_same_file_is_a_second_bulk_that_shares_the_patches() {
    let (mut lib, first) = imported();
    let again = lib.import_syx(BANK, "bank.syx", 200).unwrap();
    assert_eq!(again.bulk_name, "bank (2)");
    assert_eq!((again.added, again.duplicates), (0, 128));
    assert_eq!(
        lib.bulk_contents(first).unwrap(),
        lib.bulk_contents(again.bulk_id).unwrap()
    );
    assert_eq!(lib.list_bulks().unwrap().len(), 2);
}

#[test]
fn patches_of_a_bulk_are_listed_in_the_bulks_order() {
    let (lib, id) = imported();
    let rows = lib
        .list(&Filter {
            bulk: Some(id),
            ..Filter::default()
        })
        .unwrap();
    let mut first_seen: Vec<i64> = Vec::new();
    for s in lib.bulk_contents(id).unwrap() {
        if !first_seen.contains(&s.patch_id) {
            first_seen.push(s.patch_id);
        }
    }
    assert_eq!(rows.iter().map(|r| r.id).collect::<Vec<_>>(), first_seen);
}

#[test]
fn saving_the_bank_as_a_new_bulk_stores_each_patch_once_and_refuses_a_taken_name() {
    let (mut lib, _) = imported();
    let bank: Vec<(u8, Vec<u8>)> = (0..4u8).map(|i| (i, blocks_of(usize::from(i)))).collect();
    let before = lib.count(&Filter::default()).unwrap();
    let r = lib
        .save_bulk(SaveTarget::New, "Live set", &bank, 300)
        .unwrap();
    assert_eq!((r.added, r.reused, r.kept_version), (0, 4, false));
    assert_eq!(lib.count(&Filter::default()).unwrap(), before);
    assert_eq!(lib.bulk_contents(r.bulk_id).unwrap().len(), 4);
    assert!(matches!(
        lib.save_bulk(SaveTarget::New, "live SET", &bank, 301),
        Err(LibraryError::BulkNameTaken(_))
    ));
}

#[test]
fn overwriting_a_bulk_keeps_the_old_contents_as_a_version_that_can_be_restored() {
    let (mut lib, id) = imported();
    let new_bank: Vec<(u8, Vec<u8>)> = vec![(0, blocks_of(5)), (1, blocks_of(6))];
    let r = lib
        .save_bulk(SaveTarget::Overwrite(id), "bank", &new_bank, 400)
        .unwrap();
    assert!(r.kept_version);
    assert_eq!(lib.bulk_contents(id).unwrap().len(), 2);
    let versions = lib.bulk_revisions(id).unwrap();
    assert_eq!((versions.len(), versions[0].count), (1, 128));
    lib.restore_revision(id, versions[0].id, 500).unwrap();
    assert_eq!(lib.bulk_contents(id).unwrap().len(), 128);
    // the two-patch contents were kept too, so the restore can be undone
    assert_eq!(lib.bulk_revisions(id).unwrap().len(), 2);
    assert!(matches!(
        lib.restore_revision(id, 9999, 600),
        Err(LibraryError::NotFound(_))
    ));
}

#[test]
fn overwriting_can_rename_but_not_onto_another_bulks_name() {
    let (mut lib, id) = imported();
    let other = lib.import_syx(BANK, "other.syx", 150).unwrap().bulk_id;
    let bank = vec![(0u8, blocks_of(0))];
    assert!(matches!(
        lib.save_bulk(SaveTarget::Overwrite(id), "other", &bank, 1),
        Err(LibraryError::BulkNameTaken(_))
    ));
    lib.save_bulk(SaveTarget::Overwrite(other), "Renamed", &bank, 2)
        .unwrap();
    assert!(lib
        .list_bulks()
        .unwrap()
        .iter()
        .any(|b| b.name == "Renamed"));
}

#[test]
fn a_failed_save_changes_nothing() {
    let (mut lib, id) = imported();
    let bad = vec![(0u8, blocks_of(0)), (1u8, vec![1, 2, 3])];
    assert!(lib
        .save_bulk(SaveTarget::Overwrite(id), "bank", &bad, 1)
        .is_err());
    assert_eq!(lib.bulk_contents(id).unwrap().len(), 128);
    assert!(lib.bulk_revisions(id).unwrap().is_empty());
}

#[test]
fn placing_clearing_swapping_and_moving_patches() {
    let (mut lib, a) = imported();
    let b = lib
        .save_bulk(SaveTarget::New, "B", &[(0, blocks_of(1))], 10)
        .unwrap()
        .bulk_id;
    let (p0, p1) = (at(&lib, a, 0).unwrap(), at(&lib, a, 1).unwrap());
    lib.bulk_swap(a, 0, 1, 20).unwrap();
    assert_eq!((at(&lib, a, 0), at(&lib, a, 1)), (Some(p1), Some(p0)));
    lib.bulk_clear(a, 0, 21).unwrap();
    assert_eq!(at(&lib, a, 0), None);
    lib.bulk_swap(a, 0, 1, 22).unwrap(); // swapping with an empty position moves the patch
    assert_eq!((at(&lib, a, 0), at(&lib, a, 1)), (Some(p0), None));
    let in_b = at(&lib, b, 0).unwrap();
    lib.bulk_move((a, 0), (b, 0), 23).unwrap(); // onto an occupied position: they exchange
    assert_eq!((at(&lib, b, 0), at(&lib, a, 0)), (Some(p0), Some(in_b)));
    lib.bulk_place(b, 5, in_b, 24).unwrap();
    assert_eq!(at(&lib, b, 5), Some(in_b));
    assert_eq!(lib.bulk_append(b, p1, 25).unwrap(), 1);
    assert!(matches!(
        lib.bulk_place(b, 200, p1, 26),
        Err(LibraryError::BadPosition(200))
    ));
    assert!(matches!(
        lib.bulk_place(999, 0, p1, 26),
        Err(LibraryError::BulkNotFound(999))
    ));
}

#[test]
fn a_full_bulk_takes_no_more() {
    let (mut lib, id) = imported();
    let any = at(&lib, id, 0).unwrap();
    assert!(matches!(
        lib.bulk_append(id, any, 5),
        Err(LibraryError::BulkFull)
    ));
}

#[test]
fn a_run_of_edits_keeps_one_version_not_one_per_edit() {
    let (mut lib, id) = imported();
    for (i, now) in [10, 11, 12, 13].into_iter().enumerate() {
        lib.bulk_swap(id, 0, u8::try_from(i + 1).unwrap(), now)
            .unwrap();
    }
    assert_eq!(lib.bulk_revisions(id).unwrap().len(), 1);
    lib.bulk_swap(id, 0, 9, 1_000).unwrap(); // long after: a new version
    assert_eq!(lib.bulk_revisions(id).unwrap().len(), 2);
}

#[test]
fn deleting_a_bulk_deletes_only_patches_nothing_else_holds() {
    let (mut lib, a) = imported();
    let b = lib.import_syx(BANK, "shared.syx", 5).unwrap().bulk_id;
    let total = lib.count(&Filter::default()).unwrap();
    assert_eq!(
        lib.delete_bulk(a).unwrap(),
        0,
        "the other bulk still holds them all"
    );
    assert_eq!(lib.count(&Filter::default()).unwrap(), total);
    let keep = lib.bulk_contents(b).unwrap()[0].patch_id;
    lib.set_favourite(keep, true).unwrap();
    let removed = lib.delete_bulk(b).unwrap();
    assert_eq!(removed as u64, total - 1, "the favourite stays");
    assert!(lib.list_bulks().unwrap().is_empty());
    assert_eq!(lib.count(&Filter::default()).unwrap(), 1);
}

#[test]
fn deleting_a_patch_empties_its_positions_everywhere() {
    let (mut lib, id) = imported();
    let victim = lib.bulk_contents(id).unwrap()[3].patch_id;
    lib.delete(victim).unwrap();
    assert!(lib
        .bulk_contents(id)
        .unwrap()
        .iter()
        .all(|s| s.patch_id != victim));
}

#[test]
fn duplicating_and_renaming() {
    let (mut lib, id) = imported();
    let copy = lib.duplicate_bulk(id, "Copy", 9).unwrap();
    assert_eq!(
        lib.bulk_contents(copy).unwrap(),
        lib.bulk_contents(id).unwrap()
    );
    assert!(matches!(
        lib.duplicate_bulk(id, "copy", 9),
        Err(LibraryError::BulkNameTaken(_))
    ));
    lib.rename_bulk(copy, "Stage", 10).unwrap();
    assert!(lib.rename_bulk(copy, "BANK", 11).is_err());
    assert!(lib.rename_bulk(copy, "  ", 11).is_err());
}

#[test]
fn search_finds_patches_under_their_bulk() {
    let (lib, id) = imported();
    let first = lib.bulk_contents(id).unwrap()[0].name.clone();
    let needle: String = first.trim().chars().take(3).collect();
    let hits = lib.bulk_search(&needle, 50).unwrap();
    assert!(!hits.is_empty());
    assert!(hits
        .iter()
        .all(|h| h.bulk_id == id && h.name.to_lowercase().contains(&needle.to_lowercase())));
}

#[test]
fn an_exported_bulk_imports_back_with_each_patch_at_its_position() {
    let (mut lib, id) = imported();
    lib.bulk_clear(id, 4, 1).unwrap(); // a gap stays a gap
    let bytes = lib.export_bulk(id, 0x10).unwrap();
    let r = lib.import_syx(&bytes, "round.syx", 2).unwrap();
    assert_eq!((r.added, r.duplicates), (0, 127));
    let again = lib.bulk_contents(r.bulk_id).unwrap();
    assert_eq!(again.len(), 127);
    assert!(again.iter().all(|s| s.position != 4));
    assert_eq!(again, lib.bulk_contents(id).unwrap());
}

#[test]
fn names_are_cleaned_and_taken_from_file_names() {
    assert_eq!(clean_name("  Live  ").unwrap(), "Live");
    assert!(clean_name("").is_err());
    assert!(clean_name("tab\there").is_err());
    assert!(clean_name(&"x".repeat(61)).is_err());
    assert_eq!(name_from_source("allright.syx"), "allright");
    assert_eq!(name_from_source("Bulk 2.MID"), "Bulk 2");
    assert_eq!(name_from_source(".syx"), "Imported");
    assert_eq!(name_from_source("editor"), "editor");
}

#[test]
fn a_library_made_before_bulks_is_grouped_by_source_once_and_backed_up() {
    let dir = std::env::temp_dir().join(format!("xp-bulk-migrate-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let path = dir.join("library.sqlite");
    let _ = std::fs::remove_file(&path);
    {
        let c = rusqlite::Connection::open(&path).unwrap();
        c.execute_batch(
            "CREATE TABLE patch (id INTEGER PRIMARY KEY, hash TEXT NOT NULL, name TEXT NOT NULL, bytes BLOB NOT NULL,
               source TEXT NOT NULL, favourite INTEGER NOT NULL DEFAULT 0, added_at INTEGER NOT NULL);
             CREATE TABLE tag (patch_id INTEGER NOT NULL, tag TEXT NOT NULL, PRIMARY KEY (patch_id, tag));",
        )
        .unwrap();
        for (i, src) in ["old.syx", "old.syx", "Editor"].iter().enumerate() {
            c.execute(
                "INSERT INTO patch (hash, name, bytes, source, added_at) VALUES (?1, ?2, ?3, ?4, ?5)",
                params![
                    format!("h{i}"),
                    format!("P{i}"),
                    blocks_of(i),
                    src,
                    50 + i64::try_from(i).unwrap()
                ],
            )
            .unwrap();
        }
    }
    let lib = Library::open(&path).unwrap();
    let bulks = lib.list_bulks().unwrap();
    assert_eq!(
        bulks
            .iter()
            .map(|b| (b.name.as_str(), b.count))
            .collect::<Vec<_>>(),
        [("Editor", 1), ("old", 2)]
    );
    assert!(dir.join("library.before-bulks.bak").exists());
    drop(lib);
    let lib = Library::open(&path).unwrap();
    assert_eq!(
        lib.list_bulks().unwrap().len(),
        2,
        "opening again changes nothing"
    );
    drop(lib);
    let _ = std::fs::remove_dir_all(&dir);
}
