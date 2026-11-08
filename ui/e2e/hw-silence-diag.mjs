// Diagnose silence after load/New + Tone activate: connect, snapshot temp, New or open bank slot,
// toggle a Tone with Live on, read temp back, play a note, report tone/wave state.
import { writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { startApp, wait } from './hw-lib.mjs';

const invoke = (app, command, args = {}) =>
  app.js(`window.__TAURI_INTERNALS__.invoke(${JSON.stringify(command)},${JSON.stringify(args)})`);

const app = await startApp();
const log = [];
const say = (m) => {
  console.log(m);
  log.push(m);
};

try {
  await app.click('Patch', 'header');
  await app.openSynth();
  await app.pick('MIDI input', 'FCB1010 IN');
  await app.pick('MIDI output', 'FCB1010 OUT');
  await app.click('Connect', '.synth');
  await wait(4000);
  const status = await app.text('.synth .ok');
  say(`status: ${status}`);
  if (!/XP-60/.test(status ?? '')) throw new Error('not connected');

  await app.openSynth();
  await app.click('Fetch sound', '.synth');
  const fetched = await app.waitMessage(/Fetched/);
  say(`fetch: ${fetched}`);

  const before = Buffer.from(await invoke(app, 'midi_fetch_temp_doc', { kind: 'patch' }));
  say(`before name bytes: ${JSON.stringify([...before.slice(0, 12)])} len=${before.length}`);

  // Path A: New patch then activate Tone 2 (first live edit should whole-send).
  await app.click('New', 'header');
  await wait(500);
  const liveOn = await app.js(`!!document.querySelector('[aria-label=\"Live edit to the temporary patch\"][aria-checked=\"true\"], .synth [role=switch][aria-checked=true]')`);
  say(`live control present check: ${liveOn}`);

  // Toggle Tone 2 on via the Tone card switch (second tone).
  const toggled = await app.js(`(()=>{
    const cards=[...document.querySelectorAll('.card')];
    if(cards.length<2) return 'cards='+cards.length;
    const sw=cards[1].querySelector('button.sw,[role=switch]');
    if(!sw) return 'no switch';
    sw.click();
    return 'toggled tone2 aria='+sw.getAttribute('aria-checked');
  })()`);
  say(`tone toggle: ${toggled}`);
  await wait(2500);

  const afterNew = Buffer.from(await invoke(app, 'midi_fetch_temp_doc', { kind: 'patch' }));
  say(`after New+tone2 name bytes: ${JSON.stringify([...afterNew.slice(0, 12)])}`);
  say(`temp equals pre-fetch? ${before.equals(afterNew)}`);
  say(`messages: ${JSON.stringify(await app.messages())}`);

  await invoke(app, 'midi_note', { channel: 0, note: 60, velocity: 100, on: true });
  await wait(400);
  await invoke(app, 'midi_note', { channel: 0, note: 60, velocity: 0, on: false });
  await invoke(app, 'midi_all_notes_off', { channel: 0 });
  say('note burst after New path sent');

  // Restore fetched sound into editor and send whole patch back.
  await app.openSynth();
  await app.click('Fetch sound', '.synth');
  await app.waitMessage(/Fetched/);
  await wait(500);
  // If New left silence on the instrument, Fetch pulls silence — so send Nice by re-loading from before via whole send after opening blocks is hard.
  // Use Send to instrument after we put before bytes back through a temp open if available.
  say('diag complete — inspect log for whether New+tone overwrote temp with empty-name init');
} finally {
  writeFileSync(join(app.saves, 'silence-diag.txt'), log.join('\n'));
  try {
    await app.click('Panic', '.aud');
  } catch {
    /* ignore */
  }
  app.stop();
}
