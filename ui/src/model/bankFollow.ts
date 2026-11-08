// How the Bank screen follows a pad onto the connected instrument.
//
// Owner scenario (required): sync the keyboard User bank into the app first ("Read from synth"),
// then changing a pad must make the instrument reflect that synced User patch. Playing or
// Program-Changing empty pads *before* that sync is not logical — there is nothing in the app
// that represents the keyboard yet, so follow is a no-op until a slot holds known bytes.
//
// After a synth read (origin "synth"): Bank Select + Program Change selects that User slot on
// the panel, then the synced pad bytes are sent to temporary memory and a short preview note
// plays. Temporary SysEx is required so a late live-edit DT1 cannot leave every pad sounding
// the same after Program Change (hardware probes also need ~400 ms after PC alone).
//
// Loaded bulk / file / editor copies (not yet written to User memory) still audition to temp
// only — Program Change would play the old User slot.

import type { BankSlot } from './bank.svelte';
import { soundSelect } from './bankPanel';
import type { PatchStore } from './store.svelte';

export interface BankFollowSynth {
  connected: boolean;
  selectSound(channel: number, msb: number, lsb: number, program: number): Promise<boolean>;
  note(channel: number, note: number, velocity: number, on: boolean): Promise<void>;
  allNotesOff(channel: number): Promise<void>;
}

export interface BankFollowDeps {
  store: Pick<PatchStore, 'auditionBlocks' | 'whenQueueIdle' | 'cancelPendingLive' | 'queue'>;
  synth: BankFollowSynth;
  playPreviewNote: (
    note: (on: boolean) => Promise<void>,
    allNotesOff: () => Promise<void>,
  ) => Promise<void>;
  /** How long to wait for in-flight SysEx before continuing (ms). */
  settleMs?: number;
}

/**
 * Drive the instrument for one Bank pad. Callers serialize invocations and pass a still-valid
 * check so a newer pad click can abandon this one.
 */
export async function followBankPad(
  slot: number,
  held: BankSlot | null | undefined,
  deps: BankFollowDeps,
  stillCurrent: () => boolean,
): Promise<void> {
  if (!deps.synth.connected || !stillCurrent()) return;
  // Before "Read from synth" (or any assign), pads are empty — do not Program-Change or preview.
  if (!held?.bytes) return;

  const settleMs = deps.settleMs ?? 8000;

  const preview = async (): Promise<void> => {
    if (!stillCurrent() || !deps.synth.connected) return;
    await deps.playPreviewNote(
      (on) => deps.synth.note(0, 60, on ? 100 : 0, on),
      () => deps.synth.allNotesOff(0),
    );
  };

  deps.store.cancelPendingLive();
  await deps.store.whenQueueIdle(settleMs);
  if (!stillCurrent() || !deps.synth.connected) return;

  // Synced from the keyboard: select that User slot on the instrument, then put the synced
  // bytes in temporary memory so the preview matches the pad the musician just chose.
  if (held.origin === 'synth') {
    const sel = soundSelect('USER', slot);
    if (!sel) return;
    await deps.synth.selectSound(0, sel.msb, sel.lsb, sel.program);
    if (!stillCurrent() || !deps.synth.connected) return;
  }

  deps.store.auditionBlocks(held.bytes);
  await deps.store.whenQueueIdle(settleMs);
  if (!stillCurrent() || !deps.synth.connected) return;
  await preview();
}
