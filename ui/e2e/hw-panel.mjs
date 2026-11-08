// Read-only H-017 UI check. During READY, choose a different patch on the XP-60 panel.
import { startApp, wait } from './hw-lib.mjs';
const app = await startApp();
try {
  await app.click('Patch', 'header');
  await app.openSynth();
  if (!(await app.pick('MIDI input', 'FCB1010 IN')) || !(await app.pick('MIDI output', 'FCB1010 OUT'))) throw new Error('ports unavailable');
  await app.click('Connect', '.synth');
  await wait(4000);
  await app.openSynth();
  await app.click('Fetch sound', '.synth');
  if (!(await app.waitMessage(/Fetched/))) throw new Error('fetch failed');
  await app.openSynth();
  await app.click('Verify', '.synth');
  if (!(await app.waitMessage(/Verified/))) throw new Error('initial verification failed');
  console.log('READY: choose another patch on the instrument panel now (two-minute window).');
  let stale = false;
  for (let i = 0; i < 240; i++) {
    const chips = await app.js(`[...document.querySelectorAll('[class*=chip]')].map(e=>e.textContent.trim()).join(' | ')`);
    if (/STALE/.test(chips)) { console.log(`PASS panel change marks the app stale: ${chips}`); stale = true; break; }
    await wait(500);
  }
  if (!stale) throw new Error('No STALE state observed after panel selection');
  await app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
  await app.openSynth();
  await app.click('Fetch sound', '.synth');
  const fetched = await app.waitMessage(/Fetched/);
  if (!fetched) throw new Error('refetch failed');
  console.log(`PASS refetch after panel change: ${fetched}`);
  if (app.pageErrors.length) throw new Error(app.pageErrors.join('\n'));
  console.log('PANEL UI PASSED');
} finally { app.stop(); }
