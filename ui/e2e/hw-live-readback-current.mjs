// Focused native-app live edit check independent of status-line timing.
import { readFileSync } from 'node:fs';
import { startApp, wait } from './hw-lib.mjs';

const baseline = Buffer.from(readFileSync(new URL('../../target/hardware-acceptance/2026-11-07-codex/temp-patch.blocks', import.meta.url)));
const app = await startApp(9342);
const assert = (ok, label) => { if (!ok) throw new Error(label); console.log(`PASS ${label}`); };
const readTemp = async () => Buffer.from(await app.js(`window.__TAURI_INTERNALS__.invoke('midi_fetch_temp_patch')`));
let changed = false;
try {
  assert(await app.click('Patch', 'header'), 'Patch workspace');
  let inputPicked = false;
  for (let i = 0; i < 15 && !inputPicked; i++) {
    await app.openSynth();
    inputPicked = await app.pick('MIDI input', 'FCB1010 IN');
    if (!inputPicked) await wait(400);
  }
  assert(inputPicked, 'XP input');
  assert(await app.pick('MIDI output', 'FCB1010 OUT'), 'XP output');
  assert(await app.click('Connect', '.synth'), 'Connect');
  await wait(3000);
  await app.openSynth();
  assert(await app.click('Fetch sound', '.synth'), 'Fetch sound');
  await wait(1500);
  assert((await readTemp()).equals(baseline), 'starting temporary patch equals backup');
  const live = () => app.js(`[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit'))?.getAttribute('aria-checked')`);
  if ((await live()) !== 'true') await app.js(`[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit'))?.click()`);
  assert((await live()) === 'true', 'Live edit ON');
  assert(await app.js(`(()=>{const t=[...document.querySelectorAll('.center [role=tab]')].find(t=>t.textContent.trim()==='Patch');if(!t)return false;t.click();return true})()`), 'Patch controls');
  await wait(300);
  const before = await app.js(`document.querySelector('[data-param="common:patch_level"] [role=slider]')?.getAttribute('aria-valuenow')`);
  assert(Number(before) > 0, `Patch Level starts at ${before}`);
  assert(await app.js(`(()=>{const k=document.querySelector('[data-param="common:patch_level"] [role=slider]');if(!k)return false;k.focus();k.dispatchEvent(new KeyboardEvent('keydown',{key:'Home',bubbles:true}));return true})()`), 'Patch Level set to zero');
  changed = true;
  await wait(1800);
  const edited = await readTemp();
  const diffs = [...baseline].reduce((n, b, i) => n + (b !== edited[i]), 0);
  console.log(`READBACK changed bytes=${diffs}`);
  assert(diffs === 1, 'live edit changed exactly one device byte');
  assert(await app.click('Undo', 'header'), 'Undo');
  changed = false;
  await wait(1800);
  assert((await readTemp()).equals(baseline), 'Undo restored every temporary byte');
  assert(app.pageErrors.length === 0, 'no desktop script errors');
} finally {
  if (changed) {
    try { await app.click('Undo', 'header'); await wait(1800); } catch { /* independent restore follows on failure */ }
  }
  app.stop();
}
process.exit(0);
