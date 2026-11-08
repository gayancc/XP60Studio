// Temporary memory only: exercise Init, Randomize, Mutate, Copy/Paste, A/B and Morph through the real desktop app.
import { copyFileSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { initSync, PatchDoc } from '../src/wasm/pkg/xppatch.js';
import { startApp, wait } from './hw-lib.mjs';
initSync({module:readFileSync(new URL('../src/wasm/pkg/xppatch_bg.wasm',import.meta.url))});
const app=await startApp();
const assert=(ok,text)=>{if(!ok)throw new Error(text);console.log(`PASS ${text}`)};
const clear=()=>app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
let baselineFile;
const exported=async()=>{
  await clear();
  await app.click('File','header');
  assert(await app.click('Export patch as .syx'),'patch export selected');
  const m=await app.waitMessage(/^Saved /);
  if(!m)throw new Error('patch export failed');
  const file=join(app.saves,m.slice(6));
  const p=PatchDoc.fromSyxSlot(readFileSync(file),0);
  try{return {file,bytes:Buffer.from(p.toBlocks())}}finally{p.free()}
};
const sendVerify=async(label)=>{
  assert(await app.click('Send to synth','header'),`${label}: send available`);
  await wait(4000);
  await clear();
  await app.openSynth();
  assert(await app.click('Verify','.synth'),`${label}: verify available`);
  assert(!!(await app.waitMessage(/^Verified/,20000)),`${label}: instrument matches editor`);
};
const tool=(name)=>app.click(name,'.tools');
const restore=async()=>{
  if(!baselineFile)return;
  const document=(await app.send('DOM.getDocument',{depth:-1})).result.root.nodeId;
  const input=(await app.send('DOM.querySelector',{nodeId:document,selector:'input[type=file][aria-label^="Open a .syx"]'})).result.nodeId;
  if(!input)throw new Error('restore file input missing');
  await app.send('DOM.setFileInputFiles',{nodeId:input,files:[baselineFile]});
  await wait(600);
  await sendVerify('cleanup original patch');
};
try{
  await app.click('Patch','header');
  await app.openSynth();
  await app.pick('MIDI input','FCB1010 IN');
  await app.pick('MIDI output','FCB1010 OUT');
  await app.click('Connect','.synth');
  await wait(4000);
  await app.openSynth();
  await app.click('Fetch sound','.synth');
  assert(!!(await app.waitMessage(/Fetched/)),'original temporary patch fetched');
  const before=await exported();
  baselineFile=join(app.saves,'tools-original.syx');
  copyFileSync(before.file,baselineFile);
  await app.js(`(()=>{const s=[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit'));if(s?.getAttribute('aria-checked')==='true')s.click()})()`);
  assert(await app.js(`[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit'))?.getAttribute('aria-checked')==='false'`),'Live edit is OFF for local patch tools');
  assert(await app.js(`(()=>{const t=[...document.querySelectorAll('.left [role=tab]')].find(t=>t.textContent.trim()==='Tools');t?.click();return !!t})()`),'patch Tools panel opens');
  for(const name of ['Init','Randomize','Mutate']){
    assert(await tool(name),`${name} applied`);
    await wait(300);
    assert(!(await exported()).bytes.equals(before.bytes),`${name} changes patch bytes`);
    await sendVerify(name);
    assert(await app.click('Undo','header'),`${name}: Undo available`);
    await wait(250);
    assert((await exported()).bytes.equals(before.bytes),`${name}: one Undo restores every original byte`);
    await sendVerify(`${name} undo`);
  }
  // Copy the selected Tone, mutate it, then paste it back.
  assert(await tool('Copy'),'selected Tone copied');
  assert(await tool('Mutate'),'Tone changed before Paste');
  assert(await tool('Paste'),'Tone pasted');
  assert((await exported()).bytes.equals(before.bytes),'Copy/Paste restores selected Tone bytes');
  // B starts as a copy of A. Editing B must leave A intact.
  assert(await tool('B'),'B selected');
  assert(await tool('Mutate'),'B mutated');
  const b=(await exported()).bytes;
  assert(!b.equals(before.bytes),'B differs from A');
  assert(await tool('A'),'A selected');
  assert((await exported()).bytes.equals(before.bytes),'A remains original');
  assert(await tool('B'),'B selected again');
  assert((await exported()).bytes.equals(b),'B retains its edits');
  const morphRect=await app.js(`(()=>{const s=document.querySelector('[role=slider][aria-label="Morph"]');if(!s||s.getAttribute('aria-disabled')==='true')return null;const r=s.querySelector('.track').getBoundingClientRect();return {x:r.left+5,y:r.top+r.height/2}})()`);
  assert(!!morphRect,'Morph enabled for distinct A and B');
  await app.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:morphRect.x,y:morphRect.y});
  await app.send('Input.dispatchMouseEvent',{type:'mousePressed',x:morphRect.x,y:morphRect.y,button:'left',buttons:1,clickCount:1});
  await app.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:morphRect.x+75,y:morphRect.y,buttons:1});
  await app.send('Input.dispatchMouseEvent',{type:'mouseReleased',x:morphRect.x+75,y:morphRect.y,button:'left',clickCount:1});
  const blended=(await exported()).bytes;
  assert(!blended.equals(before.bytes)&&!blended.equals(b),'midpoint Morph differs from both A and B');
  await sendVerify('Morph');
  assert(app.pageErrors.length===0,'no script errors during patch tools');
  console.log('PATCH TOOLS PASSED');
}finally{
  try{await restore()}finally{app.stop()}
}
