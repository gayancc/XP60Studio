#!/usr/bin/env node
// Asserts that floating MessageStack overlays are gone and every app message is queued in the hint bar.
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ui = join(dirname(fileURLToPath(import.meta.url)), '..');
const fail = (msg) => {
  console.error(msg);
  process.exit(1);
};

const stackPath = join(ui, 'src/kit/MessageStack.svelte');
if (existsSync(stackPath)) fail('MessageStack.svelte still exists; floating overlays must be removed');

const workspace = readFileSync(join(ui, 'src/editor/Workspace.svelte'), 'utf8');
if (workspace.includes('MessageStack')) fail('Workspace still mounts MessageStack');

const index = readFileSync(join(ui, 'src/kit/index.ts'), 'utf8');
if (index.includes('MessageStack')) fail('kit/index.ts still exports MessageStack');

const messages = readFileSync(join(ui, 'src/model/messages.svelte.ts'), 'utf8');
for (const needle of ['lifetimeFor', 'MAX_QUEUE', 'queueSize', 'advance()', 'MIN_MESSAGE_MS', 'pause()', 'resume()', 'durationMs']) {
  if (!messages.includes(needle)) fail(`messages.svelte.ts missing ${needle}`);
}
if (messages.includes('MAX_VISIBLE') || messages.includes('position: fixed')) {
  fail('messages.svelte.ts still looks like the old popup stack');
}

const hint = readFileSync(join(ui, 'src/kit/HintBar.svelte'), 'utf8');
for (const needle of ['status-msg', 'messages.current', 'Dismiss message', 'data-kind', 'messages.pause()', 'messages.resume()', 'parked']) {
  if (!hint.includes(needle)) fail(`HintBar.svelte missing ${needle}`);
}
if (!hint.includes("role={current.kind === 'error' ? 'alert' : 'status'}")) {
  fail('HintBar.svelte does not expose error as alert / other kinds as status');
}

const e2e = readFileSync(join(ui, 'e2e/run.mjs'), 'utf8');
if (e2e.includes('.stack .msg') || e2e.includes('bottom-left corner above the hint bar')) {
  fail('e2e/run.mjs still asserts the floating corner stack');
}
if (!e2e.includes('footer .status-msg') && !e2e.includes('.status-msg')) {
  fail('e2e/run.mjs does not read the hint-bar status message');
}

const hw = readFileSync(join(ui, 'e2e/hw-lib.mjs'), 'utf8');
if (hw.includes('.stack .msg')) fail('hw-lib.mjs still reads the floating corner stack');
if (!hw.includes('status-msg')) fail('hw-lib.mjs does not read the hint-bar status message');

for (const name of readdirSync(join(ui, 'e2e'))) {
  if (!name.endsWith('.mjs')) continue;
  const body = readFileSync(join(ui, 'e2e', name), 'utf8');
  if (body.includes('.stack .msg')) fail(`${name} still targets the floating corner stack`);
}

console.log('status-bar message queue verification passed');
