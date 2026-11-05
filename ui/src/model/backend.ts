// The calls the library and the save ceremony make into Rust. One interface so the logic above it is tested without
// Tauri; `tauriBackend` is the real implementation.

import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';

export interface LibraryRow {
  id: number;
  name: string;
  tags: string[];
  favourite: boolean;
  source: string;
  added_at: number;
}

export interface LibraryFilter {
  text: string;
  tag: string;
  favouritesOnly: boolean;
  limit: number;
  offset: number;
}

export interface ImportResult {
  added: number;
  duplicates: number;
}

export interface ArmedSave {
  slot: number;
  occupant_name: string;
  backup_path: string;
}

export interface SaveReport {
  slot: number;
  messages: number;
  bytes: number;
  verified: boolean;
}

export interface ArmedSystemSave {
  backup_path: string;
}

export interface SystemSaveReport {
  messages: number;
  bytes: number;
  verified: boolean;
}

export interface ArmedBank {
  backup_path: string;
  names: string[];
}

export interface BankSaveReport {
  written: number[];
  stopped_at: [number, string] | null;
  cancelled: boolean;
}

export interface Progress {
  what: string;
  done: number;
  total: number;
}

/** One backup file written before a save. */
export interface BackupFile {
  name: string;
  bytes: number;
  /** Seconds since 1970 (0 when unknown). */
  modified: number;
}

/** The storage settings as the desktop app reports them. */
export interface StorageInfo {
  in_use: string;
  configured: string;
  default: string;
  custom: boolean;
}

export interface Backend {
  libraryImport(bytes: Uint8Array, source: string): Promise<ImportResult>;
  libraryList(f: LibraryFilter): Promise<LibraryRow[]>;
  libraryCount(f: LibraryFilter): Promise<number>;
  libraryTags(): Promise<Array<[string, number]>>;
  libraryGet(id: number): Promise<Uint8Array>;
  libraryAdd(patch: Uint8Array, source: string): Promise<[number, boolean]>;
  libraryFavourite(id: number, on: boolean): Promise<void>;
  libraryTagAdd(id: number, tag: string): Promise<void>;
  libraryTagRemove(id: number, tag: string): Promise<void>;
  libraryRename(id: number, name: string): Promise<void>;
  libraryDelete(id: number): Promise<void>;
  libraryExport(ids: number[], device: number): Promise<Uint8Array>;
  /** The backup files written before saves, newest first. */
  backupsList(): Promise<BackupFile[]>;
  /** The bytes of one backup file. */
  backupRead(name: string): Promise<Uint8Array>;
  /** Where the library and backups are kept. */
  storageInfo(): Promise<StorageInfo>;
  /** Choose the folder (null for the default). Copies the library and backups; takes effect at the next start. */
  storageSet(dir: string | null): Promise<StorageInfo>;
  armSave(slot: number): Promise<ArmedSave>;
  disarmSave(): Promise<void>;
  commitSave(patch: Uint8Array): Promise<SaveReport>;
  restoreSave(): Promise<SaveReport>;
  /** Writing the System settings: read and back them up, then write once, with a way back. */
  /** Ask where to save a file (the system Save dialog) and write it; the path, or null when the user cancelled. */
  saveFile(name: string, bytes: Uint8Array): Promise<string | null>;
  armSystemSave(): Promise<ArmedSystemSave>;
  disarmSystemSave(): Promise<void>;
  commitSystemSave(blocks: Uint8Array): Promise<SystemSaveReport>;
  restoreSystemSave(): Promise<SystemSaveReport>;
  readBank(): Promise<Array<[number, Uint8Array]>>;
  /** All of User memory the editor has a map for (patches, performances, rhythm sets) as one `.syx`; nothing is written. */
  backupAll(): Promise<Uint8Array>;
  /** Back up the bank and arm one write of exactly these slots. */
  armBank(slots: number[]): Promise<ArmedBank>;
  disarmBank(): Promise<void>;
  commitBank(patches: Array<[number, Uint8Array]>): Promise<BankSaveReport>;
  bankToSyx(patches: Array<[number, Uint8Array]>, device: number): Promise<Uint8Array>;
  cancelOperation(): Promise<void>;
  /** Progress of a long bank operation. Returns a function that stops listening. */
  onProgress(listener: (p: Progress) => void): Promise<() => void>;
}

const filterArgs = (f: LibraryFilter) => ({
  text: f.text || null,
  tag: f.tag || null,
  favouritesOnly: f.favouritesOnly,
});

export const tauriBackend: Backend = {
  libraryImport: (bytes, source) => invoke('library_import', { bytes: Array.from(bytes), source }),
  libraryList: (f) => invoke('library_list', { ...filterArgs(f), limit: f.limit, offset: f.offset }),
  libraryCount: (f) => invoke('library_count', filterArgs(f)),
  libraryTags: () => invoke('library_tags'),
  libraryGet: async (id) => new Uint8Array(await invoke<number[]>('library_get', { id })),
  libraryAdd: (patch, source) => invoke('library_add', { patch: Array.from(patch), source }),
  libraryFavourite: (id, on) => invoke('library_favourite', { id, on }),
  libraryTagAdd: (id, tag) => invoke('library_tag_add', { id, tag }),
  libraryTagRemove: (id, tag) => invoke('library_tag_remove', { id, tag }),
  libraryRename: (id, name) => invoke('library_rename', { id, name }),
  libraryDelete: (id) => invoke('library_delete', { id }),
  libraryExport: async (ids, device) => new Uint8Array(await invoke<number[]>('library_export', { ids, device })),
  backupsList: () => invoke('backups_list'),
  backupRead: async (name) => new Uint8Array(await invoke<number[]>('backup_read', { name })),
  storageInfo: () => invoke('storage_info'),
  storageSet: (dir) => invoke('storage_set', { dir }),
  armSave: (slot) => invoke('midi_arm_save', { slot }),
  disarmSave: () => invoke('midi_disarm_save'),
  commitSave: (patch) => invoke('midi_commit_save', { patch: Array.from(patch) }),
  restoreSave: () => invoke('midi_restore_save'),
  saveFile: (name, bytes) => invoke('save_file', { name, bytes: Array.from(bytes) }),
  armSystemSave: () => invoke('midi_arm_system_save'),
  disarmSystemSave: () => invoke('midi_disarm_system_save'),
  commitSystemSave: (blocks) => invoke('midi_commit_system_save', { blocks: Array.from(blocks) }),
  restoreSystemSave: () => invoke('midi_restore_system_save'),
  backupAll: async () => new Uint8Array(await invoke<number[]>('midi_backup_all')),
  readBank: async () => {
    const rows = await invoke<Array<[number, number[]]>>('midi_read_bank');
    return rows.map(([slot, bytes]) => [slot, new Uint8Array(bytes)]);
  },
  armBank: (slots) => invoke('midi_arm_bank', { slots }),
  disarmBank: () => invoke('midi_disarm_bank'),
  commitBank: (patches) =>
    invoke('midi_commit_bank', { patches: patches.map(([slot, bytes]) => [slot, Array.from(bytes)]) }),
  bankToSyx: async (patches, device) =>
    new Uint8Array(
      await invoke<number[]>('bank_to_syx', { patches: patches.map(([slot, bytes]) => [slot, Array.from(bytes)]), device }),
    ),
  cancelOperation: () => invoke('midi_cancel'),
  onProgress: (listener) => listen<Progress>('bank-progress', (e) => listener(e.payload)),
};
