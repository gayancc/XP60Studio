// H-051c remaining: pointer click on the audition keyboard sends Note On then Note Off; Y position sets velocity.
// Does not claim audible owner confirmation for mouse (computer-key audible already recorded).
import { startApp, wait } from './hw-lib.mjs';

const assert = (ok, label) => {
  if (!ok) throw new Error(label);
  console.log(`PASS ${label}`);
};

/** Trusted mouse click at a fraction of the audition piano SVG (CDP Input — synthetic PointerEvent breaks setPointerCapture). */
async function clickPiano(app, xFrac, yFrac) {
  const hit = await app.js(`(()=>{
    const svg=document.querySelector('.aud .kb svg');
    if(!svg) return null;
    const r=svg.getBoundingClientRect();
    return {x:r.left+r.width*${xFrac}, y:r.top+r.height*${yFrac}, w:r.width, h:r.height};
  })()`);
  if (!hit) return null;
  await app.send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: hit.x, y: hit.y });
  await app.send('Input.dispatchMouseEvent', {
    type: 'mousePressed',
    x: hit.x,
    y: hit.y,
    button: 'left',
    buttons: 1,
    clickCount: 1,
  });
  await wait(120);
  await app.send('Input.dispatchMouseEvent', {
    type: 'mouseReleased',
    x: hit.x,
    y: hit.y,
    button: 'left',
    clickCount: 1,
  });
  return hit;
}

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
  assert(/XP-60 · ID 17/.test(status ?? ''), `connected (got ${JSON.stringify(status)})`);
  await app.startMidiTrace();
  assert(
    await app.js(`(()=>{const t=[...document.querySelectorAll('[role=tab]')].find(x=>x.textContent.trim()==='Play'); if(!t) return false; t.click(); return true})()`),
    'Play tab'
  );
  await wait(500);
  assert(await app.js(`!!document.querySelector('.aud')`), 'Audition panel visible');

  const loudHit = await clickPiano(app, 0.15, 0.85);
  assert(!!loudHit, `loud piano click (${JSON.stringify(loudHit)})`);
  await wait(400);

  let trace = await app.midiTrace();
  let notes = trace.filter((t) => t.command === 'midi_note');
  console.log('note commands after loud click:', JSON.stringify(notes));
  assert(notes.length >= 2, `at least note on and off (${notes.length} midi_note)`);
  const ons = notes.filter((n) => (Array.isArray(n.args) ? n.args[3] === true : n.args?.on === true));
  const offs = notes.filter((n) => (Array.isArray(n.args) ? n.args[3] === false : n.args?.on === false));
  assert(ons.length >= 1, 'note on present');
  assert(offs.length >= 1, 'note off present');
  assert(ons[0].args.velocity >= 100, `lower-key click is loud (${ons[0].args.velocity})`);

  // Hold a note, then Panic (covers held-note silence without keyup).
  const hold = await app.js(`(()=>{
    const svg=document.querySelector('.aud .kb svg');
    if(!svg) return null;
    const r=svg.getBoundingClientRect();
    return {x:r.left+r.width*0.25, y:r.top+r.height*0.75};
  })()`);
  await app.send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: hold.x, y: hold.y });
  await app.send('Input.dispatchMouseEvent', {
    type: 'mousePressed',
    x: hold.x,
    y: hold.y,
    button: 'left',
    buttons: 1,
    clickCount: 1,
  });
  await wait(400);
  assert(await app.click('Panic', '.aud'), 'Panic available');
  await wait(300);
  trace = await app.midiTrace();
  assert(trace.some((t) => t.command === 'midi_all_notes_off'), 'Panic issued midi_all_notes_off');

  // Soft velocity before octave change (same keyboard layout as the loud click).
  await app.click('Panic', '.aud');
  await wait(200);
  const onsBeforeSoft = (await app.midiTrace()).filter((t) => t.command === 'midi_note' && t.args?.on).length;
  const softHit = await clickPiano(app, 0.15, 0.12);
  assert(!!softHit, `soft piano click (${JSON.stringify(softHit)})`);
  await wait(400);
  const onsAfterSoft = (await app.midiTrace()).filter((t) => t.command === 'midi_note' && t.args?.on);
  assert(onsAfterSoft.length === onsBeforeSoft + 1, `upper-key click added one note-on (${onsBeforeSoft} -> ${onsAfterSoft.length})`);
  const soft = onsAfterSoft.at(-1);
  console.log('soft click:', JSON.stringify(soft));
  assert(soft.args.velocity <= 50, `upper click soft (${soft.args.velocity})`);
  assert(soft.args.velocity < ons[0].args.velocity, `upper quieter than lower (${soft.args.velocity} < ${ons[0].args.velocity})`);

  assert(await app.js(`!!document.querySelector('[aria-label="Audition channel"]')`), 'Audition channel control present');
  assert(await app.js(`!!document.querySelector('[aria-label^="Increase Octave"]')`), 'Octave up present');
  assert(await app.js(`!!document.querySelector('[aria-label^="Decrease Octave"]')`), 'Octave down present');
  const beforeOct = await app.js(`document.querySelector('[aria-label="Octave"]')?.textContent?.trim()`);
  assert(await app.js(`(()=>{const b=document.querySelector('.aud button[aria-label^="Increase Octave"]'); if(!b) return false; b.click(); return true})()`), 'Octave up clicked');
  await wait(200);
  const afterOct = await app.js(`document.querySelector('[aria-label="Octave"]')?.textContent?.trim()`);
  assert(beforeOct && afterOct && beforeOct !== afterOct, `octave label changed (${beforeOct} -> ${afterOct})`);

  // Hold a note, step channel: AuditionPanel releases held notes on channel change (no stuck notes).
  const chBefore = await app.js(`document.querySelector('[aria-label="Audition channel"]')?.getAttribute('aria-valuenow')`);
  const hold2 = await app.js(`(()=>{const svg=document.querySelector('.aud .kb svg'); const r=svg.getBoundingClientRect(); return {x:r.left+r.width*0.3,y:r.top+r.height*0.7};})()`);
  await app.send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: hold2.x, y: hold2.y });
  await app.send('Input.dispatchMouseEvent', {
    type: 'mousePressed',
    x: hold2.x,
    y: hold2.y,
    button: 'left',
    buttons: 1,
    clickCount: 1,
  });
  await wait(200);
  const beforeStep = await app.midiTrace();
  const heldOn = beforeStep.filter((t) => t.command === 'midi_note' && t.args?.on).at(-1);
  assert(!!heldOn, 'held note-on before channel step');
  const offsBefore = beforeStep.filter((t) => t.command === 'midi_note' && t.args?.on === false).length;
  assert(
    await app.js(`(()=>{const s=document.querySelector('[aria-label="Audition channel"]'); if(!s) return false; s.focus(); s.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true})); return true})()`),
    'Audition channel stepped while holding'
  );
  await wait(400);
  const chAfter = await app.js(`document.querySelector('[aria-label="Audition channel"]')?.getAttribute('aria-valuenow')`);
  assert(chBefore !== null && chAfter !== null && Number(chAfter) === Number(chBefore) + 1, `channel ${chBefore} -> ${chAfter}`);
  const afterStep = await app.midiTrace();
  const offsAfter = afterStep.filter((t) => t.command === 'midi_note' && t.args?.on === false).length;
  assert(offsAfter > offsBefore, `channel change released held note (offs ${offsBefore} -> ${offsAfter})`);
  await app.send('Input.dispatchMouseEvent', {
    type: 'mouseReleased',
    x: hold2.x,
    y: hold2.y,
    button: 'left',
    clickCount: 1,
  });
  await wait(200);

  // Focus the keyboard, hold a computer-key note, then blur: onfocusout releases all.
  await app.js(`document.querySelector('.aud .kb')?.focus()`);
  await wait(100);
  const offsBeforeBlur = (await app.midiTrace()).filter((t) => t.command === 'midi_note' && t.args?.on === false).length;
  await app.js(`document.querySelector('.aud .kb')?.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'z',bubbles:true}))`);
  await wait(200);
  assert(
    (await app.midiTrace()).some((t) => t.command === 'midi_note' && t.args?.on && t.args?.note !== undefined),
    'computer key sounded before blur'
  );
  await app.js(`(()=>{const kb=document.querySelector('.aud .kb'); kb?.dispatchEvent(new FocusEvent('focusout',{bubbles:true})); document.body.focus?.();})()`);
  await wait(300);
  const offsAfterBlur = (await app.midiTrace()).filter((t) => t.command === 'midi_note' && t.args?.on === false).length;
  assert(offsAfterBlur > offsBeforeBlur, `focus blur released held note (offs ${offsBeforeBlur} -> ${offsAfterBlur})`);

  assert(app.pageErrors.length === 0, 'no script errors');
  console.log('H-051c MOUSE/TRACE PASS (owner audible for mouse still optional; computer-key audible already recorded)');
} finally {
  try {
    await app.click('Panic', '.aud');
  } finally {
    app.stop();
  }
}
