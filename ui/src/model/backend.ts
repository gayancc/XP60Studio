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
  /** Only the patches of this bulk, in their order there (null: all). */
  bulk: number | null;
  limit: number;
  offset: number;
}

export interface ImportResult {
  added: number;
  duplicates: number;
  /** The bulk the file became. */
  bulk_id: number;
  bulk_name: string;
}

/** A named, ordered set of up to 128 patches, as a bank file is. */
export interface BulkRow {
  id: number;
  name: string;
  source: string;
  created_at: number;
  updated_at: number;
  count: number;
}

/** One patch of a bulk and where it sits (0 to 127). */
export interface BulkSlot {
  position: number;
  patch_id: number;
  name: string;
}

/** A search result: a patch and the bulk it is in. */
export interface BulkHit {
  bulk_id: number;
  bulk_name: string;
  position: number;
  patch_id: number;
  name: string;
}

/** A kept earlier version of a bulk. */
export interface BulkRevision {
  id: number;
  saved_at: number;
  count: number;
}

export interface BulkSaveResult {
  bulk_id: number;
  name: string;
  added: number;
  reused: number;
  kept_version: boolean;
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
  bulkList(): Promise<BulkRow[]>;
  bulkContents(id: number): Promise<BulkSlot[]>;
  /** Every patch of a bulk as `[position, 590 bytes]`, to load it into the bank. */
  bulkBlocks(id: number): Promise<Array<[number, Uint8Array]>>;
  bulkSearch(text: string, limit: number): Promise<BulkHit[]>;
  bulkRevisions(id: number): Promise<BulkRevision[]>;
  /** Save the bank's patches as a new bulk (`overwrite` null) or over an existing one, whose old contents are kept as a version. */
  bulkSave(overwrite: number | null, name: string, patches: Array<[number, Uint8Array]>): Promise<BulkSaveResult>;
  bulkRename(id: number, name: string): Promise<void>;
  bulkDuplicate(id: number, name: string): Promise<number>;
  /** Delete a bulk; resolves to how many patches went with it. */
  bulkDelete(id: number): Promise<number>;
  bulkPlace(bulk: number, position: number, patchId: number): Promise<void>;
  bulkAppend(bulk: number, patchId: number): Promise<number>;
  bulkClear(bulk: number, position: number): Promise<void>;
  bulkSwap(bulk: number, a: number, b: number): Promise<void>;
  bulkMove(fromBulk: number, fromPosition: number, toBulk: number, toPosition: number): Promise<void>;
  bulkRestore(bulk: number, revision: number): Promise<void>;
  bulkExport(id: number, device: number): Promise<Uint8Array>;
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
  bulk: f.bulk,
});

const toBytes = (rows: Array<[number, number[]]>): Array<[number, Uint8Array]> => rows.map(([p, b]) => [p, new Uint8Array(b)]);

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
  bulkList: () => invoke('bulk_list'),
  bulkContents: (id) => invoke('bulk_contents', { id }),
  bulkBlocks: async (id) => toBytes(await invoke<Array<[number, number[]]>>('bulk_blocks', { id })),
  bulkSearch: (text, limit) => invoke('bulk_search', { text, limit }),
  bulkRevisions: (id) => invoke('bulk_revisions', { id }),
  bulkSave: (overwrite, name, patches) =>
    invoke('bulk_save', { overwrite, name, patches: patches.map(([slot, bytes]) => [slot, Array.from(bytes)]) }),
  bulkRename: (id, name) => invoke('bulk_rename', { id, name }),
  bulkDuplicate: (id, name) => invoke('bulk_duplicate', { id, name }),
  bulkDelete: (id) => invoke('bulk_delete', { id }),
  bulkPlace: (bulk, position, patchId) => invoke('bulk_place', { bulk, position, patchId }),
  bulkAppend: (bulk, patchId) => invoke('bulk_append', { bulk, patchId }),
  bulkClear: (bulk, position) => invoke('bulk_clear', { bulk, position }),
  bulkSwap: (bulk, a, b) => invoke('bulk_swap', { bulk, a, b }),
  bulkMove: (fromBulk, fromPosition, toBulk, toPosition) => invoke('bulk_move', { fromBulk, fromPosition, toBulk, toPosition }),
  bulkRestore: (bulk, revision) => invoke('bulk_restore', { bulk, revision }),
  bulkExport: async (id, device) => new Uint8Array(await invoke<number[]>('bulk_export', { id, device })),
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
