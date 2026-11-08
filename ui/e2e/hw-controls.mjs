// H-054: learn an actual hardware controller, drive Cutoff live, verify, undo one continuous sweep, restore sound/map.
import {copyFileSync,writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {startApp,wait} from './hw-lib.mjs';
const app=await startApp();
const assert=(ok,text)=>{if(!ok)throw new Error(text);console.log(`PASS ${text}`)};
const clear=()=>app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
const value=()=>app.js(`Number(document.querySelector('[role=slider][aria-label="Cutoff Frequency"]')?.getAttribute('aria-valuenow'))`);
let originalFile;
const originalMap=await app.js(`localStorage.getItem('xppatch.controlmap.v1')`);
writeFileSync(join(app.saves,'control-map-before.json'),JSON.stringify(originalMap));
const verify=async(label)=>{
  await clear();await app.openSynth();assert(await app.click('Verify','.synth'),`${label}: Verify available`);
  assert(!!(await app.waitMessage(/^Verified/,20000)),`${label}: instrument matches editor`);
};
try{
  await app.click('Patch','header');await app.openSynth();
  await app.pick('MIDI input','FCB1010 IN');await app.pick('MIDI output','FCB1010 OUT');
  await app.click('Connect','.synth');await wait(4000);
  await app.openSynth();await app.click('Fetch sound','.synth');
  assert(!!(await app.waitMessage(/Fetched/)),'original temporary sound fetched');
  await app.click('File','header');await app.click('Export patch as .syx');
  const saved=await app.waitMessage(/^Saved /);assert(!!saved,'original sound saved before linking');
  originalFile=join(app.saves,'controller-original.syx');copyFileSync(join(app.saves,saved.slice(6)),originalFile);
  await app.js(`(()=>{const s=document.querySelector('[role=switch][aria-label="Live edit to the temporary patch"]');if(s?.getAttribute('aria-checked')==='false')s.click()})()`);
  assert(await app.js(`(()=>{const t=[...document.querySelectorAll('.center [role=tab]')].find(t=>t.textContent.trim()==='Filter and amp');t?.click();return !!t})()`),'Filter and amp opens');await wait(300);
  let baseline=await value();assert(Number.isFinite(baseline),'Cutoff readout available');
  if(baseline===0||baseline===127){
    await app.js(`(()=>{const k=document.querySelector('[role=slider][aria-label="Cutoff Frequency"]');k.focus();k.dispatchEvent(new KeyboardEvent('keydown',{key:Number(k.getAttribute('aria-valuenow'))===0?'ArrowUp':'ArrowDown',bubbles:true}))})()`);
    await wait(500);baseline=await value();
  }
  await verify('starting cutoff');
  await app.js(`(()=>{const k=document.querySelector('[role=slider][aria-label="Cutoff Frequency"]'),r=k.getBoundingClientRect();k.dispatchEvent(new MouseEvent('contextmenu',{button:2,clientX:r.left+r.width/2,clientY:r.top+r.height/2,bubbles:true,cancelable:true}))})()`);
  assert(await app.click('Link to'),'MIDI Learn selected from the knob menu');
  await app.click('Controllers','header');
  console.log('READY MODULATION: push the modulation lever forward and back immediately in one smooth sweep, then leave it alone.');
  const learned=await app.waitMessage(/Cutoff Frequency is linked to/,600000);
  assert(!!learned,'hardware controller learned');console.log(learned);
  const observed=[];
  for(let i=0;i<120;i++){observed.push(await value());await wait(100)}
  const final=await value();console.log(`Cutoff before sweep ${baseline}, final ${final}, sampled range ${Math.min(...observed)}..${Math.max(...observed)}`);
  assert(final!==baseline,'hardware controller changes Cutoff in the editor');
  const map=await app.text('.map');assert(map?.includes('Cutoff Frequency'),'link appears in Controllers map');
  writeFileSync(join(app.saves,'control-map-learned.json'),await app.js(`localStorage.getItem('xppatch.controlmap.v1')`)??'null');
  await verify('hardware controller live edit');
  assert(await app.click('Undo','header'),'Undo of hardware sweep offered');await wait(1000);
  assert(await value()===baseline,'one Undo restores the value before the continuous sweep');
  await verify('controller sweep undo');
  assert(app.pageErrors.length===0,'no script errors during MIDI Learn');
}finally{
  try{
    if(originalFile){
      const doc=(await app.send('DOM.getDocument',{depth:-1})).result.root.nodeId;
      const node=(await app.send('DOM.querySelector',{nodeId:doc,selector:'input[type=file][aria-label^="Open a .syx"]'})).result.nodeId;
      if(!node)throw new Error('restore input missing');
      await app.send('DOM.setFileInputFiles',{nodeId:node,files:[originalFile]});await wait(500);
      assert(await app.click('Send to synth','header'),'original temporary sound sent back');await wait(3500);
      await verify('original sound restoration');
    }
  }finally{
    try{await app.js(`(()=>{const original=${JSON.stringify(originalMap)};if(original===null)localStorage.removeItem('xppatch.controlmap.v1');else localStorage.setItem('xppatch.controlmap.v1',original)})()`)}finally{app.stop()}
  }
}
console.log('HARDWARE MIDI LEARN PASSED');
