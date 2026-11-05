// The sound's synchronisation state, always shown with words as well as colour (artifact/docs/xppatchstudio/06-ui-ux-flow.md section 4).

export type SyncState = 'local' | 'edited' | 'sent' | 'tempVerified' | 'stale' | 'saved' | 'mismatch';

export interface SyncStateInfo {
  label: string;
  color: string;
  /** One line for the hint bar. */
  meaning: string;
}

export const SYNC_STATES: Readonly<Record<SyncState, SyncStateInfo>> = {
  local: { label: 'LOCAL', color: 'var(--text-dim)', meaning: 'Only in the app or library, not on the synth' },
  edited: { label: 'EDITED', color: 'var(--warn)', meaning: 'Changed since it was last fetched, sent or saved' },
  sent: { label: 'SENT', color: 'var(--info)', meaning: 'Sent to the synth’s temporary area, not yet read back' },
  tempVerified: {
    label: 'TEMP VERIFIED',
    color: 'var(--ok)',
    meaning: 'The synth’s temporary area was read back and matches',
  },
  stale: {
    label: 'STALE',
    color: 'var(--warn)',
    meaning: 'The synth’s sound was changed from its panel; this copy may be out of date',
  },
  saved: { label: 'SAVED', color: 'var(--ok)', meaning: 'Written to a User slot and read back equal' },
  mismatch: { label: 'MISMATCH', color: 'var(--bad)', meaning: 'The read-back differs, a block timed out, or the write was refused' },
};

export const SYNC_ORDER: readonly SyncState[] = ['local', 'edited', 'sent', 'tempVerified', 'stale', 'saved', 'mismatch'];
