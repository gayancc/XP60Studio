//! Bulks: a named, ordered set of up to 128 patches, as a `.syx` bank file is. Patches are still stored once (so the same patch can
//! sit in several bulks); a bulk only says which patch is at which position. Importing a bank file makes a bulk, saving the bank
//! makes or overwrites one, and every whole-bulk change keeps the previous contents as a restorable version.

use crate::{AddOutcome, Library, LibraryError, Result};
use rusqlite::{params, OptionalExtension};
use xppatch_core::Patch;
use xppatch_sysex::message::DeviceId;

/// Positions in a bulk, as slots in a bank.
pub const BULK_SLOTS: usize = 128;
/// Longest bulk name, in characters.
pub const MAX_BULK_NAME: usize = 60;
/// Versions kept per bulk; older ones are dropped.
pub const MAX_REVISIONS: i64 = 10;
/// Edits to a bulk within this many seconds of its last kept version do not add another one.
const REVISION_THROTTLE_SECS: i64 = 60;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BulkRow {
    pub id: i64,
    pub name: String,
    pub source: String,
    pub created_at: i64,
    pub updated_at: i64,
    pub count: u32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BulkSlot {
    pub position: u8,
    pub patch_id: i64,
    pub name: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BulkHit {
    pub bulk_id: i64,
    pub bulk_name: String,
    pub position: u8,
    pub patch_id: i64,
    pub name: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Revision {
    pub id: i64,
    pub saved_at: i64,
    pub count: u32,
}

/// Where a whole-bank save goes.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SaveTarget {
    /// A new bulk; the name must not be taken.
    New,
    /// Replace the contents of this bulk (the previous contents are kept as a version).
    Overwrite(i64),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SaveReport {
    pub bulk_id: i64,
    pub name: String,
    pub added: usize,
    pub reused: usize,
    pub kept_version: bool,
}

/// A bulk name as stored: trimmed, 1 to [`MAX_BULK_NAME`] characters, no control characters.
pub fn clean_name(name: &str) -> Result<String> {
    let name = name.trim();
    if name.is_empty() || name.chars().count() > MAX_BULK_NAME || name.chars().any(char::is_control)
    {
        return Err(LibraryError::BadBulkName);
    }
    Ok(name.to_owned())
}

/// The base of a bulk name taken from a file name: the file name without its `.syx` or `.mid` ending.
pub fn name_from_source(source: &str) -> String {
    let lower = source.to_ascii_lowercase();
    let stem = [".syx", ".mid", ".midi"]
        .iter()
        .find(|e| lower.ends_with(**e))
        .map_or(source, |e| &source[..source.len() - e.len()]);
    let stem: String = stem
        .chars()
        .filter(|c| !c.is_control())
        .take(MAX_BULK_NAME)
        .collect();
    let stem = stem.trim();
    if stem.is_empty() {
        "Imported".to_owned()
    } else {
        stem.to_owned()
    }
}

fn position(p: usize) -> Result<u8> {
    u8::try_from(p)
        .ok()
        .filter(|&p| usize::from(p) < BULK_SLOTS)
        .ok_or(LibraryError::BadPosition(p))
}

fn encode_slots(slots: &[(u8, i64)]) -> String {
    slots
        .iter()
        .map(|(p, id)| format!("{p}:{id}"))
        .collect::<Vec<_>>()
        .join(",")
}

fn decode_slots(text: &str) -> Vec<(u8, i64)> {
    text.split(',')
        .filter_map(|pair| {
            let (p, id) = pair.split_once(':')?;
            Some((p.parse().ok()?, id.parse().ok()?))
        })
        .collect()
}

impl Library {
    /// Run `f` as one transaction: all of it or none of it.
    pub(crate) fn in_tx<R>(&mut self, f: impl FnOnce(&mut Library) -> Result<R>) -> Result<R> {
        self.conn.execute_batch("BEGIN")?;
        match f(self) {
            Ok(v) => match self.conn.execute_batch("COMMIT") {
                Ok(()) => Ok(v),
                Err(e) => {
                    let _ = self.conn.execute_batch("ROLLBACK");
                    Err(e.into())
                }
            },
            Err(e) => {
                // The original error matters more than a failed rollback.
                let _ = self.conn.execute_batch("ROLLBACK");
                Err(e)
            }
        }
    }

    fn ensure_bulk(&self, id: i64) -> Result<()> {
        let n: i64 = self.conn.query_row(
            "SELECT COUNT(*) FROM bulk WHERE id = ?1",
            params![id],
            |r| r.get(0),
        )?;
        if n == 0 {
            Err(LibraryError::BulkNotFound(id))
        } else {
            Ok(())
        }
    }

    fn bulk_name_taken(&self, name: &str, except: Option<i64>) -> Result<bool> {
        let n: i64 = self.conn.query_row(
            "SELECT COUNT(*) FROM bulk WHERE name = ?1 COLLATE NOCASE AND id != ?2",
            params![name, except.unwrap_or(-1)],
            |r| r.get(0),
        )?;
        Ok(n > 0)
    }

    /// `base`, or `base (2)`, `base (3)` ... the first name nobody has.
    pub(crate) fn unique_bulk_name(&self, base: &str) -> Result<String> {
        let base = clean_name(base)?;
        let mut n = 1;
        loop {
            let tail = if n == 1 {
                String::new()
            } else {
                format!(" ({n})")
            };
            let keep = MAX_BULK_NAME.saturating_sub(tail.chars().count());
            let candidate = format!("{}{tail}", base.chars().take(keep).collect::<String>());
            if !self.bulk_name_taken(&candidate, None)? {
                return Ok(candidate);
            }
            n += 1;
        }
    }

    fn touch(&self, bulk: i64, now: i64) -> Result<()> {
        self.conn.execute(
            "UPDATE bulk SET updated_at = ?2 WHERE id = ?1",
            params![bulk, now],
        )?;
        Ok(())
    }

    fn slot_map(&self, bulk: i64) -> Result<Vec<(u8, i64)>> {
        let mut stmt = self.conn.prepare(
            "SELECT position, patch_id FROM bulk_patch WHERE bulk_id = ?1 ORDER BY position",
        )?;
        let rows = stmt
            .query_map(params![bulk], |r| {
                Ok((r.get::<_, u8>(0)?, r.get::<_, i64>(1)?))
            })?
            .collect::<std::result::Result<_, _>>()?;
        Ok(rows)
    }

    fn write_slot_map(&self, bulk: i64, slots: &[(u8, i64)]) -> Result<()> {
        self.conn
            .execute("DELETE FROM bulk_patch WHERE bulk_id = ?1", params![bulk])?;
        let mut stmt = self
            .conn
            .prepare("INSERT INTO bulk_patch (bulk_id, position, patch_id) VALUES (?1, ?2, ?3)")?;
        for (p, id) in slots {
            stmt.execute(params![bulk, p, id])?;
        }
        Ok(())
    }

    /// Keep the current contents as a version. With `force` false this is skipped when a version was kept moments ago, so a run of
    /// small edits makes one version, not dozens. Empty bulks keep nothing. Returns whether a version was kept.
    fn keep_version(&self, bulk: i64, now: i64, force: bool) -> Result<bool> {
        let slots = self.slot_map(bulk)?;
        if slots.is_empty() {
            return Ok(false);
        }
        let text = encode_slots(&slots);
        let last: Option<(i64, String)> = self
            .conn
            .query_row(
                "SELECT saved_at, slots FROM bulk_revision WHERE bulk_id = ?1 ORDER BY id DESC LIMIT 1",
                params![bulk],
                |r| Ok((r.get(0)?, r.get(1)?)),
            )
            .optional()?;
        if let Some((at, same)) = &last {
            if *same == text || (!force && now - at < REVISION_THROTTLE_SECS) {
                return Ok(false);
            }
        }
        self.conn.execute(
            "INSERT INTO bulk_revision (bulk_id, saved_at, slots) VALUES (?1, ?2, ?3)",
            params![bulk, now, text],
        )?;
        self.conn.execute(
            "DELETE FROM bulk_revision WHERE bulk_id = ?1 AND id NOT IN
             (SELECT id FROM bulk_revision WHERE bulk_id = ?1 ORDER BY id DESC LIMIT ?2)",
            params![bulk, MAX_REVISIONS],
        )?;
        Ok(true)
    }

    // ---- reading ----

    pub fn list_bulks(&self) -> Result<Vec<BulkRow>> {
        let mut stmt = self.conn.prepare(
            "SELECT b.id, b.name, b.source, b.created_at, b.updated_at,
                    (SELECT COUNT(*) FROM bulk_patch bp WHERE bp.bulk_id = b.id)
             FROM bulk b ORDER BY b.name COLLATE NOCASE, b.id",
        )?;
        let rows = stmt
            .query_map([], |r| {
                Ok(BulkRow {
                    id: r.get(0)?,
                    name: r.get(1)?,
                    source: r.get(2)?,
                    created_at: r.get(3)?,
                    updated_at: r.get(4)?,
                    count: r.get(5)?,
                })
            })?
            .collect::<std::result::Result<_, _>>()?;
        Ok(rows)
    }

    /// The patches of a bulk with their positions, in position order.
    pub fn bulk_contents(&self, bulk: i64) -> Result<Vec<BulkSlot>> {
        self.ensure_bulk(bulk)?;
        let mut stmt = self.conn.prepare(
            "SELECT bp.position, bp.patch_id, p.name FROM bulk_patch bp
             JOIN patch p ON p.id = bp.patch_id WHERE bp.bulk_id = ?1 ORDER BY bp.position",
        )?;
        let rows = stmt
            .query_map(params![bulk], |r| {
                Ok(BulkSlot {
                    position: r.get(0)?,
                    patch_id: r.get(1)?,
                    name: r.get(2)?,
                })
            })?
            .collect::<std::result::Result<_, _>>()?;
        Ok(rows)
    }

    /// Every patch of a bulk as `(position, 590 bytes of blocks)`, for loading the bulk into the bank in one go.
    pub fn bulk_blocks(&self, bulk: i64) -> Result<Vec<(u8, Vec<u8>)>> {
        self.ensure_bulk(bulk)?;
        let mut stmt = self.conn.prepare(
            "SELECT bp.position, p.bytes FROM bulk_patch bp
             JOIN patch p ON p.id = bp.patch_id WHERE bp.bulk_id = ?1 ORDER BY bp.position",
        )?;
        let rows = stmt
            .query_map(params![bulk], |r| Ok((r.get(0)?, r.get(1)?)))?
            .collect::<std::result::Result<_, _>>()?;
        Ok(rows)
    }

    /// Patches in any bulk whose name contains `text`, with the bulk they are in; at most `limit`.
    pub fn bulk_search(&self, text: &str, limit: u32) -> Result<Vec<BulkHit>> {
        let pattern = crate::like_pattern(text);
        let mut stmt = self.conn.prepare(
            "SELECT b.id, b.name, bp.position, bp.patch_id, p.name
             FROM bulk_patch bp JOIN bulk b ON b.id = bp.bulk_id JOIN patch p ON p.id = bp.patch_id
             WHERE p.name LIKE ?1 ESCAPE '\\'
             ORDER BY b.name COLLATE NOCASE, bp.position LIMIT ?2",
        )?;
        let rows = stmt
            .query_map(params![pattern, limit.min(2000)], |r| {
                Ok(BulkHit {
                    bulk_id: r.get(0)?,
                    bulk_name: r.get(1)?,
                    position: r.get(2)?,
                    patch_id: r.get(3)?,
                    name: r.get(4)?,
                })
            })?
            .collect::<std::result::Result<_, _>>()?;
        Ok(rows)
    }

    /// Versions of a bulk, newest first.
    pub fn bulk_revisions(&self, bulk: i64) -> Result<Vec<Revision>> {
        self.ensure_bulk(bulk)?;
        let mut stmt = self.conn.prepare(
            "SELECT id, saved_at, slots FROM bulk_revision WHERE bulk_id = ?1 ORDER BY id DESC",
        )?;
        let rows = stmt
            .query_map(params![bulk], |r| {
                let slots: String = r.get(2)?;
                Ok(Revision {
                    id: r.get(0)?,
                    saved_at: r.get(1)?,
                    count: u32::try_from(decode_slots(&slots).len()).unwrap_or(0),
                })
            })?
            .collect::<std::result::Result<_, _>>()?;
        Ok(rows)
    }

    /// The bulk as a User Patch bank `.syx`, each patch at its own position (gaps stay empty).
    pub fn export_bulk(&self, bulk: i64, device: u8) -> Result<Vec<u8>> {
        let mut patches = Vec::new();
        for slot in self.bulk_contents(bulk)? {
            patches.push((slot.position, self.get_patch(slot.patch_id)?));
        }
        Ok(xppatch_core::user_patch_bank_bytes(
            &patches,
            DeviceId::new(device)?,
            Some(128),
        )?)
    }

    // ---- making and changing bulks ----

    /// A new bulk from patches already in the library. `name` must be free; positions must be distinct and below 128.
    pub fn create_bulk(
        &mut self,
        name: &str,
        source: &str,
        slots: &[(u8, i64)],
        now: i64,
    ) -> Result<i64> {
        let name = clean_name(name)?;
        self.in_tx(|lib| lib.create_bulk_inner(&name, source, slots, now))
    }

    pub(crate) fn create_bulk_inner(
        &mut self,
        name: &str,
        source: &str,
        slots: &[(u8, i64)],
        now: i64,
    ) -> Result<i64> {
        if self.bulk_name_taken(name, None)? {
            return Err(LibraryError::BulkNameTaken(name.to_owned()));
        }
        let mut seen = [false; BULK_SLOTS];
        for (p, id) in slots {
            position(usize::from(*p))?;
            if std::mem::replace(&mut seen[usize::from(*p)], true) {
                return Err(LibraryError::BadPosition(usize::from(*p)));
            }
            self.ensure(*id)?;
        }
        self.conn.execute(
            "INSERT INTO bulk (name, source, created_at, updated_at) VALUES (?1, ?2, ?3, ?3)",
            params![name, source, now],
        )?;
        let id = self.conn.last_insert_rowid();
        let mut sorted = slots.to_vec();
        sorted.sort_unstable();
        self.write_slot_map(id, &sorted)?;
        Ok(id)
    }

    /// Store the bank's patches (590 bytes of blocks each, identical ones stored once) and make them a new bulk, or replace the
    /// contents of an existing one, which keeps its previous contents as a version. All or nothing.
    pub fn save_bulk(
        &mut self,
        target: SaveTarget,
        name: &str,
        patches: &[(u8, Vec<u8>)],
        now: i64,
    ) -> Result<SaveReport> {
        let name = clean_name(name)?;
        self.in_tx(|lib| {
            let (mut added, mut reused) = (0, 0);
            let mut slots = Vec::with_capacity(patches.len());
            for (slot, bytes) in patches {
                position(usize::from(*slot))?;
                let patch = Patch::from_blocks(bytes)?;
                match lib.add_patch(&patch, "bank", now)? {
                    AddOutcome::Added(id) => {
                        added += 1;
                        slots.push((*slot, id));
                    }
                    AddOutcome::Duplicate(id) => {
                        reused += 1;
                        slots.push((*slot, id));
                    }
                }
            }
            match target {
                SaveTarget::New => {
                    let bulk_id = lib.create_bulk_inner(&name, "bank", &slots, now)?;
                    Ok(SaveReport {
                        bulk_id,
                        name,
                        added,
                        reused,
                        kept_version: false,
                    })
                }
                SaveTarget::Overwrite(bulk_id) => {
                    lib.ensure_bulk(bulk_id)?;
                    if lib.bulk_name_taken(&name, Some(bulk_id))? {
                        return Err(LibraryError::BulkNameTaken(name));
                    }
                    let kept_version = lib.keep_version(bulk_id, now, true)?;
                    slots.sort_unstable();
                    slots.dedup_by_key(|(p, _)| *p);
                    lib.write_slot_map(bulk_id, &slots)?;
                    lib.conn.execute(
                        "UPDATE bulk SET name = ?2, updated_at = ?3 WHERE id = ?1",
                        params![bulk_id, name, now],
                    )?;
                    Ok(SaveReport {
                        bulk_id,
                        name,
                        added,
                        reused,
                        kept_version,
                    })
                }
            }
        })
    }

    pub fn rename_bulk(&mut self, bulk: i64, name: &str, now: i64) -> Result<()> {
        let name = clean_name(name)?;
        self.ensure_bulk(bulk)?;
        if self.bulk_name_taken(&name, Some(bulk))? {
            return Err(LibraryError::BulkNameTaken(name));
        }
        self.conn.execute(
            "UPDATE bulk SET name = ?2, updated_at = ?3 WHERE id = ?1",
            params![bulk, name, now],
        )?;
        Ok(())
    }

    /// Copy a bulk under a new name. Returns the new bulk's id.
    pub fn duplicate_bulk(&mut self, bulk: i64, name: &str, now: i64) -> Result<i64> {
        let name = clean_name(name)?;
        self.ensure_bulk(bulk)?;
        let slots = self.slot_map(bulk)?;
        self.in_tx(|lib| lib.create_bulk_inner(&name, "copy", &slots, now))
    }

    /// Delete a bulk. A patch that was only in this bulk (not favourited, not tagged) is deleted with it; the rest stay. Returns
    /// how many patches were deleted.
    pub fn delete_bulk(&mut self, bulk: i64) -> Result<usize> {
        self.ensure_bulk(bulk)?;
        let ids: Vec<i64> = self.slot_map(bulk)?.into_iter().map(|(_, id)| id).collect();
        self.in_tx(|lib| {
            lib.conn
                .execute("DELETE FROM bulk WHERE id = ?1", params![bulk])?;
            let mut removed = 0;
            for id in ids {
                removed += lib.conn.execute(
                    "DELETE FROM patch WHERE id = ?1 AND favourite = 0
                     AND NOT EXISTS (SELECT 1 FROM bulk_patch WHERE patch_id = ?1)
                     AND NOT EXISTS (SELECT 1 FROM tag WHERE patch_id = ?1)",
                    params![id],
                )?;
            }
            Ok(removed)
        })
    }

    /// Put an existing patch at a position (replacing what was there).
    pub fn bulk_place(&mut self, bulk: i64, pos: u8, patch: i64, now: i64) -> Result<()> {
        position(usize::from(pos))?;
        self.ensure_bulk(bulk)?;
        self.ensure(patch)?;
        self.in_tx(|lib| {
            lib.keep_version(bulk, now, false)?;
            lib.conn.execute(
                "INSERT OR REPLACE INTO bulk_patch (bulk_id, position, patch_id) VALUES (?1, ?2, ?3)",
                params![bulk, pos, patch],
            )?;
            lib.touch(bulk, now)
        })
    }

    /// Put an existing patch at the first free position. Returns the position.
    pub fn bulk_append(&mut self, bulk: i64, patch: i64, now: i64) -> Result<u8> {
        self.ensure_bulk(bulk)?;
        let used: Vec<u8> = self.slot_map(bulk)?.into_iter().map(|(p, _)| p).collect();
        let free = (0..BULK_SLOTS)
            .find(|p| !used.contains(&u8::try_from(*p).unwrap_or(u8::MAX)))
            .ok_or(LibraryError::BulkFull)?;
        let pos = position(free)?;
        self.bulk_place(bulk, pos, patch, now)?;
        Ok(pos)
    }

    /// Empty a position.
    pub fn bulk_clear(&mut self, bulk: i64, pos: u8, now: i64) -> Result<()> {
        self.ensure_bulk(bulk)?;
        self.in_tx(|lib| {
            lib.keep_version(bulk, now, false)?;
            lib.conn.execute(
                "DELETE FROM bulk_patch WHERE bulk_id = ?1 AND position = ?2",
                params![bulk, pos],
            )?;
            lib.touch(bulk, now)
        })
    }

    fn patch_at(&self, bulk: i64, pos: u8) -> Result<Option<i64>> {
        Ok(self
            .conn
            .query_row(
                "SELECT patch_id FROM bulk_patch WHERE bulk_id = ?1 AND position = ?2",
                params![bulk, pos],
                |r| r.get(0),
            )
            .optional()?)
    }

    /// Exchange two positions of one bulk (either may be empty).
    pub fn bulk_swap(&mut self, bulk: i64, a: u8, b: u8, now: i64) -> Result<()> {
        position(usize::from(a))?;
        position(usize::from(b))?;
        self.ensure_bulk(bulk)?;
        if a == b {
            return Ok(());
        }
        self.in_tx(|lib| {
            lib.keep_version(bulk, now, false)?;
            let (pa, pb) = (lib.patch_at(bulk, a)?, lib.patch_at(bulk, b)?);
            lib.conn.execute(
                "DELETE FROM bulk_patch WHERE bulk_id = ?1 AND position IN (?2, ?3)",
                params![bulk, a, b],
            )?;
            for (pos, patch) in [(a, pb), (b, pa)] {
                if let Some(id) = patch {
                    lib.conn.execute(
                        "INSERT INTO bulk_patch (bulk_id, position, patch_id) VALUES (?1, ?2, ?3)",
                        params![bulk, pos, id],
                    )?;
                }
            }
            lib.touch(bulk, now)
        })
    }

    /// Move a patch from one bulk position to another. Within one bulk, or onto an occupied position, the two exchange places.
    pub fn bulk_move(&mut self, from: (i64, u8), to: (i64, u8), now: i64) -> Result<()> {
        if from.0 == to.0 {
            return self.bulk_swap(from.0, from.1, to.1, now);
        }
        position(usize::from(from.1))?;
        position(usize::from(to.1))?;
        self.ensure_bulk(from.0)?;
        self.ensure_bulk(to.0)?;
        self.in_tx(|lib| {
            let moving = lib
                .patch_at(from.0, from.1)?
                .ok_or(LibraryError::BadPosition(usize::from(from.1)))?;
            let displaced = lib.patch_at(to.0, to.1)?;
            lib.keep_version(from.0, now, false)?;
            lib.keep_version(to.0, now, false)?;
            lib.conn.execute(
                "INSERT OR REPLACE INTO bulk_patch (bulk_id, position, patch_id) VALUES (?1, ?2, ?3)",
                params![to.0, to.1, moving],
            )?;
            match displaced {
                Some(id) => lib.conn.execute(
                    "INSERT OR REPLACE INTO bulk_patch (bulk_id, position, patch_id) VALUES (?1, ?2, ?3)",
                    params![from.0, from.1, id],
                )?,
                None => lib.conn.execute(
                    "DELETE FROM bulk_patch WHERE bulk_id = ?1 AND position = ?2",
                    params![from.0, from.1],
                )?,
            };
            lib.touch(from.0, now)?;
            lib.touch(to.0, now)
        })
    }

    /// Put a bulk back to a kept version. The contents it has now are kept as a version first, so a restore can be undone.
    pub fn restore_revision(&mut self, bulk: i64, revision: i64, now: i64) -> Result<()> {
        self.ensure_bulk(bulk)?;
        let text: String = self
            .conn
            .query_row(
                "SELECT slots FROM bulk_revision WHERE id = ?1 AND bulk_id = ?2",
                params![revision, bulk],
                |r| r.get(0),
            )
            .optional()?
            .ok_or(LibraryError::NotFound(revision))?;
        self.in_tx(|lib| {
            lib.keep_version(bulk, now, true)?;
            // A patch deleted since the version was kept cannot come back; its position stays empty.
            let mut slots = Vec::new();
            for (p, id) in decode_slots(&text) {
                if lib.ensure(id).is_ok() {
                    slots.push((p, id));
                }
            }
            lib.write_slot_map(bulk, &slots)?;
            lib.touch(bulk, now)
        })
    }

    /// Give every stored patch that is in no bulk a bulk of its own, grouped by where it came from, in the order stored. Used once,
    /// when a library made before bulks existed is opened.
    pub(crate) fn migrate_to_bulks(&mut self) -> Result<()> {
        let mut stmt = self
            .conn
            .prepare("SELECT id, source, added_at FROM patch ORDER BY id")?;
        let rows: Vec<(i64, String, i64)> = stmt
            .query_map([], |r| Ok((r.get(0)?, r.get(1)?, r.get(2)?)))?
            .collect::<std::result::Result<_, _>>()?;
        drop(stmt);
        let mut groups: Vec<(String, Vec<(i64, i64)>)> = Vec::new();
        for (id, source, at) in rows {
            match groups.iter_mut().find(|(s, _)| *s == source) {
                Some((_, v)) => v.push((id, at)),
                None => groups.push((source, vec![(id, at)])),
            }
        }
        self.in_tx(|lib| {
            for (source, members) in groups {
                for (n, chunk) in members.chunks(BULK_SLOTS).enumerate() {
                    let base = name_from_source(&source);
                    let base = if n == 0 {
                        base
                    } else {
                        format!("{base} ({})", n + 1)
                    };
                    let name = lib.unique_bulk_name(&base)?;
                    let at = chunk.iter().map(|(_, a)| *a).min().unwrap_or(0);
                    let slots: Vec<(u8, i64)> = chunk
                        .iter()
                        .enumerate()
                        .map(|(i, (id, _))| (u8::try_from(i).unwrap_or(0), *id))
                        .collect();
                    lib.create_bulk_inner(&name, &source, &slots, at)?;
                }
            }
            Ok(())
        })
    }
}

#[cfg(test)]
#[path = "bulk_tests.rs"]
mod tests;
