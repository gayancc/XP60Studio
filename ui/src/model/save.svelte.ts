// The save ceremony: writing a patch into one of the instrument's User slots, which overwrites what is there for good.
// It is a small state machine so the interface can only ever walk through the safe sequence:
//
//   idle -> arming -> armed (slot read, old patch backed up to disk) -> writing -> done | failed
//
// Nothing here is reachable by editing, selecting or connecting; each step is a deliberate act, one arming allows
// exactly one write, and the patch that is written is the one frozen when the save was armed.

import type { ArmedSave, Backend, SaveReport } from './backend';
import { errorText, humanizeErrorText, reportError, reportSuccess, reportWarning } from './messages.svelte';

export type SavePhase = 'idle' | 'arming' | 'armed' | 'writing' | 'done' | 'failed';

/** How long an arming stays valid (the backend enforces the same limit). */
export const ARMING_LIFETIME_MS = 60_000;

function message(e: unknown): string {
  return errorText(e);
}

/** What the last save did, kept so it can be undone even after another slot has been looked at. */
export interface LastSave {
  slot: number;
  backupPath: string;
  oldName: string;
}

export class SaveFlow {
  phase = $state<SavePhase>('idle');
  slot = $state(0);
  armed = $state<ArmedSave | null>(null);
  report = $state<SaveReport | null>(null);
  #error = $state('');
  get error(): string {
    return this.#error;
  }
  /** Setting text here also shows it in the bottom-left corner with every other message. */
  set error(text: string) {
    const clean = text ? humanizeErrorText(text) : '';
    this.#error = clean;
    if (clean) reportError(clean);
  }
  /** The last save, if it can still be undone. */
  last = $state<LastSave | null>(null);
  /** The patch the user confirmed, frozen when the save was armed: later edits in the editor do not change it. */
  frozenName = $state('');

  private frozen: Uint8Array | null = null;
  private armedAt = 0;
  /** Set when the panel was closed while the slot was still being read: disarm as soon as arming finishes. */
  private cancelWhenArmed = false;

  constructor(
    private readonly backend: Backend,
    private readonly now: () => number = () => Date.now(),
  ) {}

  get canUndo(): boolean {
    return this.last !== null;
  }

  /** True while a request to the instrument is in flight. */
  get working(): boolean {
    return this.phase === 'arming' || this.phase === 'writing';
  }

  /** Step 1: read the chosen slot and back it up to a file. Nothing is written to the instrument. */
  async arm(slot: number, patch: Uint8Array, name: string): Promise<boolean> {
    if (this.phase === 'arming' || this.phase === 'armed' || this.phase === 'writing') return false;
    if (!Number.isInteger(slot) || slot < 0 || slot > 127) {
      this.fail('Choose a User slot from 1 to 128.');
      return false;
    }
    this.slot = slot;
    this.frozen = patch.slice();
    this.frozenName = name;
    this.phase = 'arming';
    this.error = '';
    this.report = null;
    this.cancelWhenArmed = false;
    try {
      const armed = await this.backend.armSave(slot);
      this.armed = armed;
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
      await this.backend.disarmSave();
    } catch {
      // The next arm replaces the arming anyway.
    }
  }

  /** Step 2: write the frozen patch into the armed slot and read it back. Only possible while armed. */
  async commit(): Promise<boolean> {
    const patch = this.frozen;
    if (this.phase !== 'armed' || !patch) return false;
    if (this.now() - this.armedAt > ARMING_LIFETIME_MS) {
      await this.cancel();
      this.fail('The arming expired. Arm the save again.');
      return false;
    }
    const armed = this.armed;
    this.frozen = null;
    this.phase = 'writing';
    try {
      const report = await this.backend.commitSave(patch);
      this.report = report;
      this.armed = null;
      if (armed) this.last = { slot: armed.slot, backupPath: armed.backup_path, oldName: armed.occupant_name };
      if (!report.verified) {
        this.protectAdvice();
        this.fail('The slot was written but reading it back did not match (MISMATCH).');
        return false;
      }
      this.phase = 'done';
      reportSuccess(`Saved to USER:${String(report.slot + 1).padStart(3, '0')}: read back and it matches.`);
      return true;
    } catch (e) {
      // A write may have started, so the earlier patch can still be put back from the backup.
      this.armed = null;
      if (armed) this.last = { slot: armed.slot, backupPath: armed.backup_path, oldName: armed.occupant_name };
      this.protectAdvice();
      this.fail(message(e));
      return false;
    }
  }

  /** Put back what the last save replaced. The caller confirms first; this is a permanent write too. */
  async undo(): Promise<boolean> {
    if (!this.last || this.working || this.phase === 'armed') return false;
    this.phase = 'writing';
    try {
      const report = await this.backend.restoreSave();
      this.report = report;
      this.last = null;
      this.error = '';
      if (!report.verified) {
        this.fail('The old patch was written back but reading it back did not match (MISMATCH).');
        return false;
      }
      this.phase = 'done';
      reportSuccess('The earlier patch is back in its slot.');
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
