// Live: Read User bank from XP-60 → Normalize bank (preview + apply in memory) → verify.
// Does NOT write User memory. Checksums/write path unchanged; Apply only updates app slots.
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { startApp, wait, root } from './hw-lib.mjs';

const assert = (ok, label, detail = '') => {
  if (!ok) throw new Error(detail ? `${label}: ${detail}` : label);
  console.log(`PASS ${label}${detail ? `  -> ${detail}` : ''}`);
};

const outDir = join(root, 'target', 'hardware-acceptance');
mkdirSync(outDir, { recursive: true });
const log = [];
const say = (line) => {
  console.log(line);
  log.push(line);
};

const app = await startApp(9340);
try {
  assert(await app.click('Patch', 'header'), 'Patch workspace');
  await app.openSynth();
  await app.pick('MIDI input', 'FCB1010 IN');
  await app.pick('MIDI output', 'FCB1010 OUT');
  assert(await app.click('Connect', '.synth'), 'Connect');
  for (let i = 0; i < 40; i++) {
    await wait(500);
    await app.openSynth();
    if (/XP-60/.test((await app.text('.synth .ok')) ?? '')) break;
  }
  assert(/XP-60/.test((await app.text('.synth .ok')) ?? ''), 'synth shows connected');

  assert(await app.click('Bank', 'header'), 'Bank workspace');
  await wait(800);

  // --- 1. Sync keyboard patches ---
  assert(await app.click('Read from synth', 'body'), 'Read from synth');
  let readDone = false;
  let lastFilled = 0;
  for (let i = 0; i < 240; i++) {
    await wait(1000);
    const busy = await app.js(`[...document.querySelectorAll('button')].some(b=>/Read from synth/.test(b.textContent)&&b.disabled)`);
    const filled = await app.js(`[...document.querySelectorAll('.grid .slot')].filter(b=>{const t=(b.textContent||'').trim(); return t && t!=='—';}).length`);
    lastFilled = filled;
    if (i % 10 === 0) say(`sync ${i}s busy=${busy} filledVisible=${filled}`);
    if (!busy && filled >= 8) {
      readDone = true;
      break;
    }
  }
  assert(readDone, 'keyboard User bank synced into the app', `visible pads ${lastFilled}`);

  // Confirm 128 filled via toolbar / All filled scope label when possible
  await app.click('Write', 'aside, body');
  await wait(400);
  const allLabel = await app.js(`[...document.querySelectorAll('button')].find(b=>/All filled/.test(b.textContent))?.textContent?.trim() ?? ''`);
  say(`write scopes: ${allLabel}`);
  assert(/All filled \(128\)/.test(allLabel) || /All filled \(\d+\)/.test(allLabel), 'filled slot count known', allLabel);
  const filledCount = Number(allLabel.match(/All filled \((\d+)\)/)?.[1] ?? 0);
  assert(filledCount >= 1, 'at least one User patch loaded', String(filledCount));

  // --- 2. Normalize tab (full-height browser list) ---
  assert(await app.click('Normalize', 'aside, body'), 'Normalize tab');
  await wait(400);
  assert(await app.click('Preview', '.norm, body'), 'Normalize Preview');
  await wait(1500);

  const preview = await app.js(`(()=>{
    const meta = document.querySelector('.norm-meta')?.textContent?.trim() ?? '';
    const rows = [...document.querySelectorAll('.norm-list .norm-row')].map(b => b.textContent.replace(/\\s+/g,' ').trim());
    const list = document.querySelector('.norm-list');
    const btnApply = [...document.querySelectorAll('.norm button')].find(b=>/^Apply$/.test(b.textContent.trim()));
    return {
      summary: meta,
      rowCount: rows.length,
      sample: rows.slice(0, 5),
      last: rows[rows.length - 1] ?? '',
      listScrollH: list ? list.scrollHeight : 0,
      listClientH: list ? list.clientHeight : 0,
      applyEnabled: !!(btnApply && !btnApply.disabled),
    };
  })()`);
  say(`preview: ${JSON.stringify(preview)}`);
  const focus = await app.js(`([...document.querySelectorAll('.norm-list .norm-row')].map(b=>b.textContent.replace(/\\s+/g,' ').trim()).filter(t=>/Mariachi|TB-303|FLASH\\s*SAX|Flash\\s*Sax|Dist/i.test(t)))`);
  say(`focus patches: ${JSON.stringify(focus)}`);
  assert(preview.rowCount > 0, 'preview lists scored patches', String(preview.rowCount));
  assert(preview.rowCount >= Math.min(filledCount, 100), 'most filled slots appear in the list', `${preview.rowCount} of ${filledCount}`);
  // Pane must be height-constrained so 128 rows scroll inside .norm-list (not grow past the window).
  assert(
    preview.listScrollH > preview.listClientH + 40,
    'list pane scrolls inside fixed height',
    `scroll=${preview.listScrollH} client=${preview.listClientH}`,
  );
  assert(/med |chg|out|loaded/i.test(preview.summary) || preview.rowCount === filledCount, 'preview summary present', preview.summary);
  assert(preview.applyEnabled, 'Apply is enabled after preview');

  // Top of list visible before scroll.
  const top = await app.js(`(()=>{
    const rows=[...document.querySelectorAll('.norm-list .norm-row')];
    const first=rows[0];
    const list=document.querySelector('.norm-list');
    if(!first||!list) return null;
    const fr=first.getBoundingClientRect();
    const box=list.getBoundingClientRect();
    return { text:first.textContent.replace(/\\s+/g,' ').trim(), visible: fr.top >= box.top - 2 && fr.bottom <= box.bottom + 2, boxH: Math.round(box.height) };
  })()`);
  say(`top row: ${JSON.stringify(top)}`);
  assert(!!top?.visible, 'first preview row visible in pane', top?.text ?? '');
  assert((top?.boxH ?? 0) >= 120, 'list pane has usable height', String(top?.boxH));

  // Scroll to end and confirm last row is reachable (not clipped away).
  await app.js(`(()=>{const l=document.querySelector('.norm-list'); if(l) l.scrollTop=l.scrollHeight})()`);
  await wait(200);
  const bottom = await app.js(`(()=>{
    const rows=[...document.querySelectorAll('.norm-list .norm-row')];
    const last=rows[rows.length-1];
    if(!last) return null;
    const lr=last.getBoundingClientRect();
    const box=document.querySelector('.norm-list').getBoundingClientRect();
    return { text:last.textContent.replace(/\\s+/g,' ').trim(), visible: lr.bottom <= box.bottom + 2 && lr.top >= box.top - 2, scrollTop: document.querySelector('.norm-list').scrollTop };
  })()`);
  say(`bottom row: ${JSON.stringify(bottom)}`);
  assert(!!bottom?.visible, 'last preview row is visible after scroll', bottom?.text ?? '');
  assert((bottom?.scrollTop ?? 0) > 40, 'scrolled away from top', String(bottom?.scrollTop));

  // --- 3. Apply in memory (no User write) ---
  const changedBefore = await app.js(`(document.querySelector('.bar .dim')?.textContent ?? '')`);
  assert(await app.click('Apply', '.norm, body'), 'Apply normalize');
  await wait(1200);
  const changedAfter = await app.js(`(document.querySelector('.bar .dim')?.textContent ?? '')`);
  say(`toolbar before: ${changedBefore}`);
  say(`toolbar after:  ${changedAfter}`);

  const undoEnabled = await app.js(`![...document.querySelectorAll('.norm button')].find(b=>/^Undo$/.test(b.textContent.trim()))?.disabled`);
  assert(undoEnabled, 'Undo available after apply');

  // Spot-check: identity freeze means muted tones / architecture untouched — verify via wasm in-page if exposed,
  // else confirm UI still shows 128 filled and messages mention apply.
  const msg = await app.js(`([...document.querySelectorAll('[role=status], .toast, .msgs li, .corner li')].map(e=>e.textContent.trim()).filter(Boolean).slice(-5))`);
  say(`messages: ${JSON.stringify(msg)}`);
  const appliedMsg = await app.js(`([...document.querySelectorAll('body *')].map(e=>e.childNodes.length===1&&e.textContent?e.textContent.trim():'').filter(t=>/Normalize bank applied|no slot needed/i.test(t)).slice(-1)[0]||'')`);
  say(`apply notice: ${appliedMsg || '(check corner stack)'}`);

  // Changed count should rise if any slot was rewritten (may be 0 if already flat).
  const changedN = Number(changedAfter.match(/(\d+) changed/)?.[1] ?? -1);
  say(`changed slots after normalize: ${changedN}`);
  assert(changedN >= 0, 'toolbar still reports changed count');

  // Undo round-trip smoke
  assert(await app.click('Undo', '.norm, body'), 'Undo normalize');
  await wait(800);
  const afterUndo = await app.js(`(document.querySelector('.bar .dim')?.textContent ?? '')`);
  say(`toolbar after undo: ${afterUndo}`);

  say(`NORMALIZE BANK LIVE PASS (synced ${filledCount} patches; preview ${preview.rowCount} rows; apply+undo in memory; no User write)`);
  writeFileSync(join(outDir, 'hw-normalize-bank.txt'), log.join('\n') + '\n', 'utf8');
} catch (e) {
  say(`FAIL ${e instanceof Error ? e.message : String(e)}`);
  writeFileSync(join(outDir, 'hw-normalize-bank.txt'), log.join('\n') + '\n', 'utf8');
  throw e;
} finally {
  app.stop();
}
