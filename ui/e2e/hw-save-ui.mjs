// H-030: real desktop Save to synth ceremony on User slot 128, then Undo. Requires both Protect options OFF.
// The app backs up the slot before any write; independent post-exit comparison uses the emitted backup path.
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { initSync, PatchDoc } from '../src/wasm/pkg/xppatch.js';
import { startApp, wait } from './hw-lib.mjs';

initSync({module:readFileSync(new URL('../src/wasm/pkg/xppatch_bg.wasm',import.meta.url))});
const app=await startApp();
const assert=(ok,label)=>{if(!ok)throw new Error(label);console.log(`PASS ${label}`)};
const dlg=(label)=>app.click(label,'#save-panel');
const clear=()=>app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
let backup,restoreNeeded=false,restored=false;
try{
  await app.click('Patch','header');
  await app.openSynth();
  assert(await app.pick('MIDI input','FCB1010 IN'),'MIDI input selected');
  assert(await app.pick('MIDI output','FCB1010 OUT'),'MIDI output selected');
  assert(await app.click('Connect','.synth'),'Connect requested');
  await wait(4000);
  await app.openSynth();
  assert(await app.click('Fetch sound','.synth'),'temporary patch fetch requested');
  assert(!!(await app.waitMessage(/Fetched/)),'temporary patch fetched');
  await app.js(`[...document.querySelectorAll('.center [role=tab]')].find(t=>t.textContent.trim()==='Patch')?.click()`);
  await wait(250);
  const current=await app.js(`document.querySelector('input[aria-label="Patch name"]')?.value`);
  assert(!!current,'editor patch name present');
  assert(await app.click('Save to synth','header'),'native Save to synth panel opens');
  await app.js(`(()=>{const s=document.querySelector('#save-panel [role=spinbutton][aria-label="User slot"]');s.focus();s.dispatchEvent(new KeyboardEvent('keydown',{key:'End',bubbles:true}))})()`);
  assert(await app.js(`document.querySelector('#save-panel [aria-label="User slot"]')?.getAttribute('aria-valuetext')==='128'`),'write target is User slot 128');
  assert(await dlg('1. Read slot and back it up'),'slot backup step started');
  let panel='';for(let i=0;i<600;i++){panel=await app.text('#save-panel')??'';if(panel.includes('Backed up to')||/failed|timed out|incomplete/i.test(panel))break;await wait(200)}
  backup=panel.match(/Backed up to\s+(.+?\.syx)/)?.[1];
  assert(!!backup&&existsSync(backup),'pre-write slot backup exists');
  const old=PatchDoc.fromSyxSlot(readFileSync(backup),127);
  try{assert(old.name().trimEnd()!==current.trimEnd(),`write changes slot 128 from '${old.name().trimEnd()}' to '${current.trimEnd()}'`)}finally{old.free()}
  writeFileSync(join(app.saves,'save-ui-backup.json'),JSON.stringify({backup,current},null,2));
  console.log('Independent verification backup:',backup);
  await clear();
  restoreNeeded=true;
  assert(await dlg('2. Replace'),'slot replacement explicitly confirmed');
  const saved=await app.waitMessage(/Saved to USER:128|mismatch|read-back differs|failed/i,120000);
  assert(!!saved?.includes('Saved to USER:128'),`save read back equal (${saved})`);
  assert(await dlg('Undo the last save'),'Undo offered');
  assert(await dlg('Put the old patch back'),'Undo explicitly confirmed');
  const undone=await app.waitMessage(/earlier patch is back|Undo failed|mismatch/i,120000);
  assert(!!undone?.includes('earlier patch is back'),`Undo read back equal (${undone})`);
  restored=true;restoreNeeded=false;
  assert(app.pageErrors.length===0,'no script errors during Patch save');
  console.log('PATCH SAVE UI PASSED; independent User slot comparison required');
}finally{
  try{
    if(restoreNeeded&&!restored){
      const offered=await dlg('Undo the last save');
      if(offered){
        await dlg('Put the old patch back');
        const notice=await app.waitMessage(/earlier patch is back|Undo failed|mismatch/i,120000);
        console.log('Emergency Undo result:',notice);
      }
    }
  }finally{app.stop()}
}
