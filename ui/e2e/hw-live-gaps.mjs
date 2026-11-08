// Fill remaining live-param gaps: steppers, envelope graph stages, key-map ranges,
// all four Tone cards, Patch name, Structure 3–4. Real app UI → Verify only.
import { writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { root, startApp, wait } from './hw-lib.mjs';

const app = await startApp();
const lines = [];
const results = [];
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
  await wait(120);
  await clear();
  await app.openSynth();
  assert(await app.click('Verify', '.synth'), `${label}: Verify`);
  const m = await app.waitMessage(/Verified|Mismatch/, 25000);
  assert(!!m?.startsWith('Verified'), `${label}: ${m}`);
  results.push(['PASS', label]);
};
const openCenterTab = async (name) => {
  const ok = await app.js(`(()=>{
    for (const p of document.querySelectorAll('[role=tablist]')) {
      const t=[...p.querySelectorAll('[role=tab]')].find(x=>x.textContent.trim()===${JSON.stringify(name)});
      if(t){t.click(); return true}
    }
    return false;
  })()`);
  await wait(400);
  return ok;
};
const nudgeSlider = async (label) => {
  const before = await app.js(`document.querySelector('[role=slider][aria-label=${JSON.stringify(label)}]')?.getAttribute('aria-valuetext')`);
  if (before == null) return false;
  await app.js(`(()=>{const k=document.querySelector('[role=slider][aria-label=${JSON.stringify(label)}]');k.focus();const key=Number(k.getAttribute('aria-valuenow'))>=Number(k.getAttribute('aria-valuemax'))-1?'ArrowDown':'ArrowUp';k.dispatchEvent(new KeyboardEvent('keydown',{key,bubbles:true}))})()`);
  await wait(400);
  return true;
};
const clickIncrease = async (name) => {
  // Stepper: button aria-label="Increase <name>"
  return app.js(`(()=>{
    const b=document.querySelector('button[aria-label=${JSON.stringify('Increase ' + name)}]');
    if(!b||b.disabled) return false;
    b.click(); return true;
  })()`);
};

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

  // --- Patch tab: steppers + name ---
  assert(await openCenterTab('Patch'), 'Patch tab');
  const patchIncreases = await app.js(`[...document.querySelectorAll('main button[aria-label^="Increase "]')].map(b=>b.getAttribute('aria-label').slice(9))`);
  say(`Patch steppers: ${(patchIncreases ?? []).join(', ')}`);
  for (const name of patchIncreases ?? []) {
    if (!(await clickIncrease(name))) continue;
    await wait(500);
    await verify(`Patch stepper ${name}`);
  }
  // Patch name
  {
    const ok = await app.js(`(()=>{
      const i=document.querySelector('input[aria-label="Patch name"]');
      if(!i) return false;
      i.focus(); i.value='LiveGapTest'; i.dispatchEvent(new Event('input',{bubbles:true})); i.blur();
      return true;
    })()`);
    if (ok) {
      await wait(800);
      await verify('Patch name');
    } else say('SKIP Patch name field');
  }

  // --- Envelope graph stages on Wave and pitch / Filter and amp ---
  for (const tab of ['Wave and pitch', 'Filter and amp']) {
    assert(await openCenterTab(tab), tab);
    const envSliders = await app.js(`[...document.querySelectorAll('main [role=slider]')].map(e=>e.getAttribute('aria-label')).filter(l=>/Env|Envelope|Time \\d/i.test(l||''))`);
    say(`${tab} envelope sliders: ${(envSliders ?? []).length}`);
    for (const label of [...new Set(envSliders ?? [])]) {
      if (!(await nudgeSlider(label))) continue;
      await verify(`${tab} ${label}`);
    }
    // Remaining steppers on this tab (keyfollow etc.)
    const incs = await app.js(`[...document.querySelectorAll('main button[aria-label^="Increase "]')].map(b=>b.getAttribute('aria-label').slice(9))`);
    for (const name of [...new Set(incs ?? [])]) {
      if (!(await clickIncrease(name))) continue;
      await wait(450);
      await verify(`${tab} stepper ${name}`);
    }
  }

  // --- LFO steppers/toggles leftover ---
  assert(await openCenterTab('LFO and modulation'), 'LFO tab');
  {
    const incs = await app.js(`[...document.querySelectorAll('main button[aria-label^="Increase "]')].map(b=>b.getAttribute('aria-label').slice(9))`);
    for (const name of [...new Set(incs ?? [])]) {
      if (!(await clickIncrease(name))) continue;
      await wait(450);
      await verify(`LFO stepper ${name}`);
    }
  }

  // --- Zones: keyboard range via KeyMapH pointer drag on Tone 1 lane ---
  assert(await openCenterTab('Zones'), 'Zones');
  {
    const dragged = await app.js(`(()=>{
      const lanes=document.querySelector('.lanes, [aria-label*="key range"]');
      if(!lanes) return 'no-lanes';
      const r=lanes.getBoundingClientRect();
      const x0=r.left+r.width*0.15, y=r.top+8;
      const x1=r.left+r.width*0.35;
      lanes.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,clientX:x0,clientY:y,pointerId:1,buttons:1}));
      lanes.dispatchEvent(new PointerEvent('pointermove',{bubbles:true,clientX:x1,clientY:y,pointerId:1,buttons:1}));
      lanes.dispatchEvent(new PointerEvent('pointerup',{bubbles:true,clientX:x1,clientY:y,pointerId:1,buttons:0}));
      return 'dragged';
    })()`);
    say(`key map drag: ${dragged}`);
    if (dragged === 'dragged') {
      await wait(700);
      await verify('Zones keyboard range drag');
    }
  }

  // --- Structure type 3–4 pair ---
  assert(await openCenterTab('Structure'), 'Structure');
  {
    const opened = await app.js(`(()=>{
      const b=[...document.querySelectorAll('button[aria-label*="structure type"]')].find(x=>/Tones 3/i.test(x.getAttribute('aria-label')||''));
      if(!b) return false; b.click(); return true;
    })()`);
    if (opened) {
      await wait(400);
      const chose = await app.js(`(()=>{
        const cells=[...document.querySelectorAll('[role=dialog] button.cell[data-type]')];
        const next=cells.find(c=>c.getAttribute('aria-pressed')!=='true')||cells[3];
        if(!next) return null; next.click(); return next.dataset.type;
      })()`);
      say(`structure 3-4 type: ${chose}`);
      if (chose) {
        await wait(700);
        await verify(`Structure 3-4 type ${chose}`);
      }
    } else say('SKIP Structure 3-4 trigger');
  }

  // --- All four tones: select card, change Coarse Tune ---
  assert(await openCenterTab('Wave and pitch'), 'Wave and pitch for tones');
  for (const tone of [1, 2, 3, 4]) {
    await app.js(`([...document.querySelectorAll('.card .tag, .card button.tag')].find(b=>b.textContent.includes('Tone ${tone}'))||document.querySelector('button[aria-label="Edit Tone ${tone}"]'))?.click()`);
    await wait(300);
    // ensure tone on
    await app.js(`(()=>{const s=document.querySelector('button.sw[aria-label="Tone ${tone} on"]'); if(s&&s.getAttribute('aria-checked')==='false') s.click()})()`);
    await wait(400);
    const ok = await nudgeSlider('Coarse Tune');
    if (!ok) {
      say(`SKIP Tone ${tone} Coarse Tune`);
      results.push(['SKIP', `Tone ${tone} Coarse Tune`]);
      continue;
    }
    await verify(`Tone ${tone} Coarse Tune`);
  }

  // --- Effects: cycle a few more EFX types (defaults whole-send) ---
  assert(await openCenterTab('Effects'), 'Effects');
  for (let i = 0; i < 3; i++) {
    const ok = await app.js(`(()=>{const b=document.querySelector('button[aria-label="Next EFX type"]'); if(!b||b.disabled) return false; b.click(); return true})()`);
    if (!ok) break;
    await wait(2500);
    const name = await app.js(`document.querySelector('[role=combobox][aria-label="EFX type"]')?.textContent?.trim()`);
    await verify(`EFX type ${name}`);
  }

  say(`GAP FILL DONE PASS=${results.filter((r) => r[0] === 'PASS').length} SKIP=${results.filter((r) => r[0] === 'SKIP').length}`);
} catch (e) {
  say(`ERROR ${e}`);
  process.exitCode = 1;
} finally {
  const out = [...lines, '', 'RESULTS', ...results.map(([s, l]) => `${s}\t${l}`)].join('\n');
  writeFileSync(join(app.saves, 'live-gaps.txt'), out);
  writeFileSync(join(root, 'artifact/docs/xppatchstudio/hardware-evidence/live-gaps-ui.txt'), out);
  for (let i = 0; i < 120; i++) {
    const ok = await app.js(`(()=>{const b=[...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Undo'&&!x.disabled); if(b){b.click(); return true} return false})()`);
    if (!ok) break;
    await wait(35);
  }
  await wait(500);
  await app.js(`([...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Send to synth')||{click(){}}).click()`);
  await wait(3000);
  app.stop();
}
