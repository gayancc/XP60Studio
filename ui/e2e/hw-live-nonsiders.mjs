// Live-verify non-slider patch controls via the real app UI: toggles, segmented, dropdowns,
// Structure picker, Wave picker, Tone on/off. Each change → Synth Verify.
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
  await wait(150);
  await clear();
  await app.openSynth();
  assert(await app.click('Verify', '.synth'), `${label}: Verify available`);
  const m = await app.waitMessage(/Verified|Mismatch/, 25000);
  assert(!!m?.startsWith('Verified'), `${label}: ${m}`);
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
const record = (kind, name, status) => results.push([kind, name, status]);

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

  // Tone card on/off (not a ParamControl toggle)
  {
    const before = await app.js(`document.querySelector('button.sw[aria-label="Tone 2 on"]')?.getAttribute('aria-checked')`);
    assert(before === 'true' || before === 'false', `Tone 2 switch found (${before})`);
    await app.js(`document.querySelector('button.sw[aria-label="Tone 2 on"]')?.click()`);
    await wait(700);
    const after = await app.js(`document.querySelector('button.sw[aria-label="Tone 2 on"]')?.getAttribute('aria-checked')`);
    say(`Tone 2 switch: ${before} -> ${after}`);
    assert(before !== after, 'Tone 2 on/off changed');
    await verify('Tone 2 switch');
    record('tone-switch', 'Tone 2', 'PASS');
    await app.js(`document.querySelector('button.sw[aria-label="Tone 2 on"]')?.click()`);
    await wait(700);
    await verify('Tone 2 switch restore');
  }

  // Wave picker on Tone 1 card
  {
    assert(await openCenterTab('Wave and pitch'), 'Wave and pitch tab');
    const opened = await app.js(`(()=>{
      const b=document.querySelector('button.trig[aria-label^="Wave of Tone 1"]');
      if(!b) return 'no-trigger';
      b.click(); return 'opened';
    })()`);
    say(`wave picker: ${opened}`);
    assert(opened === 'opened', 'Wave picker opens');
    await wait(400);
    const picked = await app.js(`(()=>{
      const opts=[...document.querySelectorAll('[role=listbox] [role=option]:not([aria-disabled=true])')];
      const cur=opts.findIndex(o=>o.classList.contains('hi')||o.getAttribute('aria-selected')==='true');
      const next=opts[(cur+3+opts.length)%Math.max(opts.length,1)] || opts[2] || opts[1];
      if(!next) return 'no-option';
      next.click();
      return next.textContent.trim().slice(0,40);
    })()`);
    say(`wave picked: ${picked}`);
    assert(picked && picked !== 'no-option', 'picked a different wave');
    await wait(2500); // whole-patch send for wave identity
    await verify('Wave picker Tone 1');
    record('wave-picker', 'Tone 1 wave', 'PASS');
  }

  // Structure type picker
  {
    assert(await openCenterTab('Structure'), 'Structure tab');
    const opened = await app.js(`(()=>{
      const b=document.querySelector('button[aria-label*="structure type"]');
      if(!b) return false; b.click(); return true;
    })()`);
    assert(opened, 'Structure dialog opens');
    await wait(400);
    const chose = await app.js(`(()=>{
      const cells=[...document.querySelectorAll('[role=dialog] button.cell[data-type]')];
      const on=cells.find(c=>c.getAttribute('aria-pressed')==='true');
      const next=cells.find(c=>c!==on && c.dataset.type) || cells[2];
      if(!next) return null;
      next.click();
      return next.getAttribute('data-type');
    })()`);
    say(`structure type chosen: ${chose}`);
    assert(!!chose, 'chose another structure type');
    await wait(800);
    await verify(`Structure type ${chose}`);
    record('structure', `type ${chose}`, 'PASS');
  }

  // Walk tabs: toggles, segmented groups, dropdowns in main
  const TABS = ['Zones', 'Wave and pitch', 'Filter and amp', 'LFO and modulation', 'Patch', 'Structure', 'Routing', 'Effects'];
  for (const tab of TABS) {
    assert(await openCenterTab(tab), `open ${tab}`);

    // Segmented radiogroups (skip UI-only "Tones shown")
    const segs = await app.js(`([...document.querySelectorAll('main [role=radiogroup], .center [role=radiogroup]')].map(g=>({
      name: g.getAttribute('aria-label')||'',
      options: [...g.querySelectorAll('[role=radio]')].map(r=>({text:r.textContent.trim(), on:r.getAttribute('aria-checked')==='true'}))
    })))`);
    for (const g of segs ?? []) {
      if (!g.name || /Tones shown|Library view/i.test(g.name)) continue;
      const off = g.options.find((o) => !o.on);
      if (!off) {
        say(`SKIP segmented ${g.name} (only one option)`);
        record('segmented', `${tab}/${g.name}`, 'SKIP');
        continue;
      }
      const clicked = await app.js(`(()=>{
        const g=[...document.querySelectorAll('[role=radiogroup]')].find(x=>(x.getAttribute('aria-label')||'')===${JSON.stringify(g.name)});
        const b=[...g.querySelectorAll('[role=radio]')].find(r=>r.textContent.trim()===${JSON.stringify(off.text)} && r.getAttribute('aria-checked')!=='true');
        if(!b) return false; b.click(); return true;
      })()`);
      if (!clicked) {
        record('segmented', `${tab}/${g.name}`, 'SKIP');
        continue;
      }
      say(`${tab} segmented ${g.name} -> ${off.text}`);
      await wait(500);
      await verify(`${tab} segmented ${g.name}`);
      record('segmented', `${tab}/${g.name}`, 'PASS');
    }

    // Param toggles: role=switch inside .pc (not Live edit / tone cards)
    const toggles = await app.js(`([...document.querySelectorAll('main .pc [role=switch], .center .pc [role=switch]')].map(s=>({
      text: s.textContent.trim() || s.getAttribute('aria-label') || '',
      on: s.getAttribute('aria-checked')==='true'
    })))`);
    const seenT = new Set();
    for (const t of toggles ?? []) {
      if (!t.text || seenT.has(t.text)) continue;
      seenT.add(t.text);
      const ok = await app.js(`(()=>{
        const s=[...document.querySelectorAll('main .pc [role=switch], .center .pc [role=switch]')].find(x=>(x.textContent.trim()||x.getAttribute('aria-label')||'')===${JSON.stringify(t.text)});
        if(!s) return false; s.click(); return true;
      })()`);
      if (!ok) {
        record('toggle', `${tab}/${t.text}`, 'SKIP');
        continue;
      }
      say(`${tab} toggle ${t.text}`);
      await wait(500);
      await verify(`${tab} toggle ${t.text}`);
      record('toggle', `${tab}/${t.text}`, 'PASS');
    }

    // Dropdowns (combobox) — skip MIDI port pickers
    const boxes = await app.js(`([...document.querySelectorAll('main [role=combobox], .center [role=combobox]')].map(c=>({
      name: c.getAttribute('aria-label')||c.textContent.trim(),
      text: c.textContent.trim()
    })))`);
    const seenD = new Set();
    for (const d of boxes ?? []) {
      if (!d.name || /MIDI input|MIDI output|Tones shown/i.test(d.name)) continue;
      if (seenD.has(d.name)) continue;
      seenD.add(d.name);
      const changed = await app.js(`(()=>{
        const c=[...document.querySelectorAll('[role=combobox]')].find(x=>(x.getAttribute('aria-label')||x.textContent.trim())===${JSON.stringify(d.name)} || (x.getAttribute('aria-label')||'')===${JSON.stringify(d.name)});
        if(!c) return 'missing';
        c.click();
        return 'opened';
      })()`);
      if (changed !== 'opened') {
        record('dropdown', `${tab}/${d.name}`, 'SKIP');
        continue;
      }
      await wait(250);
      const pick = await app.js(`(()=>{
        const opts=[...document.querySelectorAll('[role=listbox] [role=option]')];
        if(opts.length<2){ document.body.click(); return 'few'; }
        const cur=opts.findIndex(o=>o.getAttribute('aria-selected')==='true'||o.classList.contains('on'));
        const next=opts[(cur+1+opts.length)%opts.length];
        const label=next.textContent.trim().slice(0,48);
        next.click();
        return label;
      })()`);
      if (pick === 'few' || !pick) {
        say(`SKIP dropdown ${d.name} (${pick})`);
        record('dropdown', `${tab}/${d.name}`, 'SKIP');
        await app.js(`document.body.click()`);
        continue;
      }
      say(`${tab} dropdown ${d.name} -> ${pick}`);
      // EFX type triggers whole-patch send
      await wait(/EFX type/i.test(d.name) ? 2500 : 600);
      await verify(`${tab} dropdown ${d.name}`);
      record('dropdown', `${tab}/${d.name}`, 'PASS');
    }
  }

  say('NON-SLIDER LIVE PARAM WALK PASSED');
} catch (e) {
  say(`ERROR ${e}`);
  process.exitCode = 1;
} finally {
  const counts = { PASS: 0, SKIP: 0, FAIL: 0 };
  for (const [, , s] of results) counts[s] = (counts[s] ?? 0) + 1;
  const out = [
    ...lines,
    '',
    `COUNTS PASS=${counts.PASS} SKIP=${counts.SKIP} FAIL=${counts.FAIL}`,
    'RESULTS',
    ...results.map(([k, n, s]) => `${s}\t${k}\t${n}`),
  ].join('\n');
  writeFileSync(join(app.saves, 'live-nonsiders.txt'), out);
  writeFileSync(join(root, 'artifact/docs/xppatchstudio/hardware-evidence/live-nonsiders-ui.txt'), out);
  for (let i = 0; i < 100; i++) {
    const ok = await app.js(`(()=>{const b=[...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Undo'&&!x.disabled); if(b){b.click(); return true} return false})()`);
    if (!ok) break;
    await wait(40);
  }
  await wait(600);
  await app.js(`([...document.querySelectorAll('header button')].find(x=>x.textContent.trim()==='Send to synth')||{click(){}}).click()`);
  await wait(3000);
  app.stop();
}
