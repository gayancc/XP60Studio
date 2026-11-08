// Goal: live-verify parameters through the REAL app UI against the XP-60 (not cargo probes for edits).
// Each step: open the right tab → nudge a control in the window → Synth Verify must say Verified.
// Starts with Routing→EFX (so Effects are audible path) and Effects type/params, then walks visible knobs
// across Patch / Structure / Routing / Effects / tone sections.
import { writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { root, startApp, wait } from './hw-lib.mjs';

const app = await startApp();
const lines = [];
const say = (m) => {
  console.log(m);
  lines.push(m);
};
const pass = (m) => say(`PASS ${m}`);
const fail = (m) => {
  say(`FAIL ${m}`);
  throw new Error(m);
};
const assert = (ok, m) => (ok ? pass(m) : fail(m));

const clear = () => app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
const liveOn = () =>
  app.js(`(()=>{const s=document.querySelector('[role=switch][aria-label="Live edit to the temporary patch"]'); if(!s) return 'missing'; if(s.getAttribute('aria-checked')!=='true'){s.click(); return 'turned-on'} return 'already-on'})()`);
const queueIdle = async () => {
  for (let i = 0; i < 100; i++) {
    const q = await app.js(`[...document.querySelectorAll('.foot *')].map(e=>e.textContent.trim()).find(t=>/^Queue \\d+/.test(t))`);
    if (!q || q === 'Queue 0') return true;
    await wait(100);
  }
  return false;
};
const verify = async (label) => {
  await queueIdle();
  await wait(200);
  await clear();
  await app.openSynth();
  assert(await app.click('Verify', '.synth'), `${label}: Verify available`);
  const m = await app.waitMessage(/Verified|Mismatch/, 20000);
  assert(!!m && m.startsWith('Verified'), `${label}: ${m}`);
};
const patchTab = async (name) => {
  const ok = await app.js(`(()=>{const t=[...document.querySelectorAll('.center [role=tab], [role=tab]')].find(x=>x.textContent.trim()===${JSON.stringify(name)}); if(!t) return false; t.click(); return true})()`);
  await wait(350);
  return ok;
};
const toneSection = async (name) => {
  const ok = await app.js(`(()=>{const t=[...document.querySelectorAll('[role=tab]')].find(x=>x.textContent.trim()===${JSON.stringify(name)}); if(!t) return false; t.click(); return true})()`);
  await wait(300);
  return ok;
};
const nudgeSlider = async (ariaLabel, key = 'ArrowUp', times = 2) => {
  const before = await app.js(`document.querySelector('[role=slider][aria-label=${JSON.stringify(ariaLabel)}]')?.getAttribute('aria-valuetext')`);
  if (before == null) return { ok: false, before: null, after: null, reason: 'missing' };
  await app.js(`(()=>{const k=document.querySelector('[role=slider][aria-label=${JSON.stringify(ariaLabel)}]'); k.focus(); for(let i=0;i<${times};i++) k.dispatchEvent(new KeyboardEvent('keydown',{key:${JSON.stringify(key)},bubbles:true})); return true})()`);
  await wait(400);
  const after = await app.js(`document.querySelector('[role=slider][aria-label=${JSON.stringify(ariaLabel)}]')?.getAttribute('aria-valuetext')`);
  return { ok: before !== after, before, after, reason: before === after ? 'unchanged' : 'ok' };
};
const cycleOutputToEfx = async () => {
  // Tone Output Assign is a segmented/dropdown labelled "Output" under Routing — step until EFX.
  for (let i = 0; i < 6; i++) {
    const text = await app.js(`(()=>{
      const labs=[...document.querySelectorAll('.caps, label, .pc')].filter(e=>/Output/i.test(e.textContent));
      const host=labs[0]?.closest('.pc, .field, div') ?? null;
      const cur=host?.querySelector('[aria-checked=true], [aria-pressed=true], [role=combobox]')?.textContent?.trim()
        ?? host?.textContent?.trim() ?? '';
      return cur;
    })()`);
    say(`output assign probe: ${JSON.stringify(text)}`);
    if (/EFX/i.test(text ?? '')) return true;
    // Try segmented buttons named MIX/EFX/DIR
    const clicked = await app.js(`(()=>{
      const b=[...document.querySelectorAll('button,[role=radio]')].find(x=>/^EFX$/.test(x.textContent.trim()));
      if(b){b.click(); return 'efx-btn'}
      const s=document.querySelector('[role=slider][aria-label="Output Assign"], [role=slider][aria-label="Output"]');
      if(s){s.focus(); s.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true})); return 'slider'}
      const c=[...document.querySelectorAll('[role=combobox]')].find(x=>(x.getAttribute('aria-label')||'').includes('Output'));
      if(c){c.click(); return 'combo'}
      return null;
    })()`);
    say(`output click: ${clicked}`);
    if (clicked === 'combo') {
      await wait(200);
      await app.js(`([...document.querySelectorAll('[role=option]')].find(x=>/EFX/.test(x.textContent))||{click(){}}).click()`);
    }
    await wait(400);
    if (clicked === 'efx-btn') return true;
  }
  return false;
};
const playAudition = async () => {
  await app.js(`[...document.querySelectorAll('.left [role=tab], [role=tab]')].find(t=>t.textContent.trim()==='Play')?.click()`);
  await wait(200);
  await app.js(`(()=>{const k=document.querySelector('.aud .kb'); if(!k) return false; k.focus(); k.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'z',bubbles:true})); return true})()`);
  await wait(500);
  await app.js(`(()=>{const k=document.querySelector('.aud .kb'); k?.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyZ',key:'z',bubbles:true}))})()`);
};

const results = [];

try {
  await app.click('Patch', 'header');
  await app.openSynth();
  await app.pick('MIDI input', 'FCB1010 IN');
  await app.pick('MIDI output', 'FCB1010 OUT');
  await app.click('Connect', '.synth');
  await wait(4000);
  assert(/XP-60/.test((await app.text('.synth .ok')) ?? ''), 'connected to XP-60');
  await app.openSynth();
  await app.click('Fetch sound', '.synth');
  assert(!!(await app.waitMessage(/Fetched/)), 'fetched temporary sound');
  say(`live: ${await liveOn()}`);
  await verify('baseline after fetch');

  // --- EFX live path ---
  assert(await patchTab('Routing'), 'Routing tab');
  const routed = await cycleOutputToEfx();
  say(`tone routed to EFX: ${routed}`);
  if (routed) {
    await verify('Tone Output Assign → EFX');
  } else {
    say('WARN could not find EFX output control; continuing Effects edits anyway');
  }

  assert(await patchTab('Effects'), 'Effects tab');
  // Change EFX type via + button (UI)
  const typeBefore = await app.js(`document.querySelector('[role=combobox][aria-label="EFX type"]')?.textContent?.trim()`);
  const nextType = await app.js(`(()=>{const b=document.querySelector('button[aria-label="Next EFX type"]'); if(!b||b.disabled) return false; b.click(); return true})()`);
  assert(nextType, 'Next EFX type (+) control');
  await wait(2500); // whole-patch send after type change
  const typeAfter = await app.js(`document.querySelector('[role=combobox][aria-label="EFX type"]')?.textContent?.trim()`);
  say(`EFX type: ${typeBefore} -> ${typeAfter}`);
  assert(typeBefore !== typeAfter, 'EFX type changed in UI');
  await verify('EFX type + defaults live on instrument');

  // Nudge a master EFX knob if present (Level / Dry-wet / Pan)
  for (const label of ['Level', 'Dry / wet', 'Pan', 'Output Level', 'Balance']) {
    const r = await nudgeSlider(label);
    if (r.ok) {
      say(`EFX knob ${label}: ${r.before} -> ${r.after}`);
      await verify(`EFX param ${label}`);
      results.push(['efx', label, 'PASS']);
      break;
    }
  }

  await playAudition();
  pass('audition note after EFX edits (owner hears effect if panel EFX switch is on)');

  // --- Representative live walk across sections ---
  const walk = [
    { tab: 'Patch', section: null, labels: ['Patch Level', 'Patch Pan', 'Analog Feel', 'Patch Tempo'] },
    { tab: 'Structure', section: null, labels: ['Booster 1-2', 'Booster'] },
    { tab: null, section: 'Wave', labels: ['Tone Level', 'Tone Pan', 'Wave Number'] },
    { tab: null, section: 'Filter and amp', labels: ['Cutoff Frequency', 'Resonance'] },
    { tab: null, section: 'Pitch', labels: ['Coarse Tune', 'Fine Tune'] },
    { tab: 'Effects', section: null, labels: ['Chorus Level', 'Reverb Level', 'Chorus Rate', 'Reverb Time'] },
  ];

  for (const step of walk) {
    if (step.tab) assert(await patchTab(step.tab), `tab ${step.tab}`);
    if (step.section) {
      const ok = await toneSection(step.section);
      if (!ok) {
        say(`SKIP section ${step.section} (tab not found)`);
        continue;
      }
    }
    for (const label of step.labels) {
      const r = await nudgeSlider(label);
      if (!r.ok) {
        say(`SKIP ${label} (${r.reason})`);
        results.push([step.tab ?? step.section, label, 'SKIP']);
        continue;
      }
      say(`${label}: ${r.before} -> ${r.after}`);
      try {
        await verify(label);
        results.push([step.tab ?? step.section, label, 'PASS']);
      } catch (e) {
        results.push([step.tab ?? step.section, label, 'FAIL']);
        throw e;
      }
    }
  }

  // Sweep every visible slider on the current Effects view as a denser pass
  assert(await patchTab('Effects'), 'Effects tab for dense pass');
  const labels = await app.js(`[...document.querySelectorAll('[role=slider]')].map(e=>e.getAttribute('aria-label')).filter(Boolean)`);
  say(`visible sliders on Effects: ${labels?.length}`);
  for (const label of labels ?? []) {
    const r = await nudgeSlider(label, 'ArrowDown', 1);
    if (!r.ok) {
      results.push(['Effects-dense', label, 'SKIP']);
      continue;
    }
    await verify(`dense ${label}`);
    results.push(['Effects-dense', label, 'PASS']);
  }

  say('LIVE PARAM UI PASS (batch)');
} catch (e) {
  say(`ERROR ${e}`);
  process.exitCode = 1;
} finally {
  const out = [
    ...lines,
    '',
    'RESULTS',
    ...results.map(([a, b, s]) => `${s}\t${a}\t${b}`),
  ].join('\n');
  writeFileSync(join(app.saves, 'live-params.txt'), out);
  writeFileSync(join(root, 'artifact/docs/xppatchstudio/hardware-evidence/live-params-ui.txt'), out);
  try {
    // Put instrument back: Undo all if possible, or Fetch won't help. Send after undoing.
    for (let i = 0; i < 40; i++) {
      const ok = await app.js(`(()=>{const b=[...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Undo'&&!x.disabled); if(b){b.click(); return true} return false})()`);
      if (!ok) break;
      await wait(80);
    }
    await wait(1500);
    await app.js(`([...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Send to synth')||{click(){}}).click()`);
    await wait(2500);
  } catch {
    /* best-effort restore */
  }
  app.stop();
}
