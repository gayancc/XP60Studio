// H-064: establish the scope of User Memory Protect by attempting one backed-up System write.
// The XP-60 accepted a System write with both protections ON; this script verifies and undoes that behavior.
import { copyFileSync, existsSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { startApp, wait } from './hw-lib.mjs';

const decode=(file)=>{
  const bytes=readFileSync(file),cells=new Map();
  for(let start=0;start<bytes.length;){
    const end=bytes.indexOf(0xf7,start);
    if(end<0)throw new Error('unterminated System frame');
    const f=bytes.subarray(start,end+1);
    if(f.length<11||f[0]!==0xf0||f[1]!==0x41||f[3]!==0x6a||f[4]!==0x12||f[5]!==0||f[6]!==0)throw new Error('not System DT1');
    if(f.subarray(5,-1).reduce((a,b)=>a+b,0)%128)throw new Error('bad System checksum');
    const base=f.subarray(5,9).reduce((a,b)=>a*128+b,0);
    for(let i=9;i<f.length-2;i++){
      const address=base+i-9;
      if(cells.has(address)&&cells.get(address)!==f[i])throw new Error('conflicting System byte');
      cells.set(address,f[i]);
    }
    start=end+1;
  }
  if(!cells.size)throw new Error('empty System export');
  return Buffer.from(JSON.stringify([...cells].sort((a,b)=>a[0]-b[0])));
};
const app=await startApp();
const assert=(ok,label)=>{if(!ok)throw new Error(label);console.log(`PASS ${label}`)};
const clear=()=>app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
const dlg=(text)=>app.click(text,'#system-save-panel');
let accepted=false;
try{
  await app.click('System','header');
  await app.openSynth();
  await app.pick('MIDI input','FCB1010 IN');
  await app.pick('MIDI output','FCB1010 OUT');
  await app.click('Connect','.synth');
  await wait(4000);
  await app.openSynth();
  await app.click('Fetch sound','.synth');
  assert(!!(await app.waitMessage(/Read the instrument.s system settings/)),'current System settings fetched');
  await clear();
  await app.click('File','header');
  assert(await app.click('Export System settings as .syx'),'current System export requested');
  assert(!!(await app.waitMessage(/^Saved /)),'current System export completed');
  const before=join(app.saves,'system-protected-original.syx');
  copyFileSync(join(app.saves,'System-settings.syx'),before);
  const original=decode(before);
  assert(await app.js(`(()=>{const t=[...document.querySelectorAll('[role=tab]')].find(t=>t.textContent.trim()==='Sound and mode');t?.click();return !!t})()`),'System Sound and mode tab opens');
  const tuneBefore=await app.js(`Number(document.querySelector('[aria-label="Master Tune"][role]')?.getAttribute('aria-valuenow'))`);
  assert(await app.js(`(()=>{const k=document.querySelector('[aria-label="Master Tune"][role]');if(!k)return false;const v=Number(k.getAttribute('aria-valuenow')),max=Number(k.getAttribute('aria-valuemax'));k.focus();k.dispatchEvent(new KeyboardEvent('keydown',{key:v<max?'ArrowUp':'ArrowDown',bubbles:true}));return true})()`),'Master Tune edit sent to local control');
  await wait(250);
  assert(Number(await app.js(`document.querySelector('[aria-label="Master Tune"][role]')?.getAttribute('aria-valuenow')`))!==tuneBefore,'Master Tune changed one step locally');
  assert(await app.click('Write to synth','header'),'System write panel opens');
  assert(await dlg('1. Read the settings'),'System backup step started');
  let panel='';for(let i=0;i<600;i++){panel=await app.text('#system-save-panel')??'';if(panel.includes('Backed up to')||/failed|timed out|incomplete/i.test(panel))break;await wait(200)}
  const backup=panel.match(/Backed up to\s+(.+?\.syx)/)?.[1];
  assert(!!backup&&existsSync(backup),'System backup exists before attempted write');
  assert(decode(backup).equals(original),'backup map equals complete pre-edit System map');
  writeFileSync(join(app.saves,'system-protected-backup.json'),JSON.stringify({backup},null,2));
  console.log('Independent verification backup:',backup);
  await clear();
  assert(await dlg('2. Replace'),'protected System write explicitly confirmed');
  let notices=[];for(let i=0;i<720;i++){
    panel=await app.text('#system-save-panel')??'';
    notices=await app.messages()??[];
    if(notices.some(m=>/read.back|mismatch|written|failed|refused/i.test(m))||(!panel.includes('Writing and reading back')&&i>0))break;
    await wait(250);
  }
  console.log('System result:',JSON.stringify({notices,panel}));
  accepted=notices.some(m=>/System settings written: read back and they match/i.test(m));
  assert(accepted,'System write remains allowed with User Memory Protect ON');
  assert(app.pageErrors.length===0,'no script errors during protected System write');
}finally{
  try{
    if(accepted){
      assert(await dlg('Undo the last write'),'accepted System write offered Undo');
      assert(await dlg('Put the old settings back'),'accepted System write Undo confirmed');
      assert(!!(await app.waitMessage(/earlier System settings are back/,30000)),'accepted System write undone');
    }
  }finally{app.stop()}
}
console.log('SYSTEM PROTECTION SCOPE OBSERVED; independent full System comparison required');
