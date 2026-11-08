// Read-only diagnostic for unexpected patch selection. Log every MIDI IPC command and compare the temp patch around a Bank read.
import {readFileSync,writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {initSync,PatchDoc} from '../src/wasm/pkg/xppatch.js';
import {startApp,wait} from './hw-lib.mjs';
initSync({module:readFileSync(new URL('../src/wasm/pkg/xppatch_bg.wasm',import.meta.url))});
const app=await startApp();
const fetch=async()=>{
  await app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
  await app.openSynth();await app.click('Fetch sound','.synth');
  const message=await app.waitMessage(/Fetched/);if(!message)throw new Error('fetch failed');console.log(message);
  await app.click('File','header');await app.click('Export patch as .syx');
  const saved=await app.waitMessage(/^Saved /);if(!saved)throw new Error('export failed');
  const p=PatchDoc.fromSyxSlot(readFileSync(join(app.saves,saved.slice(6))),0);
  try{return Buffer.from(p.toBlocks())}finally{p.free()}
};
try{
  await app.startMidiTrace();
  await app.click('Patch','header');await app.openSynth();
  await app.pick('MIDI input','FCB1010 IN');await app.pick('MIDI output','FCB1010 OUT');
  await app.click('Connect','.synth');await wait(4000);
  const before=await fetch();writeFileSync(join(app.saves,'trace-before.blocks'),before);
  if (!(await app.click('Bank','header'))) throw new Error('Bank workspace button missing');
  await wait(500);
  if (!(await app.click('Read from synth','.work'))) throw new Error('Bank read button missing');
  await wait(2000);
  for(let i=0;i<360;i++){if(!(await app.js(`!!document.querySelector('.prog')`)))break;await wait(500)}
  await app.click('Patch','header');
  const after=await fetch();writeFileSync(join(app.saves,'trace-after.blocks'),after);
  const trace=await app.midiTrace();writeFileSync(join(app.saves,'midi-ipc-trace.json'),JSON.stringify(trace,null,2));
  for (const command of ['midi_connect','midi_read_bank','midi_fetch_temp_doc']) {
    if (!trace.some(x=>x.command===command)) throw new Error(`Trace missing positive control: ${command}`);
  }
  const mutations=trace.filter(x=>/select_sound|send_frame|commit|restore/.test(x.command));
  console.log('MIDI command counts:',JSON.stringify(trace.reduce((n,x)=>(n[x.command]=(n[x.command]??0)+1,n),{})));
  console.log('Mutating MIDI commands:',JSON.stringify(mutations));
  console.log('Temporary bytes unchanged:',before.equals(after));
  if(mutations.length||!before.equals(after))throw new Error('read workflow changed the patch or emitted a mutating command');
  console.log('READ UI TRACE PASSED');
}finally{app.stop()}
