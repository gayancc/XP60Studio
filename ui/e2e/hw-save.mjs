// Hardware check 3 (PERMANENT memory, User slot 128 only): the save ceremony in the real app. It reads slot 128 and backs it up, replaces it with the sound the app holds,
// reads it back, then undoes the save from the backup. Needs User Memory Protect OFF. Afterwards compare slot 128 with e2e/hw-slot128.txt from a read_slots run.
//   node e2e/hw-save.mjs
import { existsSync, statSync } from 'node:fs';
import { startApp, wait } from './hw-lib.mjs';

const results = [];
const check = (ok, what, detail = '') => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${what}${detail ? '  -> ' + detail : ''}`);
  results.push(ok);
};
const app = await startApp();
const dialog = (expr) => app.js(`(()=>{const d=document.querySelector('#save-panel'); return d ? (${expr}) : null})()`);
const dlgClick = (text) => dialog(`(()=>{const b=[...d.querySelectorAll('button')].find(x=>x.textContent.trim().startsWith(${JSON.stringify(text)})&&!x.disabled); if(b){b.click(); return true} return false})()`);
const dlgText = () => dialog(`d.textContent.replace(/\\s+/g,' ').trim()`);
try {
  await app.click('Patch', 'header');
  await app.openSynth();
  await app.pick('MIDI input', 'FCB1010 IN');
  await app.pick('MIDI output', 'FCB1010 OUT');
  await app.click('Connect', '.synth');
  await wait(4000);
  await app.openSynth();
  await app.click('Fetch sound', '.synth');
  check(!!(await app.waitMessage(/Fetched/)), 'connected and fetched the sound to save');
  const name = (await app.waitMessage(/Fetched/))?.match(/"(.*)"/)?.[1] ?? '';

  // Open the dialog
  await app.js(`[...document.querySelectorAll('header button')].find(x=>x.textContent.trim().startsWith('Save to synth'))?.click()`);
  await wait(500);
  check(!!(await dlgText()), 'the Save dialog opens');
  // slot 128
  await dialog(`(()=>{const s=d.querySelector('[role=spinbutton][aria-label="User slot"]'); s.focus(); for(let i=0;i<130;i++) s.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true}))})()`);
  await wait(300);
  const slot = await dialog(`d.querySelector('[role=spinbutton][aria-label="User slot"]')?.getAttribute('aria-valuetext')`);
  check(slot === '128', 'the slot is set to 128', String(slot));

  check(await dlgClick('1. Read slot and back it up'), 'step 1 can be pressed');
  let armed = '';
  for (let i = 0; i < 80 && !/holds/.test(armed); i++) {
    await wait(250);
    armed = (await dlgText()) ?? '';
  }
  console.log('dialog:', armed.slice(0, 400));
  check(/128 holds “Jimmee Dee”/.test(armed) || /holds “Jimmee Dee”/.test(armed), 'it reads slot 128 and names what is there', armed.slice(0, 120));
  const backup = armed.match(/Backed up to (.+?\.syx)/)?.[1];
  check(!!backup && existsSync(backup) && statSync(backup).size > 500, 'the backup file of that slot exists on disk', String(backup));
  check(/Cancel/.test(armed) && /2\. Replace/.test(armed), 'it asks before replacing and offers Cancel');

  check(await dlgClick('2. Replace'), 'step 2 (Replace) can be pressed');
  let done = null;
  for (let i = 0; i < 80 && !done; i++) {
    await wait(250);
    done = (await app.messages()).find((m) => /Saved|saved|verified|Verified|mismatch|Mismatch|failed/i.test(m)) ?? null;
  }
  console.log('messages after the save:', JSON.stringify(await app.messages()));
  check(/aved/.test(done ?? '') && !/ismatch|ail/.test(done ?? ''), 'the save is written and read back equal', String(done));

  // Undo from the backup
  await wait(500);
  check(await dlgClick('Undo the last save'), 'Undo the last save… is offered');
  await wait(300);
  const ask = (await dlgText()) ?? '';
  check(/Put .*Jimmee Dee.* back into/.test(ask), 'it asks before putting the old patch back and names it', ask.slice(-260));
  check(await dlgClick('Put the old patch back'), 'confirming puts the old patch back');
  let undone = null;
  for (let i = 0; i < 80 && !undone; i++) {
    await wait(250);
    undone = (await app.messages()).find((m) => /earlier patch is back|MISMATCH|Undo failed/.test(m)) ?? null;
  }
  console.log('messages after the undo:', JSON.stringify(await app.messages()));
  check(/earlier patch is back/.test(undone ?? ''), 'the undo is written and read back', String(undone));
  check(app.pageErrors.length === 0, 'no script errors in the window', app.pageErrors.slice(0, 2).join(' | '));
} finally {
  app.stop();
}
console.log(results.every(Boolean) ? 'ALL PASSED' : 'SOME FAILED');
process.exit(results.every(Boolean) ? 0 : 1);
