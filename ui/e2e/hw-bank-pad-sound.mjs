// Owner scenario: before Read from synth, pad clicks must not play; after sync, pad changes
// must put different temporary patches on the XP-60 matching the synced User bank.
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { startApp, wait, root } from './hw-lib.mjs';

const assert = (ok, label) => {
  if (!ok) throw new Error(label);
  console.log(`PASS ${label}`);
};

const tempName = async (app) =>
  app.js(`(async()=>{
    const bytes = await window.__TAURI_INTERNALS__.invoke('midi_fetch_temp_doc',{kind:'patch'});
    const u8 = Uint8Array.from(bytes);
    let s = '';
    for (let i = 0; i < 12; i++) {
      const c = u8[i];
      if (c >= 32 && c < 127) s += String.fromCharCode(c);
    }
    return s.trimEnd();
  })()`);

const clickSlot = async (app, index) => {
  const ok = await app.js(`(()=>{const b=[...document.querySelectorAll('.grid .slot')][${index}]; if(!b) return false; b.click(); return true})()`);
  assert(ok, `clicked bank slot index ${index}`);
};

const outDir = join(root, 'target', 'hardware-acceptance');
mkdirSync(outDir, { recursive: true });
const log = [];
const say = (line) => {
  console.log(line);
  log.push(line);
};

const app = await startApp(9335);
try {
  assert(await app.click('Patch', 'header'), 'Patch workspace');
  await app.openSynth();
  await app.pick('MIDI input', 'FCB1010 IN');
  await app.pick('MIDI output', 'FCB1010 OUT');
  assert(await app.click('Connect', '.synth'), 'Connect');
  let status = null;
  for (let i = 0; i < 30; i++) {
    await wait(500);
    await app.openSynth();
    status = await app.text('.synth .ok');
    if (/XP-60/.test(status ?? '')) break;
  }
  assert(/XP-60/.test(status ?? ''), `connected (${JSON.stringify(status)})`);
  say(`connected: ${status}`);

  assert(await app.click('Bank', 'header'), 'Bank workspace');
  await wait(800);
  assert(await app.js(`!!document.querySelector('.grid')`), 'Bank grid visible');
  assert(await app.js(`!!document.querySelector('.follow-pill')`), 'LIVE follow pill while connected');

  await app.startMidiTrace();

  // --- Before sync: pad switch must not Program-Change or preview ---
  const beforeTemp = await tempName(app);
  await clickSlot(app, 0);
  await wait(1200);
  await clickSlot(app, 9);
  await wait(1200);
  let trace = await app.midiTrace();
  const beforeSelects = trace.filter((t) => t.command === 'midi_select_sound');
  const beforeNotes = trace.filter((t) => t.command === 'midi_note');
  const beforeFrames = trace.filter((t) => t.command === 'midi_send_frame');
  say(`before sync: selects=${beforeSelects.length} notes=${beforeNotes.length} frames=${beforeFrames.length} temp='${beforeTemp}'`);
  assert(beforeSelects.length === 0, 'before sync: no Program Change');
  assert(beforeNotes.length === 0, 'before sync: no preview note');
  assert(beforeFrames.length === 0, 'before sync: no temp audition');
  assert((await tempName(app)) === beforeTemp, 'before sync: temporary patch unchanged');

  // --- Sync keyboard bank into the app ---
  assert(await app.click('Read from synth', 'body'), 'Read from synth');
  let readDone = false;
  for (let i = 0; i < 300; i++) {
    await wait(1000);
    const busy = await app.js(`[...document.querySelectorAll('button')].some(b=>/Read from synth/.test(b.textContent)&&b.disabled)`);
    const filled = await app.js(`[...document.querySelectorAll('.grid .slot')].filter(b=>{const t=(b.textContent||'').trim(); return t && t!=='—';}).length`);
    if (i % 5 === 0) say(`read ${i}s busy=${busy} filledVisible=${filled}`);
    if (!busy && filled >= 8) {
      readDone = true;
      break;
    }
  }
  assert(readDone, 'bank read finished with named slots');

  const named = await app.js(`(()=>{
    const slots=[...document.querySelectorAll('.grid .slot')];
    const out=[];
    for (let i=0;i<slots.length;i++){
      const t=(slots[i].textContent||'').replace(/\\s+/g,' ').trim();
      if(t && t!=='—') out.push({i,t});
    }
    return out;
  })()`);
  say(`named pads (first 8): ${JSON.stringify(named.slice(0, 8))}`);
  const a = named[0];
  const b = named.find((x) => {
    const an = a.t.replace(/^[a-b]?\d+\s*/i, '').trim();
    const bn = x.t.replace(/^[a-b]?\d+\s*/i, '').trim();
    return bn && bn !== an;
  });
  assert(a && b, `two differently named synced pads (${a?.t} / ${b?.t})`);
  say(`after sync comparing pad ${a.i} '${a.t}' vs pad ${b.i} '${b.t}'`);

  await clickSlot(app, a.i);
  await wait(6000);
  const nameA = await tempName(app);
  say(`synced pad ${a.i} UI='${a.t}' temp='${nameA}'`);

  await clickSlot(app, b.i);
  await wait(6000);
  const nameB = await tempName(app);
  say(`synced pad ${b.i} UI='${b.t}' temp='${nameB}'`);

  assert(nameA !== nameB, `after sync: temp names differ ('${nameA}' vs '${nameB}')`);

  trace = await app.midiTrace();
  const selects = trace.filter((t) => t.command === 'midi_select_sound');
  const frames = trace.filter((t) => t.command === 'midi_send_frame');
  const notes = trace.filter((t) => t.command === 'midi_note');
  say(`after sync totals: select_sound=${selects.length} frames=${frames.length} notes=${notes.length}`);
  assert(selects.length >= 2, 'after sync: Program Change for synced pads');
  assert(frames.length >= 2, 'after sync: temp SysEx for synced pads');
  assert(notes.length >= 2, 'after sync: preview notes');

  await app.js(`window.__TAURI_INTERNALS__.invoke('midi_select_sound',{channel:0,msb:81,lsb:0,program:3})`);
  await wait(500);

  say('H-070 SYNC-THEN-PAD LIVE PASS');
  writeFileSync(join(outDir, 'hw-bank-pad-sound.txt'), log.join('\n') + '\n', 'utf8');
} catch (e) {
  try {
    say(`TRACE on fail: ${JSON.stringify((await app.midiTrace()).slice(-30))}`);
  } catch {
    /* ignore */
  }
  say(`FAIL ${e instanceof Error ? e.message : String(e)}`);
  writeFileSync(join(outDir, 'hw-bank-pad-sound.txt'), log.join('\n') + '\n', 'utf8');
  throw e;
} finally {
  app.stop();
}
