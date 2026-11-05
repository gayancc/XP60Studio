//! Where the library and the backup files live (parity row D5). The default is the application data folder; the user may
//! point to another folder. The choice is a small pointer file kept in the default folder, so it is found before the
//! library is opened. Choosing a folder never deletes or overwrites anything: an existing library there is used as it
//! is, and otherwise the current library and backups are *copied* in.

use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};

pub const POINTER_FILE: &str = "storage.json";
pub const LIBRARY_FILE: &str = "library.sqlite";
pub const BACKUP_DIR: &str = "backups";

#[derive(Serialize, Deserialize)]
struct Pointer {
    dir: String,
}

/// What the settings panel shows.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct StorageInfo {
    /// The folder the library and backups are in right now (this run).
    pub in_use: String,
    /// The folder that will be used from the next start.
    pub configured: String,
    pub default: String,
    pub custom: bool,
}

/// A folder is usable when it is absolute, can be created and a file can be written in it.
fn check_writable(dir: &Path) -> Result<(), String> {
    if !dir.is_absolute() {
        return Err("Give the full path of the folder, for example D:\\XP60 data.".into());
    }
    std::fs::create_dir_all(dir).map_err(|e| format!("The folder cannot be created: {e}"))?;
    let probe = dir.join(format!(".xppatch-write-test-{}", std::process::id()));
    std::fs::write(&probe, b"x").map_err(|e| format!("The folder cannot be written to: {e}"))?;
    let _ = std::fs::remove_file(&probe);
    Ok(())
}

/// The folder chosen by the pointer file, or `default` when there is none, it is unreadable, or the folder is unusable.
pub fn resolve(default: &Path) -> PathBuf {
    let Ok(text) = std::fs::read_to_string(default.join(POINTER_FILE)) else {
        return default.to_path_buf();
    };
    match serde_json::from_str::<Pointer>(&text) {
        Ok(p) if Path::new(&p.dir).is_absolute() && Path::new(&p.dir).is_dir() => {
            PathBuf::from(p.dir)
        }
        _ => default.to_path_buf(),
    }
}

pub fn info(default: &Path, in_use: &Path) -> StorageInfo {
    let configured = resolve(default);
    StorageInfo {
        in_use: in_use.display().to_string(),
        custom: configured != default,
        configured: configured.display().to_string(),
        default: default.display().to_string(),
    }
}

/// Copy every file of `src` into `dst` (one level: the backup folder holds flat files). Existing files are kept.
fn copy_files(src: &Path, dst: &Path) -> Result<(), String> {
    std::fs::create_dir_all(dst).map_err(|e| e.to_string())?;
    let Ok(entries) = std::fs::read_dir(src) else {
        return Ok(()); // nothing to copy
    };
    for entry in entries.flatten() {
        let from = entry.path();
        let to = dst.join(entry.file_name());
        if from.is_file() && !to.exists() {
            std::fs::copy(&from, &to)
                .map_err(|e| format!("Copying {} failed: {e}", from.display()))?;
        }
    }
    Ok(())
}

/// Choose `target` (or the default again when `None`). `snapshot` writes a copy of the open library to the given path;
/// it is used only when `target` has no library yet. The backups folder is copied the same way. Returns the folder that
/// will be used from the next start.
pub fn set(
    default: &Path,
    in_use: &Path,
    target: Option<&str>,
    snapshot: impl FnOnce(&Path) -> Result<(), String>,
) -> Result<PathBuf, String> {
    let pointer = default.join(POINTER_FILE);
    let Some(target) = target.map(str::trim).filter(|t| !t.is_empty()) else {
        match std::fs::remove_file(&pointer) {
            Ok(()) => {}
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => {}
            Err(e) => return Err(format!("The setting could not be cleared: {e}")),
        }
        return Ok(default.to_path_buf());
    };
    let dir = PathBuf::from(target);
    check_writable(&dir)?;
    if dir != in_use {
        if !dir.join(LIBRARY_FILE).exists() {
            snapshot(&dir.join(LIBRARY_FILE))?;
        }
        copy_files(&in_use.join(BACKUP_DIR), &dir.join(BACKUP_DIR))?;
    }
    std::fs::create_dir_all(default).map_err(|e| e.to_string())?;
    let json = serde_json::to_string(&Pointer {
        dir: dir.display().to_string(),
    })
    .map_err(|e| e.to_string())?;
    // Written to a temporary file and renamed, so a crash never leaves a half-written pointer.
    let tmp = default.join(format!("{POINTER_FILE}.tmp"));
    std::fs::write(&tmp, json).map_err(|e| format!("The setting could not be saved: {e}"))?;
    std::fs::rename(&tmp, &pointer).map_err(|e| format!("The setting could not be saved: {e}"))?;
    Ok(dir)
}

/// One backup file written before a save.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct BackupFile {
    pub name: String,
    pub bytes: u64,
    /// Seconds since 1970 (0 when the system cannot say).
    pub modified: u64,
}

/// Largest backup file read back (a whole User bank is about 112 KB).
const MAX_BACKUP_BYTES: u64 = 16 * 1024 * 1024;

fn is_backup_name(name: &str) -> bool {
    !name.is_empty()
        && !name.contains(['/', '\\', ':'])
        && !name.starts_with('.')
        && name.to_ascii_lowercase().ends_with(".syx")
}

/// The backup files in `<dir>/backups`, newest first. A folder that does not exist yet has none.
pub fn list_backups(dir: &Path) -> Vec<BackupFile> {
    let Ok(entries) = std::fs::read_dir(dir.join(BACKUP_DIR)) else {
        return Vec::new();
    };
    let mut out: Vec<BackupFile> = entries
        .flatten()
        .filter_map(|e| {
            let name = e.file_name().to_string_lossy().into_owned();
            let meta = e.metadata().ok()?;
            (meta.is_file() && is_backup_name(&name)).then(|| BackupFile {
                name,
                bytes: meta.len(),
                modified: meta
                    .modified()
                    .ok()
                    .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
                    .map_or(0, |d| d.as_secs()),
            })
        })
        .collect();
    out.sort_by(|a, b| {
        b.modified
            .cmp(&a.modified)
            .then_with(|| b.name.cmp(&a.name))
    });
    out
}

/// The bytes of one backup file, by name only: a name with a path in it, or a file that is not a backup, is refused.
pub fn read_backup(dir: &Path, name: &str) -> Result<Vec<u8>, String> {
    if !is_backup_name(name) {
        return Err("That is not a backup file name.".into());
    }
    let path = dir.join(BACKUP_DIR).join(name);
    let meta =
        std::fs::metadata(&path).map_err(|_| "That backup file no longer exists.".to_string())?;
    if !meta.is_file() || meta.len() > MAX_BACKUP_BYTES {
        return Err("That file cannot be read as a backup.".into());
    }
    std::fs::read(&path).map_err(|e| format!("The backup could not be read: {e}"))
}

/// A file name for a file the user is asked to save: plain letters, digits, spaces and `. _ -`, no folders, at most 120
/// characters, and an extension (`syx`, `mid`). Anything else is refused rather than cleaned up, so a name that was
/// never meant to be a path cannot become one.
pub fn safe_export_name(name: &str) -> Result<String, String> {
    let ok_char = |c: char| c.is_ascii_alphanumeric() || matches!(c, ' ' | '.' | '_' | '-');
    let ext_ok = name
        .rsplit_once('.')
        .is_some_and(|(stem, ext)| !stem.is_empty() && matches!(ext, "syx" | "mid"));
    if name.is_empty()
        || name.len() > 120
        || !name.chars().all(ok_char)
        || name.starts_with('.')
        || !ext_ok
    {
        return Err(format!("'{name}' is not a file name this app can save"));
    }
    Ok(name.to_owned())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scratch(name: &str) -> PathBuf {
        let dir =
            std::env::temp_dir().join(format!("xppatch-storage-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).unwrap();
        dir
    }

    fn fake_snapshot(to: &Path) -> Result<(), String> {
        std::fs::write(to, b"library-copy").map_err(|e| e.to_string())
    }

    #[test]
    fn defaults_when_nothing_is_chosen_or_the_pointer_is_bad() {
        let d = scratch("default");
        assert_eq!(resolve(&d), d);
        std::fs::write(d.join(POINTER_FILE), "not json").unwrap();
        assert_eq!(resolve(&d), d);
        std::fs::write(d.join(POINTER_FILE), r#"{"dir":"relative/path"}"#).unwrap();
        assert_eq!(resolve(&d), d);
        std::fs::write(
            d.join(POINTER_FILE),
            r#"{"dir":"Z:\\no\\such\\folder\\anywhere"}"#,
        )
        .unwrap();
        assert_eq!(resolve(&d), d);
    }

    #[test]
    fn choosing_a_folder_copies_the_library_and_backups_and_keeps_the_original() {
        let d = scratch("from");
        std::fs::write(d.join(LIBRARY_FILE), b"original").unwrap();
        std::fs::create_dir_all(d.join(BACKUP_DIR)).unwrap();
        std::fs::write(d.join(BACKUP_DIR).join("b1.syx"), b"backup").unwrap();
        let t = scratch("to");
        let used = set(&d, &d, Some(t.to_str().unwrap()), fake_snapshot).unwrap();
        assert_eq!(used, t);
        assert_eq!(
            std::fs::read(t.join(LIBRARY_FILE)).unwrap(),
            b"library-copy"
        );
        assert_eq!(
            std::fs::read(t.join(BACKUP_DIR).join("b1.syx")).unwrap(),
            b"backup"
        );
        assert_eq!(
            std::fs::read(d.join(LIBRARY_FILE)).unwrap(),
            b"original",
            "the original is untouched"
        );
        assert_eq!(resolve(&d), t);
        let i = info(&d, &d);
        assert!(i.custom);
        assert_eq!(i.configured, t.display().to_string());
        assert_eq!(i.in_use, d.display().to_string());
    }

    #[test]
    fn an_existing_library_in_the_chosen_folder_is_never_overwritten() {
        let d = scratch("keep-from");
        let t = scratch("keep-to");
        std::fs::write(t.join(LIBRARY_FILE), b"theirs").unwrap();
        set(&d, &d, Some(t.to_str().unwrap()), |_| {
            panic!("must not snapshot over an existing library")
        })
        .unwrap();
        assert_eq!(std::fs::read(t.join(LIBRARY_FILE)).unwrap(), b"theirs");
    }

    #[test]
    fn going_back_to_the_default_removes_only_the_pointer() {
        let d = scratch("back");
        let t = scratch("back-to");
        set(&d, &d, Some(t.to_str().unwrap()), fake_snapshot).unwrap();
        assert!(d.join(POINTER_FILE).exists());
        let used = set(&d, &t, None, fake_snapshot).unwrap();
        assert_eq!(used, d);
        assert!(!d.join(POINTER_FILE).exists());
        assert!(
            t.join(LIBRARY_FILE).exists(),
            "the other folder keeps its files"
        );
        assert_eq!(resolve(&d), d);
        // Clearing when nothing is set is fine.
        set(&d, &d, Some("  "), fake_snapshot).unwrap();
    }

    #[test]
    fn refuses_a_relative_path_and_a_folder_that_cannot_be_used() {
        let d = scratch("refuse");
        assert!(set(&d, &d, Some("relative"), fake_snapshot)
            .unwrap_err()
            .contains("full path"));
        // A file where a folder is wanted.
        let f = d.join("afile");
        std::fs::write(&f, b"x").unwrap();
        assert!(set(&d, &d, Some(f.to_str().unwrap()), fake_snapshot).is_err());
        assert!(
            !d.join(POINTER_FILE).exists(),
            "a refused choice leaves no pointer"
        );
    }

    #[test]
    fn a_failed_copy_leaves_the_setting_unchanged() {
        let d = scratch("fail");
        let t = scratch("fail-to");
        let err = set(&d, &d, Some(t.to_str().unwrap()), |_| {
            Err("disk full".into())
        })
        .unwrap_err();
        assert_eq!(err, "disk full");
        assert!(!d.join(POINTER_FILE).exists());
    }

    #[test]
    fn lists_only_syx_backups_newest_first_and_a_missing_folder_has_none() {
        let d = scratch("backups");
        assert!(list_backups(&d).is_empty());
        let b = d.join(BACKUP_DIR);
        std::fs::create_dir_all(&b).unwrap();
        std::fs::write(b.join("a.syx"), [1, 2, 3]).unwrap();
        std::fs::write(b.join("notes.txt"), b"x").unwrap();
        std::fs::write(b.join(".hidden.syx"), b"x").unwrap();
        std::fs::create_dir_all(b.join("sub.syx")).unwrap();
        let got = list_backups(&d);
        assert_eq!(
            got.iter().map(|f| f.name.as_str()).collect::<Vec<_>>(),
            vec!["a.syx"]
        );
        assert_eq!(got[0].bytes, 3);
    }

    #[test]
    fn reads_a_backup_by_name_and_refuses_paths_and_other_files() {
        let d = scratch("read-backup");
        let b = d.join(BACKUP_DIR);
        std::fs::create_dir_all(&b).unwrap();
        std::fs::write(b.join("ok.SYX"), [9, 8]).unwrap();
        std::fs::write(d.join("secret.syx"), b"outside").unwrap();
        assert_eq!(read_backup(&d, "ok.SYX").unwrap(), vec![9, 8]);
        for bad in [
            "../secret.syx",
            "..\\secret.syx",
            "sub/ok.syx",
            "C:ok.syx",
            "",
            ".syx",
            "library.sqlite",
            "ok.txt",
        ] {
            assert!(read_backup(&d, bad).is_err(), "{bad:?} must be refused");
        }
        assert!(read_backup(&d, "missing.syx")
            .unwrap_err()
            .contains("no longer exists"));
    }

    #[test]
    fn export_names_are_plain_file_names_with_a_known_extension() {
        assert_eq!(
            safe_export_name("ALL RIGHT TA.syx").unwrap(),
            "ALL RIGHT TA.syx"
        );
        assert_eq!(
            safe_export_name("xppatch-user-bank.mid").unwrap(),
            "xppatch-user-bank.mid"
        );
        for bad in [
            "",
            ".syx",
            "a.exe",
            "a",
            "../x.syx",
            "a/b.syx",
            "a\\b.syx",
            "C:x.syx",
            "a:b.syx",
            "con\0.syx",
        ] {
            assert!(safe_export_name(bad).is_err(), "{bad:?}");
        }
        assert!(safe_export_name(&format!("{}.syx", "a".repeat(130))).is_err());
    }
}
