// Writing the System settings into the instrument, which replaces them for good. It is the same small state machine as the
// patch save (save.svelte.ts), with no slot to choose: the settings are read and backed up to a file first, one arming
// allows exactly one write, and the settings written are the ones frozen when the save was armed.
//
//   idle -> arming -> armed (settings read, old ones backed up to disk) -> writing -> done | failed

import type { ArmedSystemSave, Backend, SystemSaveReport } from './backend';
import type { SavePhase } from './save.svelte';
import { reportError, reportSuccess, reportWarning } from './messages.svelte';

/** How long an arming stays valid (the backend enforces the same limit). */
export const SYSTEM_ARMING_LIFETIME_MS = 60_000;

function message(e: unknown): string {
  return typeof e === 'string' ? e : e instanceof Error ? e.message : String(e);
}

export class SystemSaveFlow {
  phase = $state<SavePhase>('idle');
  armed = $state<ArmedSystemSave | null>(null);
  report = $state<SystemSaveReport | null>(null);
  #error = $state('');
  get error(): string {
    return this.#error;
  }
  /** Setting text here also shows it in the bottom-left corner with every other message. */
  set error(text: string) {
    this.#error = text;
    if (text) reportError(text);
  }
  /** Where the settings the last save replaced were backed up, while that save can still be undone. */
  lastBackupPath = $state<string | null>(null);

  private frozen: Uint8Array | null = null;
  private armedAt = 0;
  private cancelWhenArmed = false;

  constructor(
    private readonly backend: Pick<Backend, 'armSystemSave' | 'disarmSystemSave' | 'commitSystemSave' | 'restoreSystemSave'>,
    private readonly now: () => number = () => Date.now(),
  ) {}

  get canUndo(): boolean {
    return this.lastBackupPath !== null;
  }

  get working(): boolean {
    return this.phase === 'arming' || this.phase === 'writing';
  }

  /** Step 1: read the settings and back them up to a file. `blocks` are the settings to write, frozen now. */
  async arm(blocks: Uint8Array): Promise<boolean> {
    if (this.phase === 'arming' || this.phase === 'armed' || this.phase === 'writing') return false;
    this.frozen = blocks.slice();
    this.phase = 'arming';
    this.error = '';
    this.report = null;
    this.cancelWhenArmed = false;
    try {
      this.armed = await this.backend.armSystemSave();
      this.armedAt = this.now();
      this.phase = 'armed';
      if (this.cancelWhenArmed) {
        await this.cancel();
        return false;
      }
      return true;
    } catch (e) {
      this.armed = null;
      this.frozen = null;
      this.fail(message(e));
      return false;
    }
  }

  /** Walk away from an armed save without writing. Safe in any phase except while writing. */
  async cancel(): Promise<void> {
    if (this.phase === 'arming') {
      this.cancelWhenArmed = true;
      return;
    }
    if (this.phase !== 'armed') return;
    // Leave the armed phase first so a click on Replace cannot race the disarm.
    this.phase = 'idle';
    this.armed = null;
    this.frozen = null;
    try {
      await this.backend.disarmSystemSave();
    } catch {
      // The next arm replaces the arming anyway.
    }
  }

  /** Step 2: write the frozen settings and read them back. Only possible while armed. */
  async commit(): Promise<boolean> {
    const blocks = this.frozen;
    if (this.phase !== 'armed' || !blocks) return false;
    if (this.now() - this.armedAt > SYSTEM_ARMING_LIFETIME_MS) {
      await this.cancel();
      this.fail('The arming expired. Arm the save again.');
      return false;
    }
    const backup = this.armed?.backup_path ?? null;
    this.frozen = null;
    this.phase = 'writing';
    try {
      const report = await this.backend.commitSystemSave(blocks);
      this.report = report;
      this.armed = null;
      this.lastBackupPath = backup;
      if (!report.verified) {
        this.protectAdvice();
        this.fail('The settings were written but reading them back did not match (MISMATCH).');
        return false;
      }
      this.phase = 'done';
      reportSuccess('System settings written: read back and they match.');
      return true;
    } catch (e) {
      // A write may have started, so the earlier settings can still be put back from the backup.
      this.armed = null;
      this.lastBackupPath = backup;
      this.protectAdvice();
      this.fail(message(e));
      return false;
    }
  }

  /** Put back what the last save replaced. The caller confirms first; this is a permanent write too. */
  async undo(): Promise<boolean> {
    if (!this.lastBackupPath || this.working || this.phase === 'armed') return false;
    this.phase = 'writing';
    try {
      const report = await this.backend.restoreSystemSave();
      this.report = report;
      this.lastBackupPath = null;
      this.error = '';
      if (!report.verified) {
        this.fail('The old settings were written back but reading them back did not match (MISMATCH).');
        return false;
      }
      this.phase = 'done';
      reportSuccess('The earlier System settings are back in the instrument.');
      return true;
    } catch (e) {
      this.fail(`Undo failed: ${message(e)}`);
      return false;
    }
  }

  reset(): void {
    if (this.working || this.phase === 'armed') return;
    this.phase = 'idle';
    this.error = '';
    this.report = null;
  }

  /** A write that fails is often User Memory Protect: say so alongside the error. */
  private protectAdvice(): void {
    reportWarning('If User Memory Protect is on, turn it off on the instrument and try again.');
  }

  private fail(text: string): void {
    this.error = text;
    this.phase = 'failed';
  }
}
