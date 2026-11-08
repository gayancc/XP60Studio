// Diagnose whether a local-only name change reaches the XP-60's temporary patch.
// Saves the starting sound; does not use Save to synth or write User memory.
import { copyFileSync } from 'node:fs';
import { join } from 'node:path';
import { startApp, wait } from './hw-lib.mjs';

const app=await startApp();
const assert=(ok,label)=>{if(!ok)throw new Error(label);console.log(`PASS ${label}`)};
const live=()=>app.js(`[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit'))?.getAttribute('aria-checked')`);
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
  await app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
  await app.click('File','header');
  assert(await app.click('Export patch as .syx'),'starting patch exported');
  const saved=await app.waitMessage(/^Saved /);
  assert(!!saved,'starting patch export completed');
  const baseline=join(app.saves,'live-off-original.syx');
  copyFileSync(join(app.saves,saved.slice(6)),baseline);
  console.log('Starting file:',baseline);
  console.log('Live before:',await live());
  if((await live())==='true')await app.js(`[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit')).click()`);
  await wait(500);
  assert((await live())==='false','Live edit visibly OFF before name change');
  await app.js(`(()=>{const t=[...document.querySelectorAll('.center [role=tab]')].find(t=>t.textContent.trim()==='Patch');t?.click()})()`);
  await wait(400);
  assert(await app.js(`(()=>{const e=document.querySelector('input[aria-label="Patch name"]');if(!e)return false;e.focus();e.value='LIVE OFFCHK';e.dispatchEvent(new Event('input',{bubbles:true}));e.blur();return true})()`),'local name marker entered');
  await wait(1000);
  assert((await live())==='false','Live edit still OFF after name change');
  await app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
  await app.openSynth();
  assert(await app.click('Verify','.synth'),'hardware read-back started');
  const result=await app.waitMessage(/^Mismatch|^Verified/,30000);
  console.log('Read-back result:',result);
  console.log('All notices:',JSON.stringify(await app.messages()));
  assert(!!result,'hardware read-back completed');
  assert(result.startsWith('Mismatch'),'local-only marker absent from hardware temporary patch');
  assert(app.pageErrors.length===0,'no script errors');
}finally{app.stop()}
