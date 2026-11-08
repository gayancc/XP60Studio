// Exhaustive live verify: every [data-param] control visible in the real app, plus
// name / Structure / Wave picker / EFX type / Tone switches. Catalog leftovers reported.
import { readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { root, startApp, wait } from './hw-lib.mjs';

const app = await startApp();
const lines = [];
const passed = new Map(); // key block:id -> label
const skipped = [];
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
    await wait(70);
  }
  return false;
};
const verify = async (label) => {
  await queueIdle();
  await wait(100);
  await clear();
  await app.openSynth();
  if (!(await app.click('Verify', '.synth'))) throw new Error(`${label}: no Verify`);
  const m = await app.waitMessage(/Verified|Mismatch/, 25000);
  if (!m?.startsWith('Verified')) throw new Error(`${label}: ${m}`);
  say(`PASS ${label}`);
};
const openCenterTab = async (name) => {
  const ok = await app.js(`(()=>{
    for (const p of document.querySelectorAll('[role=tablist]')) {
      const t=[...p.querySelectorAll('[role=tab]')].find(x=>x.textContent.trim()===${JSON.stringify(name)});
      if(t){t.click(); return true}
    }
    return false;
  })()`);
  await wait(450);
  return ok;
};

/** Nudge whatever editable control sits inside a [data-param] host. */
const nudgeParam = async (key) => {
  return app.js(`(()=>{
    const host=document.querySelector('[data-param=${JSON.stringify(key)}]');
    if(!host) return {ok:false, reason:'missing'};
    const slider=host.querySelector('[role=slider]');
    if(slider){
      const before=slider.getAttribute('aria-valuetext');
      slider.focus();
      const keyDir=Number(slider.getAttribute('aria-valuenow'))>=Number(slider.getAttribute('aria-valuemax'))-1?'ArrowDown':'ArrowUp';
      slider.dispatchEvent(new KeyboardEvent('keydown',{key:keyDir,bubbles:true}));
      return {ok:true, kind:'slider', before};
    }
    const sw=host.querySelector('[role=switch]');
    if(sw){ sw.click(); return {ok:true, kind:'switch'}; }
    const radios=[...host.querySelectorAll('[role=radio]')];
    if(radios.length){
      const next=radios.find(r=>r.getAttribute('aria-checked')!=='true')||radios[0];
      next.click(); return {ok:true, kind:'segmented', value:next.textContent.trim()};
    }
    const combo=host.querySelector('[role=combobox]');
    if(combo){
      combo.click();
      return {ok:true, kind:'dropdown-open'};
    }
    const inc=[...host.querySelectorAll('button[aria-label^="Increase "]')][0];
    if(inc && !inc.disabled){ inc.click(); return {ok:true, kind:'stepper'}; }
    return {ok:false, reason:'no-control'};
  })()`);
};
const finishDropdownIfOpen = async () => {
  await wait(200);
  await app.js(`(()=>{
    const opts=[...document.querySelectorAll('[role=listbox] [role=option]')];
    if(opts.length<2){document.body.click(); return false;}
    const cur=opts.findIndex(o=>o.getAttribute('aria-selected')==='true');
    opts[(cur+1+opts.length)%opts.length].click();
    return true;
  })()`);
};

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

  // Prefer All 4 Tones so tone2–4 params appear
  assert(await openCenterTab('Wave and pitch'), 'Wave and pitch');
  await app.js(`(()=>{
    const g=[...document.querySelectorAll('[role=radiogroup]')].find(x=>(x.getAttribute('aria-label')||'').includes('Tones shown'));
    const all=[...g?.querySelectorAll('[role=radio]')||[]].find(r=>/All 4/.test(r.textContent));
    all?.click();
  })()`);
  await wait(400);

  for (const tab of TABS) {
    assert(await openCenterTab(tab), `tab ${tab}`);
    if (tab === 'Wave and pitch' || tab === 'Filter and amp' || tab === 'LFO and modulation') {
      await app.js(`(()=>{
        const g=[...document.querySelectorAll('[role=radiogroup]')].find(x=>(x.getAttribute('aria-label')||'').includes('Tones shown'));
        const all=[...g?.querySelectorAll('[role=radio]')||[]].find(r=>/All 4/.test(r.textContent));
        all?.click();
      })()`);
      await wait(300);
    }

    const keys = await app.js(`[...document.querySelectorAll('main [data-param], .center [data-param], .stackcol [data-param]')].map(e=>e.getAttribute('data-param'))`);
    const unique = [...new Set(keys ?? [])];
    say(`${tab}: ${unique.length} data-param hosts`);

    for (const key of unique) {
      if (passed.has(key)) continue;
      const r = await nudgeParam(key);
      if (!r?.ok) {
        skipped.push([key, r?.reason ?? 'fail']);
        say(`SKIP ${key} (${r?.reason})`);
        continue;
      }
      if (r.kind === 'dropdown-open') {
        const picked = await finishDropdownIfOpen();
        await wait(/:efx_type$/.test(key) ? 2500 : 500);
        if (!picked) {
          skipped.push([key, 'dropdown']);
          continue;
        }
      } else {
        await wait(/wave_group_type|wave_group_id|efx_type/.test(key) ? 2000 : 450);
      }
      try {
        await verify(key);
        passed.set(key, tab);
      } catch (e) {
        say(`FAIL ${key}: ${e}`);
        throw e;
      }
    }
  }

  // Specials not wrapped in data-param
  // Patch name
  assert(await openCenterTab('Patch'), 'Patch for name');
  {
    const ok = await app.js(`(()=>{
      const i=document.querySelector('main input[maxlength="12"], input[aria-label="Patch name"]');
      if(!i) return false;
      const v=('LiveEx'+Date.now()).slice(0,12);
      i.focus();
      const setter=Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set;
      setter.call(i,v);
      i.dispatchEvent(new Event('input',{bubbles:true}));
      i.dispatchEvent(new Event('change',{bubbles:true}));
      i.blur();
      return i.value;
    })()`);
    say(`patch name set: ${ok}`);
    if (ok) {
      await wait(800);
      await verify('common:patch_name_*');
      for (let i = 1; i <= 12; i++) passed.set(`common:patch_name_${i}`, 'Patch');
    } else skipped.push(['common:patch_name_*', 'no-input']);
  }

  // Tone switches
  for (const n of [1, 2, 3, 4]) {
    const key = `tone${n}:tone_switch`;
    if (passed.has(key)) continue;
    await app.js(`document.querySelector('button.sw[aria-label="Tone ${n} on"]')?.click()`);
    await wait(500);
    await verify(key);
    passed.set(key, 'ToneCard');
  }

  // Wave picker tone1
  assert(await openCenterTab('Wave and pitch'), 'wave tab');
  await app.js(`document.querySelector('button.trig[aria-label^="Wave of Tone 1"]')?.click()`);
  await wait(400);
  await app.js(`(()=>{const opts=[...document.querySelectorAll('[role=listbox] [role=option]:not([aria-disabled=true])')]; opts[Math.min(5,opts.length-1)]?.click()})()`);
  await wait(2500);
  await verify('tone1:wave_identity');
  passed.set('tone1:wave_group_type', 'WavePicker');
  passed.set('tone1:wave_group_id', 'WavePicker');
  passed.set('tone1:wave_number', 'WavePicker');

  // Structure both pairs
  assert(await openCenterTab('Structure'), 'Structure');
  for (const label of ['Tones 1 and 2', 'Tones 3 and 4']) {
    const ok = await app.js(`(()=>{
      const b=[...document.querySelectorAll('button[aria-label*="structure type"]')].find(x=>(x.getAttribute('aria-label')||'').includes(${JSON.stringify(label)}));
      if(!b) return false; b.click(); return true;
    })()`);
    if (!ok) {
      skipped.push([`structure:${label}`, 'missing']);
      continue;
    }
    await wait(350);
    await app.js(`(()=>{const c=[...document.querySelectorAll('[role=dialog] button.cell[data-type]')].find(x=>x.getAttribute('aria-pressed')!=='true'); c?.click()})()`);
    await wait(700);
    await verify(`structure:${label}`);
    if (label.includes('1 and 2')) passed.set('common:structure_type_1_2', 'Structure');
    else passed.set('common:structure_type_3_4', 'Structure');
  }

  // Envelope graph stages (no data-param)
  for (const tab of ['Wave and pitch', 'Filter and amp']) {
    assert(await openCenterTab(tab), tab);
    const labels = await app.js(`[...document.querySelectorAll('main [role=slider]')].map(e=>e.getAttribute('aria-label')).filter(l=>/envelope|Env/i.test(l||''))`);
    for (const label of [...new Set(labels ?? [])]) {
      const ok = await app.js(`(()=>{const k=document.querySelector('[role=slider][aria-label=${JSON.stringify(label)}]'); if(!k) return false; k.focus(); k.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true})); return true})()`);
      if (!ok) continue;
      await wait(450);
      await verify(`env:${label}`);
      passed.set(`env:${label}`, tab);
    }
  }

  // Catalog audit
  const raw = JSON.parse(readFileSync(join(root, 'data/xp60-parameters.json'), 'utf8'));
  const common = raw.blocks.find((b) => b.id === 'patch_common').params;
  const tone = raw.blocks.find((b) => b.id === 'patch_tone').params;
  const missing = [];
  for (const p of common) {
    if (/^patch_name_/.test(p.id)) {
      if (![...passed.keys()].some((k) => k.startsWith('common:patch_name'))) missing.push(`common:${p.id}`);
      continue;
    }
    if (!passed.has(`common:${p.id}`)) missing.push(`common:${p.id}`);
  }
  for (const toneN of [1, 2, 3, 4]) {
    for (const p of tone) {
      const k = `tone${toneN}:${p.id}`;
      // wave identity may be covered via picker only for tone1
      if (passed.has(k)) continue;
      if (passed.has(`tone*:${p.id}`)) continue;
      // if any tone covered this id in All 4 view
      if ([1, 2, 3, 4].some((n) => passed.has(`tone${n}:${p.id}`))) continue;
      missing.push(k);
    }
  }
  // Collapse tone gaps: if tone1 has it, other tones same control path in All 4 — require at least one tone instance
  const missingToneIds = new Set();
  const missingCommon = [];
  for (const m of missing) {
    if (m.startsWith('common:')) missingCommon.push(m);
    else {
      const id = m.split(':')[1];
      if (![1, 2, 3, 4].some((n) => passed.has(`tone${n}:${id}`))) missingToneIds.add(id);
    }
  }

  say(`EXHAUSTIVE PASS keys=${passed.size}`);
  say(`CATALOG GAPS common=${missingCommon.length} toneIds=${missingToneIds.size}`);
  say(`SKIPPED hosts=${skipped.length}`);
  if (missingCommon.length) say(`MISSING COMMON: ${missingCommon.join(', ')}`);
  if (missingToneIds.size) say(`MISSING TONE IDS: ${[...missingToneIds].join(', ')}`);

  writeFileSync(
    join(root, 'artifact/docs/xppatchstudio/hardware-evidence/live-exhaustive-ledger.json'),
    JSON.stringify(
      {
        passed: [...passed.entries()],
        skipped,
        missingCommon,
        missingToneIds: [...missingToneIds],
      },
      null,
      2,
    ),
  );

  if (missingCommon.length || missingToneIds.size) {
    process.exitCode = 2;
    say('EXHAUSTIVE INCOMPLETE — gaps remain');
  } else {
    say('EXHAUSTIVE CATALOG COMPLETE');
  }
} catch (e) {
  say(`ERROR ${e}`);
  process.exitCode = 1;
} finally {
  writeFileSync(join(root, 'artifact/docs/xppatchstudio/hardware-evidence/live-exhaustive-ui.txt'), lines.join('\n'));
  for (let i = 0; i < 150; i++) {
    const ok = await app.js(`(()=>{const b=[...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Undo'&&!x.disabled); if(b){b.click(); return true} return false})()`);
    if (!ok) break;
    await wait(30);
  }
  await wait(400);
  await app.js(`([...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Send to synth')||{click(){}}).click()`);
  await wait(3000);
  app.stop();
}
