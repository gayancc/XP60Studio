// H-052: compare the rebuilt native XP-60 Init action to two panel captures, send/verify, Undo/restore.
import { readFileSync, readdirSync, rmSync } from 'node:fs';
import { join } from 'node:path';
import { initSync, PatchDoc } from '../src/wasm/pkg/xppatch.js';
import { startApp, wait } from './hw-lib.mjs';

initSync({ module: readFileSync(new URL('../src/wasm/pkg/xppatch_bg.wasm', import.meta.url)) });
const original = readFileSync(new URL('../../target/hardware-acceptance/2026-11-07-codex/temp-before-h052-panel-init.blocks', import.meta.url));
const panel = readFileSync(new URL('../../target/hardware-acceptance/2026-11-07-codex/temp-panel-init-h052.blocks', import.meta.url));
const app = await startApp(9338);
const assert = (ok, label) => {
  if (!ok) throw new Error(label);
  console.log(`PASS ${label}`);
};
const exportBytes = async () => {
  assert(await app.click('File', 'header'), 'File menu available');
  assert(await app.click('Export patch as .syx'), 'Patch export available');
  let file;
  for (let i = 0; i < 20 && !file; i++) {
    await wait(250);
    file = readdirSync(app.saves).find((name) => name.endsWith('.syx'));
  }
  if (!file) throw new Error(`Patch export wrote no .syx file: ${JSON.stringify(readdirSync(app.saves))}`);
  const path = join(app.saves, file);
  const doc = PatchDoc.fromSyxSlot(readFileSync(path), 0);
  try { return Buffer.from(doc.toBlocks()); }
  finally { doc.free(); rmSync(path); }
};
const readTemp = async () => Buffer.from(await app.js(`window.__TAURI_INTERNALS__.invoke('midi_fetch_temp_patch')`));
try {
  for (let i = 0; i < 20 && !(await app.js(`!!document.querySelector('header')`)); i++) await wait(300);
  assert(await app.click('Patch', 'header'), 'Patch workspace');
  await app.openSynth();
  assert(await app.pick('MIDI input', 'FCB1010 IN'), 'XP input');
  assert(await app.pick('MIDI output', 'FCB1010 OUT'), 'XP output');
  assert(await app.click('Connect', '.synth'), 'Connect');
  for (let i = 0; i < 20; i++) {
    await wait(400);
    await app.openSynth();
    if (/XP-60 · ID 17/.test((await app.text('.synth .ok')) ?? '')) break;
  }
  assert(/XP-60 · ID 17/.test((await app.text('.synth .ok')) ?? ''), 'connected to XP-60');
  assert(await app.click('Fetch sound', '.synth'), 'Fetch sound');
  await wait(1500);
  assert((await exportBytes()).equals(original), 'native editor fetched starting Jimmee Dee bytes');
  await app.js(`(()=>{const s=[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit'));if(s?.getAttribute('aria-checked')==='true')s.click()})()`);
  assert((await app.js(`[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit'))?.getAttribute('aria-checked')`)) === 'false', 'Live edit OFF');
  assert(await app.js(`(()=>{const t=[...document.querySelectorAll('.left [role=tab]')].find(t=>t.textContent.trim()==='Tools');t?.click();return !!t})()`), 'Tools panel');
  assert(await app.click('XP-60 Init', '.tools'), 'XP-60 Init action');
  assert((await exportBytes()).equals(panel), 'desktop Init bytes equal XP-60 panel DEFAULT capture');
  assert((await readTemp()).equals(original), 'Live OFF kept original temporary patch before explicit send');
  assert(await app.click('Send to synth', 'header'), 'Send Init to temporary memory');
  await wait(3500);
  assert((await readTemp()).equals(panel), 'XP-60 read-back equals panel Init and desktop bytes');
  assert(await app.click('Undo', 'header'), 'Undo Init');
  assert((await exportBytes()).equals(original), 'Undo restored original editor bytes');
  assert(await app.click('Send to synth', 'header'), 'Restore Jimmee Dee to temporary memory');
  await wait(3500);
  assert((await readTemp()).equals(original), 'XP-60 read-back restored Jimmee Dee');
  assert(app.pageErrors.length === 0, 'no desktop script errors');
} finally {
  app.stop();
}
process.exit(0);
