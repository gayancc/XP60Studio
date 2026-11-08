// H-016: owner disconnects the cable to XP-60 MIDI IN during a read, then reconnects after the timeout.
// Changes only the temporary name as a marker, backed up and restored. No permanent memory writes.
import {copyFileSync,existsSync,writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {startApp,wait} from './hw-lib.mjs';
const app=await startApp();
const assert=(ok,text)=>{if(!ok)throw new Error(text);console.log(`PASS ${text}`)};
let originalFile;
const gated=process.argv.includes('--gated');
const signal=async(name)=>{
  console.log(`WAITING FOR ${name}: ${join(app.saves,name+'.signal')}`);
  for(let i=0;i<240;i++){if(existsSync(join(app.saves,name+'.signal')))return;await wait(500)}
  throw new Error(`No ${name} confirmation signal received`);
};
try{
  await app.startMidiTrace();
  await app.click('Patch','header');
  await app.openSynth();
  await app.pick('MIDI input','FCB1010 IN');
  await app.pick('MIDI output','FCB1010 OUT');
  await app.click('Connect','.synth');
  await wait(4000);
  assert(/XP-60/.test(await app.text('.synth .ok')??''),'instrument connected before dropped-read test');
  await app.click('Fetch sound','.synth');
  assert(!!(await app.waitMessage(/Fetched/)),'starting temporary patch fetched');
  await app.click('File','header');await app.click('Export patch as .syx');
  const saved=await app.waitMessage(/^Saved /);
  assert(!!saved,'starting temporary patch saved to disk');
  originalFile=join(app.saves,'drop-original.syx');copyFileSync(join(app.saves,saved.slice(6)),originalFile);
  await app.js(`[...document.querySelectorAll('.center [role=tab]')].find(t=>t.textContent.trim()==='Patch')?.click()`);
  await wait(300);
  assert(await app.js(`(()=>{const e=document.querySelector('input[aria-label="Patch name"]');if(!e)return false;e.focus();e.value='DROP PROBE';e.dispatchEvent(new Event('input',{bubbles:true}));e.blur();return true})()`),'temporary name marker entered');
  await wait(2500);await app.openSynth();await app.click('Verify','.synth');
  assert(!!(await app.waitMessage(/^Verified/)),'temporary name edit verified before cable interruption');
  await app.click('Bank','header');
  await wait(500);
  if(gated){console.log('READY UNPLUG: confirm the cable is disconnected before the read starts.');await signal('unplugged')}
  assert(await app.click('Read from synth','.work'),'bank read started');
  console.log('READY UNPLUG: disconnect the cable at XP-60 MIDI IN now. Keep it disconnected until RECONNECT.');
  const failure=await app.waitMessage(/could not be read|timeout|incomplete/i,90000);
  assert(!!failure,`dropped read reports a useful failure: ${failure}`);
  console.log('RECONNECT: plug the same cable back into XP-60 MIDI IN now. Retrying connection for two minutes.');
  if(gated)await signal('reconnected');
  await app.click('Patch','header');
  await app.openSynth();
  await app.click('Disconnect','.synth');
  let connected=false;
  for(let i=0;i<24;i++){
    await app.openSynth();
    await app.click('Connect','.synth');
    await wait(4000);
    if(/XP-60/.test(await app.text('.synth .ok')??'')){connected=true;break}
    await wait(1000);
  }
  assert(connected,'reconnected after cable restored');
  await app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
  await app.openSynth();
  await app.click('Fetch sound','.synth');
  const fetched=await app.waitMessage(/Fetched/);
  assert(!!fetched,'fetch succeeds after physical reconnection');
  console.log('After reconnect:',fetched);
  const trace=await app.midiTrace();writeFileSync(join(app.saves,'drop-midi-ipc-trace.json'),JSON.stringify(trace,null,2));
  assert(trace.some(x=>x.command==='midi_read_bank'),'command trace includes the bank read');
  assert(!trace.some(x=>x.command==='midi_select_sound'),'no program-selection command sent by app');
  assert(fetched?.includes('DROP PROBE'),'edited temporary sound survives physical cable reconnect');
  await app.openSynth();
  await app.click('Verify','.synth');
  assert(!!(await app.waitMessage(/^Verified/)),'refetched sound verified after physical reconnection');
  assert(app.pageErrors.length===0,'no script errors during dropped-read recovery');
  console.log('DROPPED READ UI PASSED');
}finally{
  try{
    if(originalFile){
      await app.click('Patch','header');await wait(400);
      const doc=(await app.send('DOM.getDocument',{depth:-1})).result.root.nodeId;
      const node=(await app.send('DOM.querySelector',{nodeId:doc,selector:'input[type=file][aria-label^="Open a .syx"]'})).result.nodeId;
      if(!node)throw new Error(`Restore input missing; backup: ${originalFile}`);
      await app.send('DOM.setFileInputFiles',{nodeId:node,files:[originalFile]});await wait(500);
      assert(await app.click('Send to synth','header'),'restoring original temporary sound');await wait(3500);
      await app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
      await app.openSynth();await app.click('Verify','.synth');
      assert(!!(await app.waitMessage(/^Verified/)),'original temporary sound restored and verified');
    }
  }finally{app.stop()}
}
