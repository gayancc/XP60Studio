// Native Library hover audition against the XP-60. Only temporary memory is sent; all reads are read-only.
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { join } from 'node:path';
import { root, startApp, wait } from './hw-lib.mjs';

const app=await startApp();
const assert=(ok,label)=>{if(!ok)throw new Error(label);console.log(`PASS ${label}`)};
const invoke=(command,args={})=>app.js(`window.__TAURI_INTERNALS__.invoke(${JSON.stringify(command)},${JSON.stringify(args)})`);
const temp=async()=>Buffer.from(await invoke('midi_fetch_temp_doc',{kind:'patch'}));
const switchState=()=>app.js(`[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Hear on hover'))?.getAttribute('aria-checked')`);
let baseline;
let hovering=false;
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
  baseline=await temp();
  writeFileSync(join(app.saves,'hover-original.blocks'),baseline);
  console.log('Starting temp name:',baseline.subarray(0,12).toString('ascii').trimEnd());
  if(process.argv.includes('--trace-notes'))await app.startMidiTrace();
  assert(await app.click('Library','header'),'Library workspace opens');
  for(let i=0;i<80;i++){
    if(await app.js(`document.querySelectorAll('ul.rows[aria-label="Stored patches"] li:not(.empty)').length`))break;
    await wait(250);
  }
  const rows=await invoke('library_list',{text:null,tag:null,favouritesOnly:false,bulk:null,limit:50,offset:0});
  const names=await app.js(`[...document.querySelectorAll('ul.rows[aria-label="Stored patches"] li:not(.empty) .name .n')].map(e=>e.textContent.trim())`);
  const target=rows.find(r=>r.name.trim()!==baseline.subarray(0,12).toString('ascii').trim()&&names.filter(n=>n===r.name.trim()).length===1);
  assert(!!target,`distinct stored patch available (visible ${names.length})`);
  const expected=Buffer.from(await invoke('library_get',{id:target.id}));
  assert(!expected.equals(baseline),'preview target differs from starting temporary patch');
  assert(await app.js(`(()=>{const s=[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Hear on hover'));if(!s)return false;s.click();return true})()`),'Hear on hover switched on');
  assert((await switchState())==='true','Hear on hover visibly ON');
  const rect=await app.js(`(()=>{const r=[...document.querySelectorAll('ul.rows[aria-label="Stored patches"] li:not(.empty)')].find(e=>e.querySelector('.name .n')?.textContent.trim()===${JSON.stringify(target.name.trim())})?.getBoundingClientRect();return r?{x:r.x+Math.min(20,r.width/2),y:r.y+r.height/2}:null})()`);
  assert(!!rect,`target row '${target.name.trim()}' is visible`);
  await app.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:rect.x,y:rect.y});
  hovering=true;
  await wait(2200);
  const sounding=await temp();
  writeFileSync(join(app.saves,'hover-preview.blocks'),sounding);
  assert(sounding.equals(expected),'hovered stored patch reached temporary memory exactly');
  if(process.argv.includes('--stress-switch')){
    const pause=await app.js(`(()=>{const r=document.querySelector('.bar')?.getBoundingClientRect();return r?{x:r.x+5,y:r.y+5}:null})()`);
    assert(!!pause,'safe stress-cycle pointer location found');
    for(let i=0;i<4;i++){
      await app.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:pause.x,y:pause.y});
      await wait(430);
      await app.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:rect.x,y:rect.y});
      await wait(480);
    }
    console.log('Completed four quick leave/re-enter cycles');
  }
  if(process.argv.includes('--owner-hold')||process.argv.includes('--owner-notes')||process.argv.includes('--owner-auto')){
    writeFileSync(join(app.saves,'hover-ready.signal'),target.name.trim());
    console.log('OWNER HOVER READY: preview held on',target.name.trim());
    let signalled=false;
    for(let i=0;i<180;i++){
      if(existsSync(join(app.saves,'hover-owner-done.signal'))){signalled=true;break}
      if(process.argv.includes('--owner-auto')){
        const pause=await app.js(`(()=>{const r=document.querySelector('.bar')?.getBoundingClientRect();return r?{x:r.x+5,y:r.y+5}:null})()`);
        if(!pause)throw new Error('safe pointer-leave location missing');
        await app.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:pause.x,y:pause.y});
        await wait(1100);
        await app.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:rect.x,y:rect.y});
        await wait(1900);
      }else if(process.argv.includes('--owner-notes')){
        await invoke('midi_note',{channel:0,note:60,velocity:100,on:true});
        await wait(700);
        await invoke('midi_note',{channel:0,note:60,velocity:0,on:false});
        await wait(700);
      }else await wait(1000);
    }
    console.log('Owner signal received:',signalled);
  }
  const out=await app.js(`(()=>{const r=document.querySelector('.bar')?.getBoundingClientRect();return r?{x:r.x+5,y:r.y+5}:null})()`);
  assert(!!out,'safe pointer-leave location found');
  await app.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:out.x,y:out.y});
  hovering=false;
  await wait(2200);
  const restored=await temp();
  writeFileSync(join(app.saves,'hover-restored.blocks'),restored);
  assert(restored.equals(baseline),'leaving row restores every original temporary-patch byte');
  await app.js(`[...document.querySelectorAll('[role=switch]')].find(e=>e.textContent.includes('Hear on hover'))?.click()`);
  assert((await switchState())==='false','Hear on hover switched back off');
  if(process.argv.includes('--trace-notes')){
    const notes=(await app.midiTrace()).filter(x=>x.command==='midi_note'||x.command==='midi_all_notes_off');
    console.log('Preview note trace:',JSON.stringify(notes));
    const one=notes.findIndex(x=>x.command==='midi_note'&&x.args?.on===true);
    const off=notes.findIndex((x,i)=>i>one&&x.command==='midi_note'&&x.args?.on===false);
    const clear=notes.findIndex((x,i)=>i>off&&x.command==='midi_all_notes_off'&&x.args?.channel===0);
    assert(one>=0&&off>one&&clear>off,'note on, note off, channel All Notes Off were invoked in order');
  }
  assert(app.pageErrors.length===0,'no script errors during hover preview');
  console.log('HOVER PREVIEW PASSED; independent User-memory comparison still required');
}finally{
  try{
    if(hovering){
      const out=await app.js(`(()=>{const r=document.querySelector('.bar')?.getBoundingClientRect();return r?{x:r.x+5,y:r.y+5}:null})()`);
      if(out){await app.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:out.x,y:out.y});await wait(1800)}
    }
  }finally{
    app.stop();
    if(baseline&&process.env.XPPATCH_HW_SAVES){
      await wait(1500);
      const after=join(app.saves,'hover-after-app-exit.blocks');
      execFileSync(join(root,'target','debug','examples','dump_temp.exe'),['FCB1010 IN','FCB1010 OUT',after]);
      assert(readFileSync(after).equals(baseline),'temporary patch still equals original after app exit');
    }
  }
}
