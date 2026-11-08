import { describe, expect, it, vi } from 'vitest';
import { SystemSaveFlow, SYSTEM_ARMING_LIFETIME_MS } from './systemSave.svelte';

const report = (verified = true) => ({ messages: 3, bytes: 120, verified });

function backend(over: Partial<ConstructorParameters<typeof SystemSaveFlow>[0]> = {}) {
  return {
    armSystemSave: vi.fn(async () => ({ backup_path: 'C:/backups/1-system.syx' })),
    disarmSystemSave: vi.fn(async () => {}),
    commitSystemSave: vi.fn(async () => report()),
    restoreSystemSave: vi.fn(async () => report()),
    ...over,
  };
}

describe('writing the System settings', () => {
  it('walks arm, commit, done and remembers the backup for an undo', async () => {
    const b = backend();
    const f = new SystemSaveFlow(b);
    expect(await f.commit()).toBe(false); // nothing armed yet
    expect(b.commitSystemSave).not.toHaveBeenCalled();
    expect(await f.arm(new Uint8Array([1, 2, 3]))).toBe(true);
    expect(f.phase).toBe('armed');
    expect(await f.commit()).toBe(true);
    expect(f.phase).toBe('done');
    expect(b.commitSystemSave).toHaveBeenCalledWith(new Uint8Array([1, 2, 3]));
    expect(f.canUndo).toBe(true);
    expect(f.lastBackupPath).toBe('C:/backups/1-system.syx');
  });

  it('writes the settings frozen at arming, not later edits of the same buffer', async () => {
    const b = backend();
    const f = new SystemSaveFlow(b);
    const live = new Uint8Array([5, 5, 5]);
    await f.arm(live);
    live[0] = 99;
    await f.commit();
    expect(b.commitSystemSave).toHaveBeenCalledWith(new Uint8Array([5, 5, 5]));
  });

  it('one arming allows one write', async () => {
    const b = backend();
    const f = new SystemSaveFlow(b);
    await f.arm(new Uint8Array([1]));
    await f.commit();
    expect(await f.commit()).toBe(false);
    expect(b.commitSystemSave).toHaveBeenCalledTimes(1);
  });

  it('cancelling disarms and writes nothing', async () => {
    const b = backend();
    const f = new SystemSaveFlow(b);
    await f.arm(new Uint8Array([1]));
    await f.cancel();
    expect(f.phase).toBe('idle');
    expect(b.disarmSystemSave).toHaveBeenCalledTimes(1);
    expect(await f.commit()).toBe(false);
    expect(b.commitSystemSave).not.toHaveBeenCalled();
  });

  it('closing while the settings are still being read disarms as soon as arming finishes', async () => {
    let release: (v: { backup_path: string }) => void = () => {};
    const b = backend({ armSystemSave: vi.fn(() => new Promise<{ backup_path: string }>((r) => (release = r))) });
    const f = new SystemSaveFlow(b);
    const arming = f.arm(new Uint8Array([1]));
    await f.cancel();
    release({ backup_path: 'x' });
    expect(await arming).toBe(false);
    expect(b.disarmSystemSave).toHaveBeenCalledTimes(1);
    expect(f.phase).toBe('idle');
  });

  it('refuses an arming that has expired', async () => {
    let t = 0;
    const b = backend();
    const f = new SystemSaveFlow(b, () => t);
    await f.arm(new Uint8Array([1]));
    t = SYSTEM_ARMING_LIFETIME_MS + 1;
    expect(await f.commit()).toBe(false);
    expect(f.phase).toBe('failed');
    expect(f.error).toMatch(/expired/);
    expect(b.commitSystemSave).not.toHaveBeenCalled();
  });

  it('reports a mismatch and keeps the way back', async () => {
    const f = new SystemSaveFlow(backend({ commitSystemSave: vi.fn(async () => report(false)) }));
    await f.arm(new Uint8Array([1]));
    expect(await f.commit()).toBe(false);
    expect(f.phase).toBe('failed');
    expect(f.error).toMatch(/MISMATCH/);
    expect(f.canUndo).toBe(true);
  });

  it('keeps the way back when the write fails part way, and shows the instrument error', async () => {
    const f = new SystemSaveFlow(backend({ commitSystemSave: vi.fn(async () => Promise.reject('timed out')) }));
    await f.arm(new Uint8Array([1]));
    expect(await f.commit()).toBe(false);
    expect(f.error).toMatch(/timed out/i);
    expect(f.canUndo).toBe(true);
  });

  it('a failed arming writes nothing and shows why', async () => {
    const b = backend({ armSystemSave: vi.fn(async () => Promise.reject(new Error('could not write the backup file'))) });
    const f = new SystemSaveFlow(b);
    expect(await f.arm(new Uint8Array([1]))).toBe(false);
    expect(f.error).toMatch(/backup/);
    expect(f.canUndo).toBe(false);
  });

  it('undoes the last write once', async () => {
    const b = backend();
    const f = new SystemSaveFlow(b);
    expect(await f.undo()).toBe(false);
    await f.arm(new Uint8Array([1]));
    await f.commit();
    expect(await f.undo()).toBe(true);
    expect(b.restoreSystemSave).toHaveBeenCalledTimes(1);
    expect(f.canUndo).toBe(false);
    expect(await f.undo()).toBe(false);
  });

  it('shows why an undo was refused', async () => {
    const f = new SystemSaveFlow(backend({ restoreSystemSave: vi.fn(async () => Promise.reject('the System settings no longer hold what was saved')) }));
    await f.arm(new Uint8Array([1]));
    await f.commit();
    expect(await f.undo()).toBe(false);
    expect(f.error).toMatch(/Undo failed: the System settings no longer hold/i);
  });
});
