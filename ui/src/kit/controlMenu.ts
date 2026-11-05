import { snap, type Range } from './gesture';
import { valueClipboard, type MenuItem } from './menu.svelte';

interface MenuTarget {
  range: Range;
  get: () => number;
  set: (value: number) => void;
  defaultValue?: number;
  begin?: () => void;
  end?: () => void;
  /** Linking the control to a hardware controller. Absent when there is no instrument connection to listen on. */
  midi?: MidiLink;
}

/** What the menu needs to offer "link to MIDI CC". */
export interface MidiLink {
  /** The controller it is linked to ("CC 74"), or null. */
  linked: string | null;
  learn: () => void;
  unlink: () => void;
}

/** Parse typed text into a value inside the range, or null when it is not a number. */
export function parseTyped(text: string, range: Range): number | null {
  const trimmed = text.trim();
  if (trimmed === '') return null;
  const n = Number(trimmed);
  if (!Number.isFinite(n)) return null;
  return snap(n, range);
}

/** The standard right-click menu every continuous control shares: type, copy, paste, reset. */
export function controlMenuItems(t: MenuTarget): MenuItem[] {
  const commit = (v: number): void => {
    t.begin?.();
    t.set(v);
    t.end?.();
  };
  const pasted = valueClipboard.value;
  return [
    {
      label: 'Type value',
      input: {
        initial: String(t.get()),
        onSubmit: (text) => {
          const v = parseTyped(text, t.range);
          if (v !== null) commit(v);
        },
      },
    },
    {
      label: 'Copy value',
      action: () => {
        valueClipboard.value = t.get();
      },
    },
    {
      label: pasted === null ? 'Paste value' : `Paste value (${pasted})`,
      disabled: pasted === null,
      action: () => {
        if (pasted !== null) commit(snap(pasted, t.range));
      },
    },
    { label: '', separator: true },
    {
      label: 'Reset to default',
      disabled: t.defaultValue === undefined,
      action: () => {
        if (t.defaultValue !== undefined) commit(t.defaultValue);
      },
    },
    ...midiItems(t.midi),
  ];
}

function midiItems(midi: MidiLink | undefined): MenuItem[] {
  if (!midi) return [{ label: 'Link to MIDI CC…', disabled: true }];
  const items: MenuItem[] = [{ label: midi.linked ? `Link to another controller… (now ${midi.linked})` : 'Link to MIDI CC…', action: midi.learn }];
  if (midi.linked) items.push({ label: `Unlink ${midi.linked}`, action: midi.unlink });
  return items;
}
