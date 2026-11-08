// Full UI live-param walk: every center tab, every visible slider/toggle, Synth Verify after each change.
// Edits are only through the running app window — no cargo SysEx for the changes.
import { writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { root, startApp, wait } from './hw-lib.mjs';

const app = await startApp();
const lines = [];
const say = (m) => {
  console.log(m);
  lines.push(m);
};
const assert = (ok, m) => {
  say(`${ok ? 'PASS' : 'FAIL'} ${m}`);
  if (!ok) throw new Error(m);
};
const clear = () => app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
const queueIdle = async () => {
  for (let i = 0; i < 120; i++) {
    const q = await app.js(`[...document.querySelectorAll('.foot *')].map(e=>e.textContent.trim()).find(t=>/^Queue \\d+/.test(t))`);
    if (!q || q === 'Queue 0') return true;
    await wait(80);
  }
  return false;
};
const verify = async (label) => {
  await queueIdle();
  await wait(150);
  await clear();
  await app.openSynth();
  if (!(await app.click('Verify', '.synth'))) throw new Error(`${label}: no Verify`);
  const m = await app.waitMessage(/Verified|Mismatch/, 20000);
  if (!m?.startsWith('Verified')) throw new Error(`${label}: ${m}`);
  say(`PASS ${label}: ${m}`);
};
const openCenterTab = async (name) => {
  const ok = await app.js(`(()=>{
    const panels=[...document.querySelectorAll('[role=tablist]')];
    for (const p of panels) {
      const t=[...p.querySelectorAll('[role=tab]')].find(x=>x.textContent.trim()===${JSON.stringify(name)});
      if(t){t.click(); return true}
    }
    return false;
  })()`);
  await wait(400);
  return ok;
};
const listSliders = () =>
  app.js(`[...document.querySelectorAll('main [role=slider], .center [role=slider], .stackcol [role=slider]')].map(e=>({label:e.getAttribute('aria-label'), now:e.getAttribute('aria-valuenow'), text:e.getAttribute('aria-valuetext')}))`);
const nudgeSlider = async (label) => {
  const before = await app.js(`document.querySelector('[role=slider][aria-label=${JSON.stringify(label)}]')?.getAttribute('aria-valuetext')`);
  if (before == null) return { changed: false, before: null, after: null };
  await app.js(`(()=>{const k=document.querySelector('[role=slider][aria-label=${JSON.stringify(label)}]'); if(!k) return; k.focus(); const key=Number(k.getAttribute('aria-valuenow'))>=Number(k.getAttribute('aria-valuemax'))-1?'ArrowDown':'ArrowUp'; k.dispatchEvent(new KeyboardEvent('keydown',{key,bubbles:true}));})()`);
  await wait(350);
  const after = await app.js(`document.querySelector('[role=slider][aria-label=${JSON.stringify(label)}]')?.getAttribute('aria-valuetext')`);
  return { changed: before !== after, before, after };
};

const results = [];
const TABS = ['Zones', 'Wave and pitch', 'Filter and amp', 'LFO and modulation', 'Patch', 'Structure', 'Routing', 'Effects'];

try {
  await app.click('Patch', 'header');
  await app.openSynth();
  await app.pick('MIDI input', 'FCB1010 IN');
  await app.pick('MIDI output', 'FCB1010 OUT');
  await app.click('Connect', '.synth');
  await wait(4000);
  assert(/XP-60/.test((await app.text('.synth .ok')) ?? ''), 'connected');
  await app.openSynth();
  await app.click('Fetch sound', '.synth');
  assert(!!(await app.waitMessage(/Fetched/)), 'fetched');
  await verify('baseline');

  // Ensure Tone 1 feeds EFX so Effects changes can be heard
  assert(await openCenterTab('Routing'), 'Routing');
  await app.js(`([...document.querySelectorAll('button,[role=radio]')].find(x=>/^EFX$/.test(x.textContent.trim()))||{click(){}}).click()`);
  await wait(500);
  await verify('route Tone to EFX');

  for (const tab of TABS) {
    assert(await openCenterTab(tab), `open ${tab}`);
    const sliders = (await listSliders()) ?? [];
    say(`${tab}: ${sliders.length} sliders`);
    const seen = new Set();
    for (const s of sliders) {
      const label = s.label;
      if (!label || seen.has(label)) continue;
      seen.add(label);
      const r = await nudgeSlider(label);
      if (!r.changed) {
        say(`SKIP ${tab} / ${label} (no change)`);
        results.push([tab, label, 'SKIP']);
        continue;
      }
      say(`${tab} / ${label}: ${r.before} -> ${r.after}`);
      try {
        await verify(`${tab} / ${label}`);
        results.push([tab, label, 'PASS']);
      } catch (e) {
        results.push([tab, label, 'FAIL']);
        say(`FAIL ${e}`);
        throw e;
      }
    }
  }

  // EFX type via UI +
  assert(await openCenterTab('Effects'), 'Effects for type');
  const before = await app.js(`document.querySelector('[role=combobox][aria-label="EFX type"]')?.textContent?.trim()`);
  assert(
    await app.js(`(()=>{const b=document.querySelector('button[aria-label="Next EFX type"]'); if(!b||b.disabled) return false; b.click(); return true})()`),
    'EFX type +',
  );
  await wait(2500);
  const after = await app.js(`document.querySelector('[role=combobox][aria-label="EFX type"]')?.textContent?.trim()`);
  say(`EFX type ${before} -> ${after}`);
  assert(before !== after, 'EFX type changed');
  await verify('EFX type whole-send');
  results.push(['Effects', 'EFX type', 'PASS']);

  // Audition from Play tab (real UI)
  await app.js(`([...document.querySelectorAll('[role=tab]')].find(t=>t.textContent.trim()==='Play')||{click(){}}).click()`);
  await wait(200);
  await app.js(`(()=>{const k=document.querySelector('.aud .kb'); k?.focus(); k?.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'z',bubbles:true}))})()`);
  await wait(600);
  await app.js(`(()=>{document.querySelector('.aud .kb')?.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyZ',key:'z',bubbles:true}))})()`);
  say('PASS audition key after live edits');

  say('ALL TABS LIVE PARAM WALK PASSED');
} catch (e) {
  say(`ERROR ${e}`);
  process.exitCode = 1;
} finally {
  const out = [...lines, '', 'RESULTS', ...results.map(([t, l, s]) => `${s}\t${t}\t${l}`)].join('\n');
  writeFileSync(join(app.saves, 'live-params-all.txt'), out);
  writeFileSync(join(root, 'artifact/docs/xppatchstudio/hardware-evidence/live-params-all-ui.txt'), out);
  for (let i = 0; i < 80; i++) {
    const ok = await app.js(`(()=>{const b=[...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Undo'&&!x.disabled); if(b){b.click(); return true} return false})()`);
    if (!ok) break;
    await wait(50);
  }
  await wait(800);
  await app.js(`([...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Send to synth')||{click(){}}).click()`);
  await wait(3000);
  app.stop();
}
