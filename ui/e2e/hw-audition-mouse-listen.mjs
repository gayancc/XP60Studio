// H-051c owner listen: repeated trusted mouse clicks on the desktop Audition keyboard.
import { createInterface } from 'node:readline';
import { startApp, wait } from './hw-lib.mjs';

const app = await startApp(9341);
const rl = createInterface({ input: process.stdin, output: process.stdout });
const assert = (ok, label) => { if (!ok) throw new Error(label); console.log(`PASS ${label}`); };
let finished = false;
try {
  assert(await app.click('Patch', 'header'), 'Patch workspace');
  await app.openSynth();
  assert(await app.pick('MIDI input', 'FCB1010 IN'), 'XP input');
  assert(await app.pick('MIDI output', 'FCB1010 OUT'), 'XP output');
  assert(await app.click('Connect', '.synth'), 'Connect');
  await wait(3000);
  assert(await app.js(`(()=>{const t=[...document.querySelectorAll('[role=tab]')].find(x=>x.textContent.trim()==='Play');if(!t)return false;t.click();return true})()`), 'Audition Play tab');
  const hit = await app.js(`(()=>{const s=document.querySelector('.aud .kb svg');if(!s)return null;const r=s.getBoundingClientRect();return {x:r.left+r.width*0.15,y:r.top+r.height*0.85}})()`);
  assert(hit !== null, 'Audition keyboard available');
  console.log('LISTEN repeated mouse-click notes; type done and Enter after listening.');
  const entered = new Promise((resolve) => rl.once('line', () => { finished = true; resolve('owner'); }));
  const limit = new Promise((resolve) => setTimeout(() => { finished = true; resolve('timeout'); }, 120000));
  while (!finished) {
    await app.send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: hit.x, y: hit.y });
    await app.send('Input.dispatchMouseEvent', { type: 'mousePressed', x: hit.x, y: hit.y, button: 'left', buttons: 1, clickCount: 1 });
    await wait(270);
    await app.send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: hit.x, y: hit.y, button: 'left', clickCount: 1 });
    await wait(400);
  }
  assert((await Promise.race([entered, limit])) === 'owner', 'owner completed listening');
  await app.js(`window.__TAURI_INTERNALS__.invoke('midi_all_notes_off',{channel:0})`);
  assert(app.pageErrors.length === 0, 'no desktop script errors');
  console.log('H-051c MOUSE LISTEN CHECK COMPLETE');
} finally {
  try { await app.js(`window.__TAURI_INTERNALS__.invoke('midi_all_notes_off',{channel:0})`); } catch { /* disconnected */ }
  rl.close(); app.stop();
}
process.exit(0);
