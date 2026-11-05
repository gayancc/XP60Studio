// The hint bar's shared state. Controls publish what the pointer (or keyboard focus) is on; the bar shows it.

export interface HintInfo {
  /** Dim breadcrumb, for example "Tone 2 > TVF". */
  context: string;
  name: string;
  /** What the value means ("+1 octave"). */
  value: string;
  /** The raw number, shown in brackets when it differs from the meaning. */
  raw: string;
  shortcut: string;
}

export const hint: HintInfo = $state({ context: '', name: '', value: '', raw: '', shortcut: '' });

export function showHint(info: Partial<HintInfo>): void {
  hint.context = info.context ?? '';
  hint.name = info.name ?? '';
  hint.value = info.value ?? '';
  hint.raw = info.raw ?? '';
  hint.shortcut = info.shortcut ?? '';
}

export function clearHint(): void {
  showHint({});
}
