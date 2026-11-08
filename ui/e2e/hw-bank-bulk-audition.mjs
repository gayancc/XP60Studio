// Live H-070: a loaded bulk pad sends its patch to temporary memory and plays a short note.
// Keep the owner-provided preflight snapshots outside the app profile for independent restoration.
import { existsSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { root, startApp, wait } from './hw-lib.mjs';
import { initSync, PatchDoc } from '../src/wasm/pkg/xppatch.js';

const evidence = process.env.XPPATCH_HW_SAVES;
if (!evidence) throw new Error('XPPATCH_HW_SAVES is required');
const bank = join(root, 'tests', 'golden', 'xp60-user-bank.syx');
const baseline = readFileSync(join(evidence, 'temp-before-h070-bulk.blocks'));
rmSync(join(evidence, 'h070-golden-bulk-done.signal'), {force:true});
initSync({module: readFileSync(join(root, 'ui', 'src', 'wasm', 'pkg', 'xppatch_bg.wasm'))});
const slotIndex = 4;
const fixture = PatchDoc.fromSyxSlot(readFileSync(bank), slotIndex);
const expected = Buffer.from(fixture.toBlocks());
fixture.free();
const alternateIndex = 5;
const alternate = PatchDoc.fromSyxSlot(readFileSync(bank), alternateIndex);
const alternateExpected = Buffer.from(alternate.toBlocks());
alternate.free();
const app = await startApp(9335);
const assert = (ok, label) => { if (!ok) throw new Error(label); console.log(`PASS ${label}`); };
const invoke = (command, args = {}) => app.js(`window.__TAURI_INTERNALS__.invoke(${JSON.stringify(command)},${JSON.stringify(args)})`);
const temp = async () => Buffer.from(await invoke('midi_fetch_temp_doc', {kind:'patch'}));
let importedId = null;
try {
  await app.click('Patch', 'header');
  await app.openSynth();
  assert(await app.pick('MIDI input', 'FCB1010 IN'), 'MIDI input selected');
  assert(await app.pick('MIDI output', 'FCB1010 OUT'), 'MIDI output selected');
  assert(await app.click('Connect', '.synth'), 'XP-60 connection requested');
  await wait(3500);
  assert((await temp()).equals(baseline), 'preflight temporary patch matches independent backup');
  assert(await app.click('Bank', 'header'), 'Bank workspace opens');
  await wait(450);
  assert(await app.click('Library', '.right'), 'Bank library panel opens');
  await wait(250);
  const existing = await app.js(`[...document.querySelectorAll('.right li.bulk .head')].map(e=>e.getAttribute('aria-label'))`);
  const doc = (await app.send('DOM.getDocument', {depth:-1})).result.root.nodeId;
  const node = (await app.send('DOM.querySelector', {nodeId:doc, selector:'input[type=file][aria-label^="Import a .syx or .mid bulk file"]'})).result.nodeId;
  assert(!!node, 'bulk file input found');
  await app.send('DOM.setFileInputFiles', {nodeId:node, files:[bank]});
  await wait(400);
  assert(await app.js(`(()=>{const b=[...document.querySelectorAll('.right button')].find(b=>b.textContent.trim()==='Import 128 as bulk'); b?.click(); return !!b})()`), 'golden bulk imported');
  await wait(700);
  const imported = await app.js(`(()=>{const old=new Set(${JSON.stringify(existing)}); return [...document.querySelectorAll('.right li.bulk .head')].filter(e=>!old.has(e.getAttribute('aria-label'))).map(e=>e.getAttribute('aria-label'))})()`);
  assert(imported.length===1, `exactly one new fixture bulk appears: ${imported.join(', ')}`);
  const importedName = imported[0].slice('Bulk '.length);
  const importedRows = (await invoke('bulk_list')).filter(r => r.name===importedName && r.source==='xp60-user-bank.syx' && r.count===128);
  assert(importedRows.length===1, 'new fixture bulk has a unique library ID');
  importedId = importedRows[0].id;
  const bulkRow = `.right li.bulk:has(.head[aria-label=${JSON.stringify(imported[0])}])`;
  assert(await app.js(`(()=>{const row=document.querySelector(${JSON.stringify(bulkRow)}); if(!row)return false; if(!row.classList.contains('open'))row.querySelector('.head button.name')?.click(); return true})()`), 'newly imported bulk opened');
  await wait(250);
  assert(await app.js(`(()=>{const b=[...document.querySelectorAll(${JSON.stringify(bulkRow+' .tools button')})].find(b=>b.textContent.trim()==='Load into bank'); b?.click(); return !!b})()`), 'fixture bulk load requested');
  await wait(180);
  assert(await app.js(`(()=>{const b=document.querySelector(${JSON.stringify(bulkRow+' .ask button.go')}); b?.click(); return !!b})()`), 'local bank replaced by fixture');
  await wait(800);
  await app.startMidiTrace();
  const slot = `.grid[aria-label="User patches a"] button.slot:nth-child(${slotIndex+1})`;
  const name = await app.text(`${slot} .nm`);
  assert(name===expected.subarray(0,12).toString('ascii').trim(), `fixture slot name is ${name}`);
  assert(await app.js(`(()=>{const b=document.querySelector(${JSON.stringify(slot)}); b?.click(); return !!b})()`), 'loaded pad selected');
  await wait(2200);
  const preview = await temp();
  writeFileSync(join(evidence, 'temp-h070-golden-bulk-preview.blocks'), preview);
  assert(preview.equals(expected), 'temporary patch matches exact golden fixture bytes');
  const notes = (await app.midiTrace()).filter(x => x.command === 'midi_note' || x.command === 'midi_all_notes_off');
  writeFileSync(join(evidence, 'h070-golden-bulk-note-trace.json'), JSON.stringify(notes, null, 2));
  const on = notes.findIndex(x => x.command === 'midi_note' && x.args?.on === true);
  const off = notes.findIndex((x,i) => i > on && x.command === 'midi_note' && x.args?.on === false);
  const clear = notes.findIndex((x,i) => i > off && x.command === 'midi_all_notes_off' && x.args?.channel === 0);
  assert(on >= 0 && off > on && clear > off, 'note on, note off, All Notes Off sent in order');
  if (process.argv.includes('--stress')) {
    const other = `.grid[aria-label="User patches a"] button.slot:nth-child(${alternateIndex+1})`;
    for (let i=0; i<4; i++) {
      // Two choices in one UI turn: only the newer ticket should reach temporary memory.
      const latest = i%2===0 ? other : slot;
      const older = i%2===0 ? slot : other;
      assert(await app.js(`(()=>{const a=document.querySelector(${JSON.stringify(older)});const b=document.querySelector(${JSON.stringify(latest)});if(!a||!b)return false;a.click();b.click();return true})()`), `rapid choice ${i+1} dispatched`);
      await wait(2700);
      const actual = await temp();
      assert(actual.equals(i%2===0 ? alternateExpected : expected), `rapid choice ${i+1} ended on newest patch bytes`);
    }
    writeFileSync(join(evidence, 'h070-bulk-stress-note-trace.json'), JSON.stringify((await app.midiTrace()).filter(x => x.command==='midi_note'||x.command==='midi_all_notes_off'), null, 2));
  } else {
    writeFileSync(join(evidence, 'h070-golden-bulk-ready.signal'), name);
    console.log(`OWNER LISTENING: ${name}; click cycles running`);
    for (let i=0; i<30; i++) {
      if (existsSync(join(evidence, 'h070-golden-bulk-done.signal'))) break;
      await app.js(`document.querySelector(${JSON.stringify(slot)})?.click()`);
      await wait(2200);
    }
  }
  assert(app.pageErrors.length===0, 'no desktop script errors');
} finally {
  try {
    if (importedId !== null) {
      await invoke('bulk_delete', {id:importedId});
      console.log(`Removed temporary fixture bulk ${importedId} from the Library`);
    }
  } finally { app.stop(); }
}
