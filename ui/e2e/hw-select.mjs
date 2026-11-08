// H-070 native Bank half: with "Select on the instrument" on, clicking User slots sends Bank Select + Program Change.
// Preset groups (PR-A/B/C/GM) are covered by the MIDI-tool select_probe; Bank UI is USER-only after the Bank redesign.
// Restores temporary Nice Piano (PR-A 004) afterward via midi_select_sound. Owner LCD confirmation remains separate.
import { startApp, wait } from './hw-lib.mjs';

const assert = (ok, label) => {
  if (!ok) throw new Error(label);
  console.log(`PASS ${label}`);
};

const app = await startApp();
try {
  assert(await app.click('Patch', 'header'), 'Patch workspace');
  await app.openSynth();
  await app.pick('MIDI input', 'FCB1010 IN');
  await app.pick('MIDI output', 'FCB1010 OUT');
  assert(await app.click('Connect', '.synth'), 'Connect');
  let status = null;
  for (let i = 0; i < 20; i++) {
    await wait(500);
    await app.openSynth();
    status = await app.text('.synth .ok');
    if (/XP-60 · ID 17/.test(status ?? '')) break;
  }
  assert(/XP-60 · ID 17/.test(status ?? ''), `connected (${JSON.stringify(status)})`);

  assert(await app.click('Bank', 'header'), 'Bank workspace');
  await wait(800);
  assert(await app.js(`!!document.querySelector('.grid')`), 'Bank grid visible');

  // Ensure follow is on (defaults true; assert and force if needed).
  const followOn = await app.js(`(()=>{
    const t=document.querySelector('[role=switch][aria-label="Select the patch on the instrument too"], [role=switch][name="Select the patch on the instrument too"]')
      || [...document.querySelectorAll('[role=switch]')].find(s=>/Select on the instrument|Select the patch on the instrument/i.test(s.getAttribute('aria-label')||s.textContent||''));
    if(!t) return 'missing';
    if(t.getAttribute('aria-checked')==='false') t.click();
    return t.getAttribute('aria-checked');
  })()`);
  assert(followOn === 'true' || followOn === true, `Select on the instrument on (got ${JSON.stringify(followOn)})`);

  await app.startMidiTrace();

  // Slot 0 = USER 1 (PC 0), slot 9 = USER 10 (PC 9), slot 127 needs subgroup b.
  await app.js(`[...document.querySelectorAll('.grid .slot')][0]?.click()`);
  await wait(500);
  await app.js(`[...document.querySelectorAll('.grid .slot')][9]?.click()`);
  await wait(500);
  await app.js(`[...document.querySelectorAll('.pad')].find(b=>b.textContent.trim()==='b')?.click()`);
  await wait(300);
  await app.js(`[...document.querySelectorAll('.grid .slot')][63]?.click()`); // USER 128
  await wait(500);

  const selects = (await app.midiTrace()).filter((t) => t.command === 'midi_select_sound');
  console.log('select commands:', JSON.stringify(selects));
  assert(selects.length >= 3, `at least 3 midi_select_sound (${selects.length})`);

  const argsOf = (t) => (Array.isArray(t.args) ? { channel: t.args[0], msb: t.args[1], lsb: t.args[2], program: t.args[3] } : t.args);
  const found = (msb, lsb, program) =>
    selects.some((t) => {
      const a = argsOf(t);
      return a.msb === msb && a.lsb === lsb && a.program === program;
    });
  assert(found(80, 0, 0), 'USER 1 -> CC0=80 CC32=0 PC=0');
  assert(found(80, 0, 9), 'USER 10 -> CC0=80 CC32=0 PC=9');
  assert(found(80, 0, 127), 'USER 128 -> CC0=80 CC32=0 PC=127');

  // Restore Nice Piano (PR-A 004) so later checks keep the session temp baseline.
  await app.js(`window.__TAURI_INTERNALS__.invoke('midi_select_sound',{channel:0,msb:81,lsb:0,program:3})`);
  await wait(600);
  assert(app.pageErrors.length === 0, 'no script errors');
  console.log('H-070 NATIVE BANK USER SELECT PASS (preset groups via select_probe; owner LCD remains)');
} finally {
  app.stop();
}
