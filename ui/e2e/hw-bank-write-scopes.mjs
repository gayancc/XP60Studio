// Live: after Read from synth, Write offers Changed / This slot / All filled — not one blocked control.
// Does not commit a User write (PERM); only checks the prepare choices are available.
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { startApp, wait, root } from './hw-lib.mjs';

const assert = (ok, label) => {
  if (!ok) throw new Error(label);
  console.log(`PASS ${label}`);
};

const outDir = join(root, 'target', 'hardware-acceptance');
mkdirSync(outDir, { recursive: true });
const log = [];
const say = (line) => {
  console.log(line);
  log.push(line);
};

const app = await startApp(9336);
try {
  assert(await app.click('Patch', 'header'), 'Patch workspace');
  await app.openSynth();
  await app.pick('MIDI input', 'FCB1010 IN');
  await app.pick('MIDI output', 'FCB1010 OUT');
  assert(await app.click('Connect', '.synth'), 'Connect');
  for (let i = 0; i < 30; i++) {
    await wait(500);
    await app.openSynth();
    if (/XP-60/.test((await app.text('.synth .ok')) ?? '')) break;
  }
  assert(await app.click('Bank', 'header'), 'Bank workspace');
  await wait(600);
  assert(await app.click('Write', 'aside, body'), 'Write tab');
  await wait(400);

  let scopes = await app.js(`([...document.querySelectorAll('button')].map(b=>{
    const t=b.textContent.trim();
    return /^1[abc]\\./.test(t) ? {t, disabled:!!b.disabled} : null;
  }).filter(Boolean))`);
  say(`before sync: ${JSON.stringify(scopes)}`);
  assert(scopes.some((s) => /Changed only/.test(s.t)), 'Changed only visible');
  assert(scopes.some((s) => /This slot/.test(s.t)), 'This slot visible');
  assert(scopes.some((s) => /All filled/.test(s.t)), 'All filled visible');

  assert(await app.click('Read from synth', 'body'), 'Read from synth');
  let readDone = false;
  for (let i = 0; i < 300; i++) {
    await wait(1000);
    const busy = await app.js(`[...document.querySelectorAll('button')].some(b=>/Read from synth/.test(b.textContent)&&b.disabled)`);
    const filled = await app.js(`[...document.querySelectorAll('.grid .slot')].filter(b=>{const t=(b.textContent||'').trim(); return t && t!=='—';}).length`);
    if (i % 5 === 0) say(`read ${i}s busy=${busy} filled=${filled}`);
    if (!busy && filled >= 8) {
      readDone = true;
      break;
    }
  }
  assert(readDone, 'bank synced');

  assert(await app.click('Write', 'aside, body'), 'Write tab after sync');
  await wait(400);
  scopes = await app.js(`([...document.querySelectorAll('button')].map(b=>{
    const t=b.textContent.trim();
    return /^1[abc]\\./.test(t) ? {t, disabled:!!b.disabled} : null;
  }).filter(Boolean))`);
  say(`after sync: ${JSON.stringify(scopes)}`);
  const changed = scopes.find((s) => /Changed only/.test(s.t));
  const selected = scopes.find((s) => /This slot/.test(s.t));
  const all = scopes.find((s) => /All filled/.test(s.t));
  assert(!!changed && changed.disabled && /\(0\)/.test(changed.t), 'Changed only (0) disabled after clean sync');
  assert(!!selected && !selected.disabled, 'This slot enabled after sync');
  assert(!!all && !all.disabled && /\(128\)/.test(all.t), 'All filled (128) enabled after sync');

  const hint = await app.js(`document.querySelector('.write-hint')?.textContent ?? ''`);
  say(`hint: ${hint}`);
  assert(/No slot differs|differ from the instrument/i.test(hint), 'explains why Changed is empty');

  say('H-050b WRITE SCOPES LIVE PASS');
  writeFileSync(join(outDir, 'hw-bank-write-scopes.txt'), log.join('\n') + '\n', 'utf8');
} catch (e) {
  say(`FAIL ${e instanceof Error ? e.message : String(e)}`);
  writeFileSync(join(outDir, 'hw-bank-write-scopes.txt'), log.join('\n') + '\n', 'utf8');
  throw e;
} finally {
  app.stop();
}
