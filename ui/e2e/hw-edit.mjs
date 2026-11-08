// Hardware check 2 (temporary memory only): live edits, Undo, Send to synth, Write gap and Chunk, through the real app against the connected XP-60.
// Everything goes to the keyboard's temporary patch. The sound is put back at the end by undoing every edit and sending it again.
//   node e2e/hw-edit.mjs
import { readFileSync } from 'node:fs';
import { startApp, wait } from './hw-lib.mjs';

const results = [];
const check = (ok, what, detail = '') => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${what}${detail ? '  -> ' + detail : ''}`);
  results.push(ok);
};
const app = await startApp();
const baseline = Buffer.from(readFileSync(new URL('../../target/hardware-acceptance/2026-11-07-codex/temp-patch.blocks', import.meta.url)));
const readTemp = async () => Buffer.from(await app.js(`window.__TAURI_INTERNALS__.invoke('midi_fetch_temp_patch')`));
const requireCheck = (ok, what) => { if (!ok) throw new Error(what); console.log(`PASS  ${what}`); };
const revealStatus = async () => {
  await app.js(`document.activeElement?.blur()`);
  await app.send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: 2, y: 2 });
};
const knob = (label, index = 0) => `document.querySelectorAll('[role=slider][aria-label=${JSON.stringify(label)}]')[${index}]`;
const value = (label, index = 0) => app.js(`${knob(label, index)}?.getAttribute('aria-valuetext')`);
const press = (label, key, times, index = 0) =>
  app.js(`(()=>{const k=${knob(label, index)}; if(!k) return false; k.focus(); for(let i=0;i<${times};i++) k.dispatchEvent(new KeyboardEvent('keydown',{key:${JSON.stringify(key)},bubbles:true})); return true})()`);
const queueIdle = async () => {
  for (let i = 0; i < 80; i++) {
    const q = await app.js(`[...document.querySelectorAll('.foot *')].map(e=>e.textContent.trim()).find(t=>/^Queue \\d+/.test(t))`);
    if (q === 'Queue 0') return true;
    await wait(100);
  }
  return false;
};
const synthButton = async (text) => {
  await app.openSynth();
  return app.click(text, '.synth');
};
const verify = async () => {
  await queueIdle();
  await wait(300);
  await clearMessages();
  await synthButton('Verify');
  await revealStatus();
  const m = await app.waitMessage(/Verified|Mismatch|changed while/, 15000);
  return m ?? '';
};
const chip = () => app.js(`[...document.querySelectorAll('[class*=chip]')].map(e=>e.textContent.trim()).join(' | ')`);
const liveSwitch = `[...document.querySelectorAll('[role=switch]')].find(e=>(e.getAttribute('aria-label')||'').includes('Live edit')||e.textContent.includes('Live edit'))`;
const toggleLive = () => app.js(`${liveSwitch}?.click()`);
const liveState = () => app.js(`${liveSwitch}?.getAttribute('aria-checked')`);
const sendButton = () => app.js(`(()=>{const b=[...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Send to synth'); if(b&&!b.disabled){b.click(); return true} return false})()`);
const clearMessages = async () => {
  await revealStatus();
  for (let i = 0; i < 32; i++) {
    const clicked = await app.js(`(()=>{const b=document.querySelector('footer .status-msg .x');if(!b)return false;b.click();return true})()`);
    if (!clicked) break;
    await wait(30); // let Svelte replace the current queue entry before clicking again
  }
};

try {
  await app.click('Patch', 'header');
  let picked = false;
  for (let i = 0; i < 15 && !picked; i++) {
    await app.openSynth();
    picked = await app.pick('MIDI input', 'FCB1010 IN');
    if (!picked) await wait(400);
  }
  requireCheck(picked, 'XP input selected');
  requireCheck(await app.pick('MIDI output', 'FCB1010 OUT'), 'XP output selected');
  requireCheck(await app.click('Connect', '.synth'), 'Connect pressed');
  await wait(3000);
  await app.openSynth();
  requireCheck(/XP-60/.test((await app.text('.synth .ok')) ?? ''), 'XP-60 connected');
  requireCheck(await app.click('Fetch sound', '.synth'), 'Fetch pressed');
  await wait(1500);
  requireCheck((await readTemp()).equals(baseline), 'starting temporary patch equals session backup');
  await revealStatus();
  const fetched = await app.waitMessage(/Fetched/);
  check(!!fetched, 'connect and fetch the temporary sound', String(fetched));
  check((await liveState()) === 'true', 'Live edit is on');
  await wait(500);
  console.log('sound:', await app.js(`document.querySelector('.name, [aria-label="Patch name"]')?.textContent ?? document.title`));

  // E1: a live edit of Tone 1's level
  const lvl0 = await value('Tone Level', 0);
  await press('Tone Level', 'ArrowDown', 6, 0);
  const lvl1 = await value('Tone Level', 0);
  check(lvl0 !== lvl1, 'a knob moves in the window', `${lvl0} -> ${lvl1}`);
  check((await verify()).startsWith('Verified'), 'the live edit reached the instrument (Verify matches)');
  console.log('chip:', await chip());

  // E2: other kinds of value, all live
  for (const [label, key, n] of [['Wave Number', 'ArrowUp', 3], ['Coarse Tune', 'ArrowUp', 2], ['Tone Pan', 'ArrowDown', 4]]) {
    const before = await value(label, 0);
    await press(label, key, n, 0);
    const after = await value(label, 0);
    await clearMessages();
    const v = await verify();
    check(before !== after && v.startsWith('Verified'), `${label}: ${before} -> ${after}, live, verified`, v);
  }

  // E3: Undo while live edit is on sends the old values
  let undone = 0;
  for (let i = 0; i < 4; i++) {
    const ok = await app.js(`(()=>{const b=[...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Undo'&&!x.disabled); if(b){b.click(); return true} return false})()`);
    if (ok) undone++;
    await wait(300);
  }
  await clearMessages();
  const vu = await verify();
  check(undone > 0 && vu.startsWith('Verified'), `Undo x${undone} with Live edit on is sent too`, vu);

  // E4: Live edit off: edits stay in the app; Verify says so; Send to synth puts them on the instrument
  await toggleLive();
  await wait(300);
  check((await liveState()) === 'false', 'Live edit can be turned off');
  const beforeOff = await readTemp();
  await press('Tone Level', 'ArrowDown', 5, 0);
  await wait(450);
  check((await readTemp()).equals(beforeOff), 'Live edit off leaves the instrument temporary bytes untouched');
  await clearMessages();
  const vOff = await verify();
  check(vOff.startsWith('Mismatch'), 'with Live edit off the instrument differs from the app (Verify says Mismatch)', vOff);
  check(await sendButton(), 'Send to synth is available and pressed');
  await queueIdle();
  await wait(1500);
  await clearMessages();
  const vSent = await verify();
  check(vSent.startsWith('Verified'), 'after Send to synth the instrument matches the app', vSent);

  // E5: Write gap and Chunk change how a whole sound is sent, and it still arrives intact
  const spin = (name) => `document.querySelector('[role=spinbutton][aria-label=${JSON.stringify(name)}]')`;
  await app.openSynth();
  const gap0 = await app.js(`${spin('Gap between writes in milliseconds')}?.getAttribute('aria-valuenow')`);
  await app.js(`(()=>{const s=${spin('Gap between writes in milliseconds')}; s.focus(); for(let i=0;i<6;i++) s.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true}))})()`);
  await wait(500);
  const gap1 = await app.js(`${spin('Gap between writes in milliseconds')}?.getAttribute('aria-valuenow')`);
  await app.js(`(()=>{const s=${spin('Most data bytes in one message when a whole sound is sent')}; s.focus(); for(let i=0;i<3;i++) s.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowDown',bubbles:true}))})()`);
  const chunk = await app.js(`${spin('Most data bytes in one message when a whole sound is sent')}?.getAttribute('aria-valuenow')`);
  check(gap0 !== gap1, 'the Write gap changes', `${gap0} -> ${gap1} ms; chunk ${chunk}`);
  await sendButton();
  await queueIdle();
  await wait(3000);
  await clearMessages();
  const vGap = await verify();
  check(vGap.startsWith('Verified'), 'a whole sound sent with a longer gap and a smaller chunk arrives intact', vGap);
  // restore the defaults
  await app.openSynth();
  await app.js(`(()=>{const s=${spin('Gap between writes in milliseconds')}; s.focus(); s.dispatchEvent(new KeyboardEvent('keydown',{key:'Home',bubbles:true}))})()`);
  await app.js(`(()=>{const s=${spin('Most data bytes in one message when a whole sound is sent')}; s.focus(); s.dispatchEvent(new KeyboardEvent('keydown',{key:'End',bubbles:true}))})()`);
  await wait(400);
  const restoredGap = await app.js(`${spin('Gap between writes in milliseconds')}?.getAttribute('aria-valuenow')`);
  const restoredChunk = await app.js(`${spin('Most data bytes in one message when a whole sound is sent')}?.getAttribute('aria-valuenow')`);
  check(restoredGap === '20' && restoredChunk === '128', 'gap/chunk restored to 20 ms / 128 B', `${restoredGap} / ${restoredChunk}`);
  await toggleLive();
  check((await liveState()) === 'true', 'Live edit can be turned back on');

  // E6: put the original sound back: undo everything, send, verify
  let n = 0;
  for (let i = 0; i < 40; i++) {
    const ok = await app.js(`(()=>{const b=[...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Undo'&&!x.disabled); if(b){b.click(); return true} return false})()`);
    if (!ok) break;
    n++;
    await wait(120);
  }
  await sendButton();
  await queueIdle();
  await wait(2000);
  await clearMessages();
  const vBack = await verify();
  check(vBack.startsWith('Verified'), `the original sound is back on the instrument (undid ${n} step(s), sent, verified)`, vBack);
  check(app.pageErrors.length === 0, 'no script errors in the window', app.pageErrors.slice(0, 2).join(' | '));
} finally {
  app.stop();
}
console.log(results.every(Boolean) ? 'ALL PASSED' : 'SOME FAILED');
process.exit(results.every(Boolean) ? 0 : 1);
