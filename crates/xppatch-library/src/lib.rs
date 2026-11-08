//! The patch library: patches kept in a SQLite file, imported from and exported to `.syx`, with tags, favourites, search
//! and duplicate detection. Nothing here talks to the instrument.
//!
//! A patch is stored as its 590 bytes of block data (Common then Tone 1 to 4). Two patches are duplicates when those
//! bytes are identical; the hash only finds candidates quickly and the bytes are always compared.

mod bulk;

pub use bulk::{
    clean_name, name_from_source, BulkHit, BulkRow, BulkSlot, Revision, SaveReport, SaveTarget,
    BULK_SLOTS, MAX_BULK_NAME, MAX_REVISIONS,
};

use rusqlite::{params, Connection, OptionalExtension};
use std::collections::BTreeMap;
use thiserror::Error;
use xppatch_core::{Patch, PatchError, SyxError, SyxImage};
use xppatch_sysex::message::{DeviceId, ParseError};

/// Bytes in a stored patch: Common 73 and four Tones of 129.
pub const PATCH_BYTES: usize = Patch::BLOCK_BYTES;

#[derive(Debug, Error)]
#[non_exhaustive]
pub enum LibraryError {
    #[error(
        "The library could not be opened or saved. Close other copies of this app and try again."
    )]
    Database(#[from] rusqlite::Error),
    #[error(transparent)]
    Syx(#[from] SyxError),
    #[error("That patch data is not valid.")]
    Patch(#[from] PatchError),
    #[error("That device ID is not valid. Use 17 to 32 on the panel.")]
    Device(#[from] ParseError),
    #[error("That file holds no patches.")]
    NoPatches,
    #[error("That patch is no longer in the library.")]
    NotFound(i64),
    #[error("A bank holds at most 128 patches (this one has {0}).")]
    TooMany(usize),
    #[error("Stored patch {0} is damaged and cannot be opened.")]
    Damaged(i64, String),
    #[error("A tag must be 1 to 40 characters with no control characters.")]
    BadTag,
    #[error("That name is already used by another patch.")]
    WouldDuplicate(i64),
    #[error("A bulk name must be 1 to 60 characters with no control characters.")]
    BadBulkName,
    #[error("A bulk called \"{0}\" already exists.")]
    BulkNameTaken(String),
    #[error("That bulk is no longer in the library.")]
    BulkNotFound(i64),
    #[error("That is not a valid place in a bulk (use positions 1 to 128).")]
    BadPosition(usize),
    #[error("That bulk already holds 128 patches.")]
    BulkFull,
}

pub type Result<T> = std::result::Result<T, LibraryError>;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AddOutcome {
    Added(i64),
    /// An identical patch is already stored.
    Duplicate(i64),
}

/// What an import did.
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct ImportReport {
    /// Patches that were new to the library.
    pub added: usize,
    /// Patches that were already stored (the bulk refers to the stored one).
    pub duplicates: usize,
    /// The bulk the file became.
    pub bulk_id: i64,
    pub bulk_name: String,
}

#[derive(Debug, Clone, Default)]
pub struct Filter {
    /// Case-insensitive substring of the name.
    pub text: Option<String>,
    pub tag: Option<String>,
    pub favourites_only: bool,
    /// Only patches of this bulk, in their order there.
    pub bulk: Option<i64>,
    pub limit: Option<u32>,
    pub offset: u32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Row {
    pub id: i64,
    pub name: String,
    pub tags: Vec<String>,
    pub favourite: bool,
    pub source: String,
    pub added_at: i64,
}

pub struct Library {
    conn: Connection,
}

/// FNV-1a, 64 bit: stable across platforms and versions. Used only to find duplicate candidates.
fn hash(bytes: &[u8]) -> String {
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    for &b in bytes {
        h ^= u64::from(b);
        h = h.wrapping_mul(0x0000_0100_0000_01b3);
    }
    format!("{h:016x}")
}

fn snapshot(patch: &Patch) -> Vec<u8> {
    patch.to_blocks()
}

fn from_snapshot(id: i64, bytes: &[u8]) -> Result<Patch> {
    Patch::from_blocks(bytes).map_err(|e| LibraryError::Damaged(id, e.to_string()))
}

/// The `LIKE` pattern for everything inside the folder `path`: the path, a slash and anything after it.
fn folder_pattern(path: &str) -> String {
    let mut out = String::new();
    for c in path.chars() {
        if matches!(c, '%' | '_' | '\\') {
            out.push('\\');
        }
        out.push(c);
    }
    out.push_str("/%");
    out
}

/// Escape `%`, `_` and the escape character itself for a `LIKE ... ESCAPE '\'` pattern.
fn like_pattern(text: &str) -> String {
    let mut out = String::from("%");
    for c in text.chars() {
        if matches!(c, '%' | '_' | '\\') {
            out.push('\\');
        }
        out.push(c);
    }
    out.push('%');
    out
}

impl Library {
    pub fn open(path: &std::path::Path) -> Result<Library> {
        let conn = Connection::open(path)?;
        // A library made before bulks existed is grouped into bulks once; keep a copy of it as it was first.
        let version: i64 = conn.query_row("PRAGMA user_version", [], |r| r.get(0))?;
        let has_patches = conn
            .query_row(
                "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'patch'",
                [],
                |r| r.get::<_, i64>(0),
            )
            .map(|n| n > 0)?;
        if version < 1 && has_patches {
            let backup = path.with_extension("before-bulks.bak");
            if !backup.exists() {
                conn.execute("VACUUM INTO ?1", params![backup.to_string_lossy()])?;
            }
        }
        Library::init(conn)
    }

    /// Write a consistent copy of the whole library to `dest` (which must not exist yet). The open library is untouched.
    pub fn snapshot_to(&self, dest: &std::path::Path) -> Result<()> {
        self.conn
            .execute("VACUUM INTO ?1", params![dest.to_string_lossy()])?;
        Ok(())
    }

    pub fn open_in_memory() -> Result<Library> {
        Library::init(Connection::open_in_memory()?)
    }

    fn init(conn: Connection) -> Result<Library> {
        conn.execute_batch(
            "PRAGMA foreign_keys = ON;
             CREATE TABLE IF NOT EXISTS patch (
               id INTEGER PRIMARY KEY,
               hash TEXT NOT NULL,
               name TEXT NOT NULL,
               bytes BLOB NOT NULL,
               source TEXT NOT NULL,
               favourite INTEGER NOT NULL DEFAULT 0,
               added_at INTEGER NOT NULL
             );
             CREATE INDEX IF NOT EXISTS patch_hash ON patch(hash);
             CREATE TABLE IF NOT EXISTS tag (
               patch_id INTEGER NOT NULL REFERENCES patch(id) ON DELETE CASCADE,
               tag TEXT NOT NULL,
               PRIMARY KEY (patch_id, tag)
             );
             CREATE TABLE IF NOT EXISTS bulk (
               id INTEGER PRIMARY KEY,
               name TEXT NOT NULL UNIQUE COLLATE NOCASE,
               source TEXT NOT NULL DEFAULT '',
               created_at INTEGER NOT NULL,
               updated_at INTEGER NOT NULL
             );
             CREATE TABLE IF NOT EXISTS bulk_patch (
               bulk_id INTEGER NOT NULL REFERENCES bulk(id) ON DELETE CASCADE,
               position INTEGER NOT NULL CHECK (position BETWEEN 0 AND 127),
               patch_id INTEGER NOT NULL REFERENCES patch(id) ON DELETE CASCADE,
               PRIMARY KEY (bulk_id, position)
             );
             CREATE INDEX IF NOT EXISTS bulk_patch_patch ON bulk_patch(patch_id);
             CREATE TABLE IF NOT EXISTS bulk_revision (
               id INTEGER PRIMARY KEY,
               bulk_id INTEGER NOT NULL REFERENCES bulk(id) ON DELETE CASCADE,
               saved_at INTEGER NOT NULL,
               slots TEXT NOT NULL
             );",
        )?;
        let mut lib = Library { conn };
        let version: i64 = lib
            .conn
            .query_row("PRAGMA user_version", [], |r| r.get(0))?;
        if version < 1 {
            lib.migrate_to_bulks()?;
            lib.conn.execute_batch("PRAGMA user_version = 1")?;
        }
        Ok(lib)
    }

    /// Store `patch`, unless an identical one is already stored.
    pub fn add_patch(&mut self, patch: &Patch, source: &str, now: i64) -> Result<AddOutcome> {
        let issues = patch.validate().len();
        if issues > 0 {
            return Err(PatchError::Invalid { count: issues }.into());
        }
        let bytes = snapshot(patch);
        let h = hash(&bytes);
        let mut stmt = self
            .conn
            .prepare("SELECT id, bytes FROM patch WHERE hash = ?1")?;
        let mut rows = stmt.query(params![h])?;
        while let Some(row) = rows.next()? {
            let existing: Vec<u8> = row.get(1)?;
            if existing == bytes {
                return Ok(AddOutcome::Duplicate(row.get(0)?));
            }
        }
        drop(rows);
        drop(stmt);
        self.conn.execute(
            "INSERT INTO patch (hash, name, bytes, source, added_at) VALUES (?1, ?2, ?3, ?4, ?5)",
            params![h, patch.name(), bytes, source, now],
        )?;
        Ok(AddOutcome::Added(self.conn.last_insert_rowid()))
    }

    /// Import every patch of a `.syx` file as a bulk named after the file: a User Patch bank keeps each patch at its slot, a single
    /// patch dump to the temporary patch becomes a bulk of one. A name already taken gets a number ("bank (2)"); nothing is
    /// overwritten. All or nothing.
    pub fn import_syx(&mut self, bytes: &[u8], source: &str, now: i64) -> Result<ImportReport> {
        // A file brought in by a person is read the way the instrument reads it: damaged messages are skipped, not fatal.
        let (image, skipped) = SyxImage::parse_lenient(bytes)?;
        let mut patches: Vec<(u8, Patch)> = image.user_patches_lenient(skipped.len())?.0;
        if patches.is_empty() {
            patches.extend(image.temp_patch()?.map(|p| (0, p)));
        }
        if patches.is_empty() {
            return Err(LibraryError::NoPatches);
        }
        self.in_tx(|lib| {
            let (mut added, mut duplicates) = (0, 0);
            let mut slots = Vec::with_capacity(patches.len());
            for (slot, p) in &patches {
                match lib.add_patch(p, source, now)? {
                    AddOutcome::Added(id) => {
                        added += 1;
                        slots.push((*slot, id));
                    }
                    AddOutcome::Duplicate(id) => {
                        duplicates += 1;
                        slots.push((*slot, id));
                    }
                }
            }
            let bulk_name = lib.unique_bulk_name(&name_from_source(source))?;
            let bulk_id = lib.create_bulk_inner(&bulk_name, source, &slots, now)?;
            Ok(ImportReport {
                added,
                duplicates,
                bulk_id,
                bulk_name,
            })
        })
    }

    pub fn get_patch(&self, id: i64) -> Result<Patch> {
        let bytes: Vec<u8> = self
            .conn
            .query_row("SELECT bytes FROM patch WHERE id = ?1", params![id], |r| {
                r.get(0)
            })
            .optional()?
            .ok_or(LibraryError::NotFound(id))?;
        from_snapshot(id, &bytes)
    }

    fn where_clause(f: &Filter) -> (String, Vec<String>) {
        let mut clauses = Vec::new();
        let mut args = Vec::new();
        if let Some(t) = f.text.as_deref().filter(|t| !t.is_empty()) {
            args.push(like_pattern(t));
            clauses.push(format!("p.name LIKE ?{} ESCAPE '\\'", args.len()));
        }
        if let Some(tag) = f.tag.as_deref().filter(|t| !t.is_empty()) {
            // A tag is also a folder: "Pads" finds patches tagged "Pads" and those tagged "Pads/Warm" or "Pads/Warm/Dark".
            args.push(tag.to_owned());
            let exact = args.len();
            args.push(folder_pattern(tag));
            clauses.push(format!(
                "EXISTS (SELECT 1 FROM tag t WHERE t.patch_id = p.id AND (t.tag = ?{exact} OR t.tag LIKE ?{} ESCAPE '\\'))",
                args.len()
            ));
        }
        if f.favourites_only {
            clauses.push("p.favourite = 1".to_owned());
        }
        if let Some(bulk) = f.bulk {
            // An integer, formatted in: nothing a person typed reaches the SQL.
            clauses.push(format!(
                "p.id IN (SELECT patch_id FROM bulk_patch WHERE bulk_id = {bulk})"
            ));
        }
        let sql = if clauses.is_empty() {
            String::new()
        } else {
            format!("WHERE {}", clauses.join(" AND "))
        };
        (sql, args)
    }

    pub fn count(&self, f: &Filter) -> Result<u64> {
        let (clause, args) = Library::where_clause(f);
        let n: i64 = self.conn.query_row(
            &format!("SELECT COUNT(*) FROM patch p {clause}"),
            rusqlite::params_from_iter(args.iter()),
            |r| r.get(0),
        )?;
        Ok(u64::try_from(n).unwrap_or(0))
    }

    pub fn list(&self, f: &Filter) -> Result<Vec<Row>> {
        let (clause, args) = Library::where_clause(f);
        // Never an unbounded list: callers that do not ask for a page get the first 5000.
        let limit = i64::from(f.limit.unwrap_or(5000).min(5000));
        let order = match f.bulk {
            Some(bulk) => format!(
                "(SELECT bp.position FROM bulk_patch bp WHERE bp.bulk_id = {bulk} AND bp.patch_id = p.id), p.id"
            ),
            None => "p.name COLLATE NOCASE, p.id".to_owned(),
        };
        let sql = format!(
            "SELECT p.id, p.name, p.favourite, p.source, p.added_at FROM patch p {clause}
             ORDER BY {order} LIMIT {limit} OFFSET {}",
            f.offset
        );
        let mut stmt = self.conn.prepare(&sql)?;
        let mut rows: Vec<Row> = stmt
            .query_map(rusqlite::params_from_iter(args.iter()), |r| {
                Ok(Row {
                    id: r.get(0)?,
                    name: r.get(1)?,
                    tags: Vec::new(),
                    favourite: r.get::<_, i64>(2)? != 0,
                    source: r.get(3)?,
                    added_at: r.get(4)?,
                })
            })?
            .collect::<std::result::Result<_, _>>()?;
        let mut tags: BTreeMap<i64, Vec<String>> = BTreeMap::new();
        let mut tag_stmt = self
            .conn
            .prepare("SELECT patch_id, tag FROM tag ORDER BY tag")?;
        for t in tag_stmt.query_map([], |r| Ok((r.get::<_, i64>(0)?, r.get::<_, String>(1)?)))? {
            let (id, tag) = t?;
            tags.entry(id).or_default().push(tag);
        }
        for row in &mut rows {
            row.tags = tags.remove(&row.id).unwrap_or_default();
        }
        Ok(rows)
    }

    pub fn all_tags(&self) -> Result<Vec<(String, u64)>> {
        let mut stmt = self
            .conn
            .prepare("SELECT tag, COUNT(*) FROM tag GROUP BY tag ORDER BY tag")?;
        let out = stmt
            .query_map([], |r| Ok((r.get::<_, String>(0)?, r.get::<_, i64>(1)?)))?
            .map(|r| r.map(|(t, n)| (t, u64::try_from(n).unwrap_or(0))))
            .collect::<std::result::Result<_, _>>()?;
        Ok(out)
    }

    fn ensure(&self, id: i64) -> Result<()> {
        let n: i64 = self.conn.query_row(
            "SELECT COUNT(*) FROM patch WHERE id = ?1",
            params![id],
            |r| r.get(0),
        )?;
        if n == 0 {
            Err(LibraryError::NotFound(id))
        } else {
            Ok(())
        }
    }

    pub fn set_favourite(&mut self, id: i64, favourite: bool) -> Result<()> {
        self.ensure(id)?;
        self.conn.execute(
            "UPDATE patch SET favourite = ?2 WHERE id = ?1",
            params![id, i64::from(favourite)],
        )?;
        Ok(())
    }

    pub fn add_tag(&mut self, id: i64, tag: &str) -> Result<()> {
        let tag = tag.trim();
        if tag.is_empty() || tag.chars().count() > 40 || tag.chars().any(char::is_control) {
            return Err(LibraryError::BadTag);
        }
        self.ensure(id)?;
        self.conn.execute(
            "INSERT OR IGNORE INTO tag (patch_id, tag) VALUES (?1, ?2)",
            params![id, tag],
        )?;
        Ok(())
    }

    pub fn remove_tag(&mut self, id: i64, tag: &str) -> Result<()> {
        self.ensure(id)?;
        self.conn.execute(
            "DELETE FROM tag WHERE patch_id = ?1 AND tag = ?2",
            params![id, tag.trim()],
        )?;
        Ok(())
    }

    pub fn delete(&mut self, id: i64) -> Result<()> {
        self.ensure(id)?;
        self.conn
            .execute("DELETE FROM patch WHERE id = ?1", params![id])?;
        Ok(())
    }

    /// Give a stored patch a new name. Refused if the renamed patch would be identical to another one.
    pub fn rename(&mut self, id: i64, name: &str) -> Result<()> {
        let mut patch = self.get_patch(id)?;
        patch
            .set_name(name)
            .map_err(|e| LibraryError::Patch(PatchError::from(e)))?;
        let bytes = snapshot(&patch);
        let h = hash(&bytes);
        let mut stmt = self
            .conn
            .prepare("SELECT id, bytes FROM patch WHERE hash = ?1 AND id != ?2")?;
        let mut rows = stmt.query(params![h, id])?;
        while let Some(row) = rows.next()? {
            let other: Vec<u8> = row.get(1)?;
            if other == bytes {
                return Err(LibraryError::WouldDuplicate(row.get(0)?));
            }
        }
        drop(rows);
        drop(stmt);
        self.conn.execute(
            "UPDATE patch SET name = ?2, bytes = ?3, hash = ?4 WHERE id = ?1",
            params![id, patch.name(), bytes, h],
        )?;
        Ok(())
    }

    /// Export patches as a User Patch bank `.syx` in the given order (slots 0, 1, 2, ...), each block split to at most
    /// 128 data bytes so the file can be sent to the instrument as it is.
    pub fn export_syx(&self, ids: &[i64], device: u8) -> Result<Vec<u8>> {
        if ids.len() > 128 {
            return Err(LibraryError::TooMany(ids.len()));
        }
        let mut patches = Vec::with_capacity(ids.len());
        for (slot, &id) in ids.iter().enumerate() {
            let slot = u8::try_from(slot).map_err(|_| LibraryError::TooMany(ids.len()))?;
            patches.push((slot, self.get_patch(id)?));
        }
        Ok(xppatch_core::user_patch_bank_bytes(
            &patches,
            DeviceId::new(device)?,
            Some(128),
        )?)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use xppatch_core::BlockBuf;

    const BANK: &[u8] = include_bytes!("../../../tests/golden/xp60-user-bank.syx");

    fn bank_patches() -> Vec<Patch> {
        SyxImage::parse(BANK)
            .unwrap()
            .user_patches()
            .unwrap()
            .into_iter()
            .map(|(_, p)| p)
            .collect()
    }

    fn unique_count() -> usize {
        let mut seen: Vec<Vec<u8>> = Vec::new();
        for p in bank_patches() {
            let s = snapshot(&p);
            if !seen.contains(&s) {
                seen.push(s);
            }
        }
        seen.len()
    }

    fn library() -> Library {
        Library::open_in_memory().unwrap()
    }

    #[test]
    fn importing_a_real_bank_stores_each_distinct_patch_once() {
        let mut lib = library();
        let r = lib.import_syx(BANK, "bank.syx", 100).unwrap();
        assert_eq!(r.added, unique_count());
        assert_eq!(r.added + r.duplicates, 128);
        assert_eq!(
            lib.count(&Filter::default()).unwrap() as usize,
            unique_count()
        );
        let again = lib.import_syx(BANK, "bank.syx", 200).unwrap();
        assert_eq!((again.added, again.duplicates), (0, 128));
        assert_eq!(again.bulk_name, "bank (2)");
    }

    #[test]
    fn a_stored_patch_comes_back_byte_for_byte() {
        let mut lib = library();
        lib.import_syx(BANK, "bank.syx", 1).unwrap();
        let rows = lib.list(&Filter::default()).unwrap();
        let original = bank_patches();
        for row in rows {
            let got = lib.get_patch(row.id).unwrap();
            assert!(original.iter().any(|p| p == &got), "{}", row.name);
            assert_eq!(got.name().trim_end(), row.name.trim_end());
        }
    }

    #[test]
    fn single_patch_dumps_to_the_temporary_patch_import_too() {
        use xppatch_sysex::chunk::{split_dt1, MAX_DT1_DATA_TO_DEVICE};
        use xppatch_sysex::memory::TEMP_PATCH;
        let patch = bank_patches().remove(3);
        let mut bytes = Vec::new();
        for (a, d) in patch.to_dt1(TEMP_PATCH).unwrap() {
            for m in split_dt1(a, &d, MAX_DT1_DATA_TO_DEVICE).unwrap() {
                bytes.extend(m.encode(DeviceId::DEFAULT).unwrap());
            }
        }
        let mut lib = library();
        let r = lib.import_syx(&bytes, "single.syx", 5).unwrap();
        assert_eq!(
            (r.added, r.duplicates, r.bulk_name.as_str()),
            (1, 0, "single")
        );
        let id = lib.list(&Filter::default()).unwrap()[0].id;
        assert_eq!(lib.get_patch(id).unwrap(), patch);
    }

    #[test]
    fn files_without_patches_or_with_damage_are_errors_and_change_nothing() {
        let mut lib = library();
        assert!(lib.import_syx(&[], "x", 1).is_err());
        assert!(lib.import_syx(&BANK[..500], "x", 1).is_err());
        assert!(lib.import_syx(b"not a syx file", "x", 1).is_err());
        // A performance dump has no patches.
        let perf = [
            0xF0, 0x41, 0x10, 0x6A, 0x12, 0x01, 0x00, 0x00, 0x00, 0x05, 0x7A, 0xF7,
        ];
        assert!(matches!(
            lib.import_syx(&perf, "x", 1),
            Err(LibraryError::NoPatches)
        ));
        assert_eq!(lib.count(&Filter::default()).unwrap(), 0);
    }

    #[test]
    fn invalid_patches_are_refused() {
        let mut lib = library();
        let mut p = Patch::init();
        p.tones[0] = BlockBuf::from_bytes(p.tones[0].def(), &{
            let mut raw = p.tones[0].bytes().to_vec();
            raw[0] = 9; // tone_switch is 0..=1
            raw
        })
        .unwrap();
        assert!(matches!(
            lib.add_patch(&p, "x", 1),
            Err(LibraryError::Patch(_))
        ));
    }

    #[test]
    fn search_tags_and_favourites_filter_together() {
        let mut lib = library();
        let mut ids = Vec::new();
        for (i, name) in ["Warm Pad", "Bright Pad", "Slap Bass", "100% Organ", "a_b"]
            .iter()
            .enumerate()
        {
            let mut p = Patch::init();
            p.set_name(name).unwrap();
            p.tones[0].set("tone_level", i as i32 + 1).unwrap();
            let AddOutcome::Added(id) = lib.add_patch(&p, "t", 10).unwrap() else {
                panic!("expected a new patch");
            };
            ids.push(id);
        }
        lib.add_tag(ids[0], "pad").unwrap();
        lib.add_tag(ids[1], "pad").unwrap();
        lib.add_tag(ids[1], "bright").unwrap();
        lib.add_tag(ids[2], "bass").unwrap();
        lib.set_favourite(ids[1], true).unwrap();

        let names = |f: &Filter| -> Vec<String> {
            lib.list(f)
                .unwrap()
                .into_iter()
                .map(|r| r.name.trim_end().to_owned())
                .collect()
        };
        let text = |t: &str| Filter {
            text: Some(t.to_owned()),
            ..Filter::default()
        };
        assert_eq!(names(&text("pad")), ["Bright Pad", "Warm Pad"]);
        assert_eq!(names(&text("PAD")), ["Bright Pad", "Warm Pad"]);
        assert_eq!(names(&text("%")), ["100% Organ"]);
        assert_eq!(names(&text("_")), ["a_b"]);
        assert_eq!(names(&text("\\")), Vec::<String>::new());
        assert_eq!(
            names(&Filter {
                tag: Some("pad".into()),
                ..Filter::default()
            }),
            ["Bright Pad", "Warm Pad"]
        );
        assert_eq!(
            names(&Filter {
                tag: Some("pad".into()),
                favourites_only: true,
                ..Filter::default()
            }),
            ["Bright Pad"]
        );
        assert_eq!(
            names(&Filter {
                tag: Some("pad".into()),
                text: Some("warm".into()),
                ..Filter::default()
            }),
            ["Warm Pad"]
        );
        assert_eq!(lib.count(&text("pad")).unwrap(), 2);
        let first = lib
            .list(&Filter {
                limit: Some(2),
                ..Filter::default()
            })
            .unwrap();
        assert_eq!(first.len(), 2);
        let rest = lib
            .list(&Filter {
                limit: Some(10),
                offset: 2,
                ..Filter::default()
            })
            .unwrap();
        assert_eq!(rest.len(), 3);
        let tags = lib.all_tags().unwrap();
        assert_eq!(
            tags,
            [("bass".into(), 1), ("bright".into(), 1), ("pad".into(), 2)]
        );
        let row = lib.list(&text("Bright")).unwrap().remove(0);
        assert_eq!(row.tags, ["bright", "pad"]);
        assert!(row.favourite);
    }

    #[test]
    fn tags_are_validated_and_removable_and_deleting_cascades() {
        let mut lib = library();
        let AddOutcome::Added(id) = lib.add_patch(&Patch::init(), "t", 1).unwrap() else {
            panic!()
        };
        assert!(matches!(lib.add_tag(id, "  "), Err(LibraryError::BadTag)));
        assert!(matches!(lib.add_tag(id, "a\nb"), Err(LibraryError::BadTag)));
        assert!(matches!(
            lib.add_tag(id, &"x".repeat(41)),
            Err(LibraryError::BadTag)
        ));
        assert!(matches!(
            lib.add_tag(999, "ok"),
            Err(LibraryError::NotFound(999))
        ));
        lib.add_tag(id, "  keys ").unwrap();
        lib.add_tag(id, "keys").unwrap();
        assert_eq!(lib.all_tags().unwrap(), [("keys".to_owned(), 1)]);
        lib.remove_tag(id, "keys").unwrap();
        assert!(lib.all_tags().unwrap().is_empty());
        lib.add_tag(id, "again").unwrap();
        lib.delete(id).unwrap();
        assert!(lib.all_tags().unwrap().is_empty());
        assert!(matches!(lib.get_patch(id), Err(LibraryError::NotFound(_))));
        assert!(matches!(lib.delete(id), Err(LibraryError::NotFound(_))));
    }

    #[test]
    fn renaming_changes_the_stored_patch_and_cannot_create_a_duplicate() {
        let mut lib = library();
        let mut a = Patch::init();
        a.set_name("Alpha").unwrap();
        let mut b = Patch::init();
        b.set_name("Beta").unwrap();
        let AddOutcome::Added(ia) = lib.add_patch(&a, "t", 1).unwrap() else {
            panic!()
        };
        let AddOutcome::Added(ib) = lib.add_patch(&b, "t", 1).unwrap() else {
            panic!()
        };
        lib.rename(ia, "Gamma").unwrap();
        assert_eq!(lib.get_patch(ia).unwrap().name(), "Gamma");
        assert!(
            matches!(lib.rename(ib, "Gamma"), Err(LibraryError::WouldDuplicate(id)) if id == ia)
        );
        assert_eq!(lib.get_patch(ib).unwrap().name(), "Beta");
        assert!(lib.rename(ia, "far too long for a name").is_err());
        // The old content can now be added again (it is no longer stored).
        assert!(matches!(
            lib.add_patch(&a, "t", 2).unwrap(),
            AddOutcome::Added(_)
        ));
    }

    #[test]
    fn exporting_produces_a_bank_the_instrument_accepts_and_that_imports_back() {
        let mut lib = library();
        lib.import_syx(BANK, "bank.syx", 1).unwrap();
        let ids: Vec<i64> = lib
            .list(&Filter::default())
            .unwrap()
            .iter()
            .take(10)
            .map(|r| r.id)
            .collect();
        let out = lib.export_syx(&ids, 0x10).unwrap();
        let image = SyxImage::parse(&out).unwrap();
        assert!(image.records.iter().all(|r| r.data.len() <= 128));
        let slots = image.user_patches().unwrap();
        assert_eq!(slots.len(), 10);
        for ((slot, patch), id) in slots.iter().zip(&ids) {
            assert_eq!(
                usize::from(*slot),
                ids.iter().position(|i| i == id).unwrap()
            );
            assert_eq!(patch, &lib.get_patch(*id).unwrap());
        }
        assert!(lib.export_syx(&[1, 999], 0x10).is_err());
        assert!(lib.export_syx(&vec![1; 129], 0x10).is_err());
        assert!(lib.export_syx(&ids, 0x99).is_err());
    }

    #[test]
    fn a_file_database_persists_between_opens() {
        let dir = std::env::temp_dir().join(format!("xppatch-lib-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("library.sqlite");
        let _ = std::fs::remove_file(&path);
        {
            let mut lib = Library::open(&path).unwrap();
            lib.import_syx(BANK, "bank.syx", 1).unwrap();
        }
        let lib = Library::open(&path).unwrap();
        assert_eq!(
            lib.count(&Filter::default()).unwrap() as usize,
            unique_count()
        );
        drop(lib);
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_damaged_stored_blob_is_reported_not_trusted() {
        let mut lib = library();
        let AddOutcome::Added(id) = lib.add_patch(&Patch::init(), "t", 1).unwrap() else {
            panic!()
        };
        lib.conn
            .execute(
                "UPDATE patch SET bytes = x'0102' WHERE id = ?1",
                params![id],
            )
            .unwrap();
        assert!(matches!(lib.get_patch(id), Err(LibraryError::Damaged(..))));
        lib.conn
            .execute(
                "UPDATE patch SET bytes = zeroblob(590) WHERE id = ?1",
                params![id],
            )
            .unwrap();
        // All zero bytes are not valid text for the name either.
        assert!(matches!(lib.get_patch(id), Err(LibraryError::Damaged(..))));
    }

    #[test]
    fn a_snapshot_is_a_complete_independent_copy() {
        let mut lib = library();
        lib.import_syx(BANK, "bank", 1).unwrap();
        let dir = std::env::temp_dir().join(format!("xppatch-snap-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let dest = dir.join("copy.sqlite");
        let _ = std::fs::remove_file(&dest);
        lib.snapshot_to(&dest).unwrap();
        let copy = Library::open(&dest).unwrap();
        assert_eq!(
            copy.count(&Filter::default()).unwrap(),
            lib.count(&Filter::default()).unwrap()
        );
        // The destination must not exist: a snapshot never overwrites a library.
        assert!(lib.snapshot_to(&dest).is_err());
        drop(copy);
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_tag_is_also_a_folder_that_holds_the_tags_below_it() {
        let mut lib = library();
        lib.import_syx(BANK, "bank", 1).unwrap();
        let ids: Vec<i64> = lib
            .list(&Filter::default())
            .unwrap()
            .iter()
            .map(|r| r.id)
            .collect();
        lib.add_tag(ids[0], "Pads").unwrap();
        lib.add_tag(ids[1], "Pads/Warm").unwrap();
        lib.add_tag(ids[2], "Pads/Warm/Dark").unwrap();
        lib.add_tag(ids[3], "Padsy").unwrap(); // starts with the same letters but is not inside the folder
        lib.add_tag(ids[4], "Bass/Pads").unwrap(); // "Pads" appears, but not at the start of the path
        let count = |tag: &str| {
            lib.count(&Filter {
                tag: Some(tag.into()),
                ..Filter::default()
            })
            .unwrap()
        };
        assert_eq!(count("Pads"), 3);
        assert_eq!(count("Pads/Warm"), 2);
        assert_eq!(count("Pads/Warm/Dark"), 1);
        assert_eq!(count("Padsy"), 1);
        assert_eq!(count("Bass"), 1);
        assert_eq!(count("Nothing"), 0);
    }

    #[test]
    fn folder_names_with_wildcards_match_literally() {
        let mut lib = library();
        lib.import_syx(BANK, "bank", 1).unwrap();
        let ids: Vec<i64> = lib
            .list(&Filter::default())
            .unwrap()
            .iter()
            .map(|r| r.id)
            .collect();
        lib.add_tag(ids[0], "100%/a").unwrap();
        lib.add_tag(ids[1], "1000/a").unwrap();
        lib.add_tag(ids[2], "a_b/x").unwrap();
        lib.add_tag(ids[3], "aXb/x").unwrap();
        let count = |tag: &str| {
            lib.count(&Filter {
                tag: Some(tag.into()),
                ..Filter::default()
            })
            .unwrap()
        };
        assert_eq!(count("100%"), 1);
        assert_eq!(count("a_b"), 1);
    }
}
