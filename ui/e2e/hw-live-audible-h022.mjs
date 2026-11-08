// H-022 owner listen: alternate the backed-up temporary patch and Patch Level 0,
// using the desktop app's own MIDI notes and live single-parameter edit path.
import { readFileSync } from 'node:fs';
import { createInterface } from 'node:readline';
import { startApp, wait } from './hw-lib.mjs';

const baseline = Buffer.from(readFileSync(new URL('../../target/hardware-acceptance/2026-11-07-codex/temp-patch.blocks', import.meta.url)));
const app = await startApp(9340);
const rl = createInterface({ input: process.stdin, output: process.stdout });
const assert = (ok, label) => { if (!ok) throw new Error(label); console.log(`PASS ${label}`); };
const readTemp = async () => Buffer.from(await app.js(`window.__TAURI_INTERNALS__.invoke('midi_fetch_temp_patch')`));
const note = async () => {
  await app.js(`window.__TAURI_INTERNALS__.invoke('midi_note',{channel:0,note:60,velocity:105,on:true})`);
  await wait(300);
  await app.js(`window.__TAURI_INTERNALS__.invoke('midi_note',{channel:0,note:60,velocity:0,on:false})`);
  await wait(300);
};
const hear = async (label) => {
  console.log(`LISTEN ${label}: repeated C notes; type done and Enter after listening.`);
  let finished = false;
  const entered = new Promise((resolve) => rl.once('line', () => { finished = true; resolve('owner'); }));
  const limit = new Promise((resolve) => setTimeout(() => { finished = true; resolve('timeout'); }, 120000));
  while (!finished) await note();
  const result = await Promise.race([entered, limit]);
  await app.js(`window.__TAURI_INTERNALS__.invoke('midi_all_notes_off',{channel:0})`);
  assert(result === 'owner', `${label} owner listening completed`);
};
let edited = false;
try {
  assert(await app.click('Patch', 'header'), 'Patch workspace');
  await app.openSynth();
  assert(await app.pick('MIDI input', 'FCB1010 IN'), 'XP input');
  assert(await app.pick('MIDI output', 'FCB1010 OUT'), 'XP output');
  assert(await app.click('Connect', '.synth'), 'Connect');
  await wait(3000);
  await app.openSynth();
  assert(await app.click('Fetch sound', '.synth'), 'Fetch temporary patch');
  await wait(1400);
  assert((await readTemp()).equals(baseline), 'starting temporary patch matches session backup');
  const live = () => app.js(`[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit'))?.getAttribute('aria-checked')`);
  if ((await live()) !== 'true') await app.js(`[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit'))?.click()`);
  assert((await live()) === 'true', 'Live edit ON');
  await hear('original Patch Level');
  assert(await app.js(`(()=>{const t=[...document.querySelectorAll('.center [role=tab]')].find(t=>t.textContent.trim()==='Patch');if(!t)return false;t.click();return true})()`), 'Patch controls');
  await wait(350);
  const before = await app.js(`document.querySelector('[data-param="common:patch_level"] [role=slider]')?.getAttribute('aria-valuenow')`);
  assert(before !== null && Number(before) > 0, `Patch Level begins above zero (${before})`);
  assert(await app.js(`(()=>{const k=document.querySelector('[data-param="common:patch_level"] [role=slider]');if(!k)return false;k.focus();k.dispatchEvent(new KeyboardEvent('keydown',{key:'Home',bubbles:true}));return true})()`), 'Patch Level control moved to zero');
  edited = true;
  await wait(1100);
  const after = await readTemp();
  const changed = [...baseline].reduce((n, b, i) => n + (b !== after[i]), 0);
  assert(changed === 1, `live single-parameter edit changed one temporary byte (${changed})`);
  await hear('Patch Level zero');
  assert(await app.click('Undo', 'header'), 'Undo live edit');
  edited = false;
  await wait(1400);
  assert((await readTemp()).equals(baseline), 'Undo restored every temporary byte');
  assert(app.pageErrors.length === 0, 'no desktop script errors');
  console.log('H-022 LIVE EDIT LISTEN CHECK COMPLETE');
} finally {
  try {
    await app.js(`window.__TAURI_INTERNALS__.invoke('midi_all_notes_off',{channel:0})`);
    if (edited) {
      await app.click('Undo', 'header');
      await wait(1400);
      assert((await readTemp()).equals(baseline), 'cleanup restored original temporary patch');
    }
  } finally { rl.close(); app.stop(); }
}
process.exit(0);
