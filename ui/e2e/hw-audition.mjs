// H-051c: the real desktop keyboard makes notes and Panic stops held notes. Owner confirms sound.
import { startApp, wait } from './hw-lib.mjs';
const app = await startApp();
const key = (code, type) => app.js(`(()=>{const k=document.querySelector('.aud .kb'); if(!k) throw new Error('audition keyboard missing'); k.focus(); k.dispatchEvent(new KeyboardEvent(${JSON.stringify(type)},{code:${JSON.stringify(code)},key:'z',bubbles:true}))})()`);
try {
  await app.click('Patch', 'header');
  await app.openSynth();
  await app.pick('MIDI input', 'FCB1010 IN');
  await app.pick('MIDI output', 'FCB1010 OUT');
  await app.click('Connect', '.synth');
  await wait(4000);
  await app.openSynth();
  await app.click('Fetch sound', '.synth');
  if (!(await app.waitMessage(/Fetched/))) throw new Error('Fetch failed');
  await app.js(`[...document.querySelectorAll('.left [role=tab]')].find(t=>t.textContent.trim()==='Play')?.click()`);
  console.log('READY: playing repeated ascending notes for 20 seconds, then a held note cut by Panic.');
  for (let round=0;round<8;round++) {
    for (const code of ['KeyZ','KeyC','KeyB']) { await key(code,'keydown'); await wait(550); await key(code,'keyup'); await wait(250); }
  }
  await key('KeyZ','keydown');
  await wait(1500);
  if (!(await app.click('Panic', '.aud'))) throw new Error('Panic missing');
  console.log('PANIC pressed while a note was held; no keyup yet.');
  await wait(2500);
  await key('KeyZ','keyup');
  if (app.pageErrors.length) throw new Error(app.pageErrors.join('\n'));
  console.log('AUDITION UI SENT; owner sound confirmation required');
} finally {
  try { await app.click('Panic', '.aud'); } finally { app.stop(); }
}
