// H-044/H-045/H-047: Performance mode required. Temporary documents only; original exports and blocks kept on disk.
import {copyFileSync,readFileSync,writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {initSync,Doc} from '../src/wasm/pkg/xppatch.js';
import {startApp,wait} from './hw-lib.mjs';
initSync({module:readFileSync(new URL('../src/wasm/pkg/xppatch_bg.wasm',import.meta.url))});
const app=await startApp();
const assert=(ok,text)=>{if(!ok)throw new Error(text);console.log(`PASS ${text}`)};
const clear=()=>app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
const exported=async(kind)=>{
  await clear();await app.click('File','header');
  assert(await app.click(`Export ${kind==='performance'?'performance':'rhythm set'} as .syx`),'document export selected');
  const m=await app.waitMessage(/^Saved /);if(!m)throw new Error('document export failed');
  const file=join(app.saves,m.slice(6)),d=Doc.fromSyxSlot(kind,readFileSync(file),0);
  try{return {file,bytes:Buffer.from(d.toBlocks())}}finally{d.free()}
};
const verify=async(label)=>{
  await clear();await app.openSynth();assert(await app.click('Verify','.synth'),`${label}: Verify available`);
  assert(!!(await app.waitMessage(/^Verified/,30000)),`${label}: complete instrument document matches editor`);
};
const send=async(label)=>{assert(await app.click('Send to synth','header'),`${label}: whole-document send available`);await wait(5000);await verify(label)};
try{
  await app.click('Performance','header');await wait(500);
  await app.openSynth();await app.pick('MIDI input','FCB1010 IN');await app.pick('MIDI output','FCB1010 OUT');
  await app.click('Connect','.synth');await wait(4000);
  assert(/XP-60/.test(await app.text('.synth .ok')??''),'connected in Performance mode');
  for(const [kind,mode,tab,label] of [['performance','Performance','Mix','Part Level'],['rhythm','Rhythm set','Amp','Tone Level']]){
    await app.click(mode,'header');await wait(500);await clear();await app.openSynth();
    assert(await app.click('Fetch sound','.synth'),`${kind}: fetch available`);
    const fetched=await app.waitMessage(/Fetched/,30000);assert(!!fetched,`${kind}: fetched from instrument`);console.log(fetched);
    const original=await exported(kind),file=join(app.saves,`${kind}-original.syx`);
    copyFileSync(original.file,file);writeFileSync(join(app.saves,`${kind}-original.blocks`),original.bytes);
    let touched=false;
    try{
      await app.js(`(()=>{const s=document.querySelector('[role=switch][aria-label="Live edit to the temporary patch"]');if(s?.getAttribute('aria-checked')==='false')s.click()})()`);
      assert(await app.js(`(()=>{const t=[...document.querySelectorAll('.stage [role=tab]')].find(t=>t.textContent.trim()===${JSON.stringify(tab)});t?.click();return !!t})()`),`${kind}: ${tab} section opens`);
      await wait(300);touched=true;
      assert(await app.js(`(()=>{const k=document.querySelector('[role=slider][aria-label=${JSON.stringify(label)}]');if(!k)return false;const v=Number(k.getAttribute('aria-valuenow'));k.focus();k.dispatchEvent(new KeyboardEvent('keydown',{key:v>0?'ArrowDown':'ArrowUp',bubbles:true}));return true})()`),`${kind}: ${label} edited`);
      await wait(1500);await verify(`${kind} live edit`);
      const edited=(await exported(kind)).bytes;
      assert(edited.length===original.bytes.length&&edited.reduce((n,v,i)=>n+(v!==original.bytes[i]?1:0),0)===1,`${kind}: only one parameter byte changed`);
      await send(`${kind} whole send`);
      assert(await app.click('Undo','header'),`${kind}: Undo available`);await wait(1500);
      assert((await exported(kind)).bytes.equals(original.bytes),`${kind}: Undo restores every original byte`);
      await verify(`${kind} undo`);
    }finally{
      if(touched){
        const root=(await app.send('DOM.getDocument',{depth:-1})).result.root.nodeId;
        const input=(await app.send('DOM.querySelector',{nodeId:root,selector:'input[type=file][aria-label^="Open a .syx"]'})).result.nodeId;
        if(!input)throw new Error(`${kind}: restore input missing; backup ${file}`);
        await app.send('DOM.setFileInputFiles',{nodeId:input,files:[file]});await wait(600);
        await send(`${kind} original restore`);
      }
    }
  }
  assert(app.pageErrors.length===0,'no script errors during document editing');
  console.log('TEMP DOCUMENT UI PASSED');
}finally{app.stop()}
