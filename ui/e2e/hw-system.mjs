// H-061/H-062/H-063 or --scale for H-065: fetch System settings, edit one safe value, back up, write, undo, compare exported bytes.
// No MIDI channels or receive switches are changed. A full session snapshot must exist first.
import { copyFileSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { startApp, wait } from './hw-lib.mjs';
// System files have no slots. Compare the actual address/data map, independent of frame chunking.
const decode = (path) => {
  const bytes = readFileSync(path), cells = new Map();
  for (let start=0;start<bytes.length;) {
    const end = bytes.indexOf(0xf7,start);
    if (end<0) throw new Error('unterminated System SysEx');
    const f = bytes.subarray(start,end+1);
    if (f.length<11 || f[0]!==0xf0 || f[1]!==0x41 || f[3]!==0x6a || f[4]!==0x12 || f[5]!==0 || f[6]!==0) throw new Error('not System DT1');
    if (f.subarray(5,-1).reduce((a,b)=>a+b,0)%128) throw new Error('invalid System checksum');
    const address = f.subarray(5,9).reduce((a,b)=>a*128+b,0);
    for (let i=9;i<f.length-2;i++) {
      const a=address+i-9;
      if (cells.has(a) && cells.get(a)!==f[i]) throw new Error('conflicting System bytes');
      cells.set(a,f[i]);
    }
    start=end+1;
  }
  if (!cells.size) throw new Error('empty System export');
  return Buffer.from(JSON.stringify([...cells].sort((a,b)=>a[0]-b[0])));
};
const app = await startApp();
const scale = process.argv.includes('--scale');
const assert = (ok, text) => { if (!ok) throw new Error(text); console.log(`PASS ${text}`); };
const clear = () => app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
const button = (text) => app.click(text, '#system-save-panel');
const text = () => app.text('#system-save-panel');
let exportIndex = 0;
const exportSystem = async () => {
  await clear();
  await app.click('File', 'header');
  await app.click('Export System settings as .syx');
  assert(!!(await app.waitMessage(/Saved/)), 'System export saved');
  const path = join(app.saves, 'System-settings.syx');
  copyFileSync(path, join(app.saves, `system-check-${++exportIndex}.syx`));
  return decode(path);
};
let needsUndo = false;
const undo = async () => {
  await clear();
  assert(await button('Undo the last write'), 'System undo offered');
  assert(await button('Put the old settings back'), 'System undo confirmed');
  assert(!!(await app.waitMessage(/earlier System settings are back/, 20000)), 'System undo read back equal');
  needsUndo = false;
};
try {
  await app.click('System', 'header');
  await app.openSynth();
  assert(await app.pick('MIDI input', 'FCB1010 IN'),'MIDI input selected');
  assert(await app.pick('MIDI output', 'FCB1010 OUT'),'MIDI output selected');
  assert(await app.click('Connect', '.synth'),'Connect requested');
  await wait(4000);
  await app.openSynth();
  assert(await app.click('Fetch sound', '.synth'),'System fetch requested');
  const firstFetch=await app.waitMessage(/Read the instrument.s system settings/, 30000);
  if(!firstFetch)console.log('Fetch diagnostic:',JSON.stringify({messages:await app.messages(),synth:await app.text('.synth'),pageErrors:app.pageErrors}));
  assert(!!firstFetch, 'System fetched before editing');
  const original = await exportSystem();
  if(scale){
    assert(await app.js(`(()=>{const t=[...document.querySelectorAll('[role=tab]')].find(t=>t.textContent.trim()==='Scale tune');t?.click();return !!t})()`),'System Scale tune tab opens');
    assert(await app.click('Patch mode','.scale .target'),'Patch-mode scale selected');
    const before=Number(await app.js(`document.querySelector('[role=slider][aria-label="Scale Tune for C"]')?.getAttribute('aria-valuenow')`));
    assert(await app.js(`(()=>{const k=document.querySelector('[role=slider][aria-label="Scale Tune for C"]');if(!k)return false;const v=Number(k.getAttribute('aria-valuenow')),max=Number(k.getAttribute('aria-valuemax'));k.focus();k.dispatchEvent(new KeyboardEvent('keydown',{key:v<max?'ArrowUp':'ArrowDown',bubbles:true}));return true})()`),'Patch-mode C scale edit entered locally');
    await wait(300);
    assert(Number(await app.js(`document.querySelector('[role=slider][aria-label="Scale Tune for C"]')?.getAttribute('aria-valuenow')`))!==before,'Patch-mode C scale changed one step locally');
    const edited=await exportSystem();
    const oldMap=new Map(JSON.parse(original.toString())),newMap=new Map(JSON.parse(edited.toString()));
    const diffs=[...oldMap].filter(([address,value])=>newMap.get(address)!==value);
    assert(diffs.length===1&&diffs[0][0]===(0x20<<7),'local System export changes only Patch-mode C Scale Tune byte');
  }else{
    assert(await app.js(`(()=>{const t=[...document.querySelectorAll('[role=tab]')].find(t=>t.textContent.trim()==='Sound and mode'); t?.click(); return !!t})()`), 'System Sound and mode tab opens');
    assert(await app.js(`(()=>{const k=document.querySelector('[aria-label="Master Tune"][role]'); if(!k) return false; const v=Number(k.getAttribute('aria-valuenow')), max=Number(k.getAttribute('aria-valuemax')); k.focus(); k.dispatchEvent(new KeyboardEvent('keydown',{key:v<max?'ArrowUp':'ArrowDown',bubbles:true})); return true})()`), 'Master Tune changed by one step locally');
  }
  await app.click('Write to synth', 'header');
  assert(await button('1. Read the settings'), 'System backup step started');
  let armed = '';
  for (let i=0;i<80;i++) { armed = await text() ?? ''; if (/Backed up to/.test(armed)) break; await wait(250); }
  const backup = armed.match(/Backed up to\s+(.+?\.syx)/)?.[1];
  assert(backup && decode(backup).equals(original), 'System backup file equals the original export');
  await clear();
  needsUndo = true;
  assert(await button('2. Replace'), 'System write explicitly confirmed');
  assert(!!(await app.waitMessage(/System settings written: read back and they match/, 20000)), 'changed System settings written and read back equal');
  if(scale){
    await clear();
    await app.openSynth();
    await app.click('Fetch sound','.synth');
    assert(!!(await app.waitMessage(/Read the instrument.s system settings/)),'changed Scale Tune fetched from instrument');
    const written=await exportSystem();
    const oldMap=new Map(JSON.parse(original.toString())),newMap=new Map(JSON.parse(written.toString()));
    const diffs=[...oldMap].filter(([address,value])=>newMap.get(address)!==value);
    assert(diffs.length===1&&diffs[0][0]===(0x20<<7),'instrument changed only Patch-mode C Scale Tune byte');
  }
  await undo();
  await button('Close');
  await clear();
  await app.openSynth();
  await app.click('Fetch sound', '.synth');
  assert(!!(await app.waitMessage(/Read the instrument.s system settings/)), 'restored System settings fetched again');
  assert((await exportSystem()).equals(original), 'all System bytes equal the pre-test export');
  assert(app.pageErrors.length === 0, 'System window has no script errors');
  console.log('SYSTEM UI PASSED');
} finally {
  try { if (needsUndo) await undo(); } finally { app.stop(); }
}
