import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

// Every app message appears in the bottom hint bar. This guards the views: a message line (role "alert" or
// "status") may only appear in the places listed here, each a different kind of thing from a queued status message.

const ALLOWED: Record<string, number> = {
  // the status line in the hint bar (alert for errors, status for the rest)
  'kit/HintBar.svelte': 2,
  // live readouts, not messages
  'kit/StateChip.svelte': 1,
  'kit/XpReadout.svelte': 1,
  // a progress readout while a long operation runs, and a page that says a feature needs the desktop app
  'editor/BankView.svelte': 3, // desktop-only page, progress, and the write confirmation
  'editor/LibraryView.svelte': 1, // desktop-only page
  // the question a confirmation asks before something permanent (not a result)
  'editor/SaveCeremony.svelte': 1,
  // the question asked in place before a bulk is saved over, the bank is replaced by a bulk, or a bulk is deleted (not a result)
  'editor/BulkSaveBar.svelte': 1,
  'editor/BulkTree.svelte': 2,
  'editor/SystemSaveCeremony.svelte': 1,
  // the connection readout on the Synth menu
  'editor/SynthBar.svelte': 1,
  // staged-import error / success lines on the Bank Library panel
  'editor/BulkLibraryPanel.svelte': 2,
};

function files(dir: string): string[] {
  return readdirSync(dir).flatMap((n) => {
    const p = join(dir, n);
    return statSync(p).isDirectory() ? (n === 'wasm' ? [] : files(p)) : p.endsWith('.svelte') ? [p] : [];
  });
}

describe('where messages may appear', () => {
  it('has no message line in a view outside the allowed places', () => {
    const found: Record<string, number> = {};
    for (const f of files('src')) {
      const key = f.replaceAll('\\', '/').replace(/^src\//, '');
      if (key.startsWith('storybook/')) continue;
      const n = (readFileSync(f, 'utf8').match(/role="(alert|status)"/g) ?? []).length;
      if (n > 0) found[key] = n;
    }
    for (const [file, n] of Object.entries(found)) expect(ALLOWED[file] ?? 0, `${file} draws ${n} message line(s)`).toBeGreaterThanOrEqual(n);
  });

  it('keeps every app message in the hint bar with no floating stack', () => {
    expect(() => readFileSync('src/kit/MessageStack.svelte', 'utf8')).toThrow();
    const bar = readFileSync('src/kit/HintBar.svelte', 'utf8');
    expect(bar).toMatch(/status-msg/);
    expect(bar).toMatch(/messages\.current/);
    const workspace = readFileSync('src/editor/Workspace.svelte', 'utf8');
    expect(workspace).not.toMatch(/MessageStack/);
    const index = readFileSync('src/kit/index.ts', 'utf8');
    expect(index).not.toMatch(/MessageStack/);
  });
});
