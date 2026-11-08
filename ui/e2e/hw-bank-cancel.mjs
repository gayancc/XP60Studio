// H-048 remaining half: Cancel stops a Bank "Read from synth" within one slot. Read-only.
import { startApp, wait } from './hw-lib.mjs';

const assert = (ok, label) => {
  if (!ok) throw new Error(label);
  console.log(`PASS ${label}`);
};
const app = await startApp();
const working = () => app.js(`!!document.querySelector('.prog')`);
const progress = () =>
  app.js(`document.querySelector('.prog')?.textContent?.replace(/\\s+/g, ' ').trim() ?? ''`);
const press = (text) =>
  app.js(`(()=>{const b=[...document.querySelectorAll('.work button, .right button, aside button')].find(x=>x.textContent.trim().startsWith(${JSON.stringify(text)})&&!x.disabled); if(b){b.click(); return true} return false})()`);

try {
  assert(await app.click('Patch', 'header'), 'Patch workspace opens');
  await app.openSynth();
  await app.pick('MIDI input', 'FCB1010 IN');
  await app.pick('MIDI output', 'FCB1010 OUT');
  assert(await app.click('Connect', '.synth'), 'Connect requested');
  await wait(4000);
  await app.openSynth();
  const status = await app.text('.synth .ok');
  assert(/XP-60 · ID 17/.test(status ?? ''), `connected XP-60 at ID 17 (${status})`);

  assert(await app.click('Bank', 'header'), 'Bank workspace opens');
  await wait(800);
  assert(await press('Read from synth'), 'Read from synth started');

  let sawProgress = '';
  let before = -1;
  // Wait until several slots have completed so Cancel is exercised mid-read, not before the first reply.
  for (let i = 0; i < 240; i++) {
    await wait(250);
    if (!(await working())) break;
    sawProgress = await progress();
    before = Number(sawProgress.match(/(\d+)\s*\/\s*128/)?.[1] ?? -1);
    if (before >= 8 && before < 120) break;
  }
  assert(/\d+\s*\/\s*128/.test(sawProgress), `progress visible before cancel (${sawProgress})`);
  assert(before >= 8 && before < 120, `cancel armed mid-read (done=${before})`);

  assert(await press('Cancel'), 'Cancel pressed');
  let idle = false;
  let lastText = sawProgress;
  let peak = before;
  for (let i = 0; i < 80; i++) {
    await wait(250);
    if (await working()) {
      lastText = await progress();
      const n = Number(lastText.match(/(\d+)\s*\/\s*128/)?.[1] ?? peak);
      if (n > peak) peak = n;
    } else {
      idle = true;
      break;
    }
  }
  assert(idle, `read stopped after Cancel (last progress: ${lastText})`);
  // Cancel may finish the in-flight slot, so at most one beyond the count when Cancel was pressed.
  assert(peak <= before + 1, `stopped within one slot (cancel@${before}, peak=${peak})`);
  const notice = await app.js(`document.querySelector('.msg, [role=status], .stack')?.textContent?.replace(/\\s+/g,' ') ?? ''`);
  const err = await app.js(`document.querySelector('.err, .error, .fail')?.textContent?.replace(/\\s+/g,' ') ?? ''`);
  console.log(`BANK CANCEL: cancel@${before}/128 peak=${peak}/128 notice=${JSON.stringify(notice)} err=${JSON.stringify(err)}`);
  assert(app.pageErrors.length === 0, 'no script errors during bank cancel');
  console.log('H-048 CANCEL PASSED');
} finally {
  app.stop();
}
