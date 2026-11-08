// Real XP-60, temporary patch only. Send the same fetched sound with 128-byte and 32-byte DT1 chunks.
import { copyFileSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { initSync, PatchDoc } from '../src/wasm/pkg/xppatch.js';
import { startApp, wait } from './hw-lib.mjs';

initSync({module:readFileSync(new URL('../src/wasm/pkg/xppatch_bg.wasm',import.meta.url))});
const app=await startApp();
const assert=(condition,description)=>{if(!condition)throw new Error(description);console.log(`PASS ${description}`)};
const stepName='Most data bytes in one message when a whole sound is sent';
const step=()=>`document.querySelector('[role=spinbutton][aria-label=${JSON.stringify(stepName)}]')`;
const current=()=>app.js(`${step()}?.getAttribute('aria-valuenow')`);
const setChunk=async(target)=>{
  assert(await app.js(`(()=>{const s=${step()};if(!s)return false;s.focus();s.dispatchEvent(new KeyboardEvent('keydown',{key:'Home',bubbles:true}));for(let i=16;i<${target};i++)s.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true}));return true})()`),`Chunk control set toward ${target}`);
  await wait(400);
  assert(Number(await current())===target,`Chunk control reads ${target} bytes`);
};
const clear=()=>app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
const sendVerify=async(size)=>{
  await clear();
  const began=Date.now();
  assert(await app.click('Send to synth','header'),`${size}: whole-sound send started`);
  await wait(400);
  await app.openSynth();
  assert(await app.click('Verify','.synth'),`${size}: read-back requested`);
  const notice=await app.waitMessage(/^Verified|^Mismatch/,90000);
  assert(!!notice?.startsWith('Verified'),`${size}: real temporary patch equals editor`);
  console.log(`SEND+READ ${size}: ${Date.now()-began} ms`);
};

let originalChunk;
try{
  await app.click('Patch','header');
  await app.openSynth();
  await app.pick('MIDI input','FCB1010 IN');
  await app.pick('MIDI output','FCB1010 OUT');
  await app.click('Connect','.synth');
  await wait(4000);
  await app.openSynth();
  await app.click('Fetch sound','.synth');
  assert(!!(await app.waitMessage(/Fetched/)),'starting temporary patch fetched');
  await clear();
  await app.click('File','header');
  assert(await app.click('Export patch as .syx'),'starting patch exported');
  const saved=await app.waitMessage(/^Saved /);
  assert(!!saved,'starting patch export completed');
  const baseline=join(app.saves,'chunk-original.syx');
  copyFileSync(join(app.saves,saved.slice(6)),baseline);
  const doc=PatchDoc.fromSyxSlot(readFileSync(baseline),0);
  try{console.log('BASELINE:',doc.name().trimEnd(),doc.toBlocks().length,'bytes')}finally{doc.free()}
  await app.openSynth();
  originalChunk=Number(await current());
  assert(Number.isInteger(originalChunk)&&originalChunk>=16&&originalChunk<=128,'original Chunk preference recorded');
  await setChunk(128);
  await sendVerify(128);
  await setChunk(32);
  await sendVerify(32);
  assert(app.pageErrors.length===0,'no script errors during chunk comparison');
  console.log('CHUNK COMPARISON PASSED; independent temporary-patch comparison required');
}finally{
  try{if(originalChunk!==undefined){await app.openSynth();await setChunk(originalChunk)}}finally{app.stop()}
}
