// Owner must first confirm BOTH User Memory Protect options ON. Native save must report refusal, not success.
// Uses slot 128, app backup first. The editor marker is local (Live off). No temporary sound writes.
import {copyFileSync,existsSync,readFileSync,writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {initSync,PatchDoc} from '../src/wasm/pkg/xppatch.js';
import {startApp,wait} from './hw-lib.mjs';
initSync({module:readFileSync(new URL('../src/wasm/pkg/xppatch_bg.wasm',import.meta.url))});
const app=await startApp();
const assert=(ok,text)=>{if(!ok)throw new Error(text);console.log(`PASS ${text}`)};
const dlg=text=>app.click(text,'#save-panel');
let accepted=false;
try{
  await app.click('Patch','header');await app.openSynth();
  await app.pick('MIDI input','FCB1010 IN');await app.pick('MIDI output','FCB1010 OUT');
  await app.click('Connect','.synth');await wait(4000);await app.openSynth();await app.click('Fetch sound','.synth');
  assert(!!(await app.waitMessage(/Fetched/)),'current temporary patch fetched');
  await app.click('File','header');await app.click('Export patch as .syx');
  const saved=await app.waitMessage(/^Saved /);assert(!!saved,'temporary patch saved before the refusal test');
  copyFileSync(join(app.saves,saved.slice(6)),join(app.saves,'protected-temp-original.syx'));
  await app.js(`(()=>{const s=[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit'));if(s?.getAttribute('aria-checked')==='true')s.click()})()`);
  assert(await app.js(`[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Live edit'))?.getAttribute('aria-checked')==='false'`),'Live edit is OFF before local marker');
  await app.js(`[...document.querySelectorAll('.center [role=tab]')].find(t=>t.textContent.trim()==='Patch')?.click()`);await wait(300);
  assert(await app.js(`(()=>{const e=document.querySelector('input[aria-label="Patch name"]');if(!e)return false;e.focus();e.value='PROTECT TEST';e.dispatchEvent(new Event('input',{bubbles:true}));e.blur();return true})()`),'distinct write marker entered locally');
  assert(await app.click('Save to synth','header'),'native save panel opens');await wait(300);
  await app.js(`(()=>{const s=document.querySelector('#save-panel [role=spinbutton][aria-label="User slot"]');s.focus();s.dispatchEvent(new KeyboardEvent('keydown',{key:'End',bubbles:true}))})()`);
  assert(await app.js(`document.querySelector('#save-panel [aria-label="User slot"]')?.getAttribute('aria-valuetext')==='128'`),'protected write target is slot 128');
  assert(await dlg('1. Read slot and back it up'),'slot backup step started');
  let text='';for(let i=0;i<600;i++){text=await app.text('#save-panel')??'';if(text.includes('Backed up to')||text.includes('Close')&&/failed|timed out|incomplete/i.test(text))break;await wait(200)}
  console.log('Arm panel:',text);
  const backup=text.match(/Backed up to\s+(.+?\.syx)/)?.[1];assert(!!backup&&existsSync(backup),'slot backup exists before attempted write');
  const patch=PatchDoc.fromSyxSlot(readFileSync(backup),127);
  try{assert(patch.name().trimEnd()!=='PROTECT TEST','attempted content differs from the protected slot')}finally{patch.free()}
  writeFileSync(join(app.saves,'protected-slot-backup.json'),JSON.stringify({backup},null,2));console.log('Independent verification backup:',backup);
  await app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
  assert(await dlg('2. Replace'),'protected write attempted through the explicit confirmation');
  let result=null;let panel='';let notices=[];
  for(let i=0;i<720;i++){
    panel=await app.text('#save-panel')??'';
    notices=await app.messages()??[];
    result=notices.find(m=>/Saved to|mismatch|not match|read-back differs|failed|refused/i.test(m))??null;
    if(result||(!panel.includes('Writing and reading back')&&i>0))break;
    await wait(250);
  }
  console.log('Result:',JSON.stringify({result,notices,panel}));
  accepted=/Saved to/.test(result??'');
  assert(!accepted&&/mismatch|not match|read-back differs/i.test(result??''),'protected write reported as a mismatch, not saved');
  assert(notices.some(m=>/Memory Protect/i.test(m)),'refusal includes Memory Protect advice');
  assert(existsSync(backup),'backup retained after refused write');
  assert(app.pageErrors.length===0,'no script errors during protected save');
}finally{
  try{
    if(accepted){
      await dlg('Undo the last save');await dlg('Put the old patch back');
      assert(!!(await app.waitMessage(/earlier patch is back/,25000)),'unexpectedly accepted write was undone and verified');
    }
  }finally{app.stop()}
}
console.log('PROTECTED SAVE UI PASSED; independent slot comparison required');
