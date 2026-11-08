// Hardware check 4 (the Bank workspace, PERMANENT writes to User slot 128 only, restored afterwards): read the bank from the instrument, select patches on the
// instrument, put a patch in slot 128, run the write steps (back up all 128, write, read back), open the backup it made, write it back, export .syx and .mid, and
// back up all memory. Needs User Memory Protect OFF. Run `read_slots` / `compare_bank` afterwards to check the instrument independently.
//   node e2e/hw-bank.mjs
import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs';
import { join } from 'node:path';
import { startApp, wait } from './hw-lib.mjs';
import { initSync, Doc, PatchDoc, smfToSyx } from '../src/wasm/pkg/xppatch.js';

initSync({ module: readFileSync(new URL('../src/wasm/pkg/xppatch_bg.wasm', import.meta.url)) });
const validateBank = (file) => {
  if (!file || !existsSync(file)) return false;
  const bytes = readFileSync(file);
  for (let slot = 0; slot < 128; slot++) {
    const patch = PatchDoc.fromSyxSlot(bytes, slot);
    patch.free();
  }
  return true;
};
const validateMemory = (bytes) => {
  for (const [kind, count] of [['performance', 32], ['rhythm', 2]]) {
    for (let slot = 0; slot < count; slot++) Doc.fromSyxSlot(kind, bytes, slot).free();
  }
  for (let slot = 0; slot < 128; slot++) PatchDoc.fromSyxSlot(bytes, slot).free();
  return true;
};

if (process.argv[2] === '--verify-exports') {
  const dir = process.argv[3];
  if (!dir) throw new Error('pass the retained export folder');
  validateBank(join(dir, 'xppatch-user-bank.syx'));
  if (!Buffer.from(smfToSyx(readFileSync(join(dir, 'xppatch-user-bank.mid')))).equals(readFileSync(join(dir, 'xppatch-user-bank.syx')))) throw new Error('MIDI export differs');
  validateMemory(readFileSync(join(dir, 'xppatch-user-memory.syx')));
  console.log('EXPORTS PASSED: bank 128 slots, MIDI byte equality, memory 128 patches + 32 performances + 2 rhythm sets');
  process.exit(0);
}

const results = [];
const check = (ok, what, detail = '') => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${what}${detail ? '  -> ' + detail : ''}`);
  results.push(ok);
  if (!ok) throw new Error(`Bank acceptance stopped: ${what}`);
};
const app = await startApp();
const toolbar = () => app.js(`document.querySelector('.bar .dim')?.textContent?.trim() ?? ''`);
const working = () => app.js(`!!document.querySelector('.prog')`);
const names = () => app.js(`[...document.querySelectorAll('.grid .slot .nm')].map(e=>e.textContent.trim())`);
const waitIdle = async (ms) => {
  await wait(1500);
  for (let t = 0; t < ms; t += 500) {
    if (!(await working())) return true;
    await wait(500);
  }
  return false;
};
const rightTab = (name) => app.js(`(()=>{const t=[...document.querySelectorAll('.right [role=tab], aside [role=tab]')].find(x=>x.textContent.trim()===${JSON.stringify(name)}); if(t){t.click(); return true} return false})()`);
const press = (text) => app.js(`(()=>{const b=[...document.querySelectorAll('.work button, .right button, aside button')].find(x=>x.textContent.trim().startsWith(${JSON.stringify(text)})&&!x.disabled); if(b){b.click(); return true} return false})()`);
const changedCount = async () => Number((await toolbar()).match(/(\d+) changed/)?.[1] ?? -1);

try {
  await app.click('Patch', 'header');
  await app.openSynth();
  await app.pick('MIDI input', 'FCB1010 IN');
  await app.pick('MIDI output', 'FCB1010 OUT');
  await app.click('Connect', '.synth');
  await wait(4000);
  await app.openSynth();
  await app.click('Fetch sound', '.synth');
  check(!!(await app.waitMessage(/Fetched/)), 'connected; the editor holds the keyboard\'s sound (to put in slot 128)');

  // Ensure this is a real changed-content write, even if the keyboard already holds slot 128.
  // Keep this edit local: Bank selection below may replace the instrument's temporary sound.
  await app.js(`(()=>{const s=document.querySelector('[role=switch][aria-label="Live edit to the temporary patch"]'); if(s?.getAttribute('aria-checked')==='true') s.click()})()`);
  await app.js(`(()=>{const k=document.querySelector('[role=slider][aria-label="Tone Level"]'); if(!k) throw new Error('Tone Level missing'); const v=Number(k.getAttribute('aria-valuenow')); k.dispatchEvent(new KeyboardEvent('keydown',{key:v>0?'ArrowDown':'ArrowUp',bubbles:true}))})()`);

  await app.click('Bank', 'header');
  await wait(800);
  check(!!(await app.js(`document.querySelector('.grid')`)), 'the Bank workspace opens');

  // 1. Read from synth
  let t0 = Date.now();
  check(await press('Read from synth'), 'Read from synth can be pressed');
  const done1 = await waitIdle(180000);
  const secs = Math.round((Date.now() - t0) / 1000);
  const n1 = (await names()).length;
  check(done1 && n1 === 64, 'the bank is read and the grid fills', `${secs} s, ${n1} slots shown for sub-group a`);
  await app.js(`[...document.querySelectorAll('.pad')].find(b=>b.textContent.trim()==='b')?.click()`);
  await wait(300);
  check(await app.js(`document.querySelector('.grid')?.getAttribute('aria-label') === 'User patches b'`), 'subgroup b opens');
  const bNames = await names();
  await app.js(`[...document.querySelectorAll('.pad')].find(b=>b.textContent.trim()==='a')?.click()`);
  await wait(300);
  const aNames = await names();
  check(aNames.length === 64 && bNames.length === 64, 'both subgroups contain 64 readable slots', `${aNames.slice(0, 3).join(', ')} … ${bNames[63]}`);
  check((await changedCount()) === 0, 'right after reading, nothing differs from the instrument', await toolbar());
  console.log('read time:', secs, 's');

  // 2. Select patches on the instrument (the owner confirms what the display does)
  await app.js(`[...document.querySelectorAll('.grid .slot')][4]?.click()`);
  await wait(1500);
  await app.js(`[...document.querySelectorAll('.grid .slot')][11]?.click()`);
  await wait(1500);
  console.log('selected slot 5 then slot 12 while connected: the owner should have seen the display change to Brass Sect 1, then to Marimba');

  // 3. Put the editor's patch in slot 128 and write it
  await app.js(`[...document.querySelectorAll('.pad')].find(b=>b.textContent.trim()==='b')?.click()`);
  await wait(300);
  await app.js(`[...document.querySelectorAll('.grid .slot')][63]?.click()`);
  await wait(500);
  check(await app.js(`document.querySelector('.lcdkey')?.textContent?.trim() === 'PATCH 128'`), 'the permanent write target is User slot 128');
  await rightTab('Slot');
  check(await press('Put the editor'), 'the editor\'s patch can be put in the selected slot (128)');
  await wait(500);
  check((await changedCount()) === 1, 'the bank shows one changed slot', await toolbar());

  await rightTab('Write');
  check(await press('1. Back up all 128'), 'step 1 (back up all 128 and prepare) can be pressed');
  t0 = Date.now();
  let armedText = '';
  for (let i = 0; i < 400 && !/Backup saved/.test(armedText); i++) {
    await wait(500);
    armedText = (await app.js(`document.querySelector('.confirm')?.textContent?.replace(/\\s+/g,' ') ?? ''`)) ?? '';
  }
  console.log('confirmation:', armedText.slice(0, 260));
  const backup = armedText.match(/Backup saved: (.+?\.syx)/)?.[1];
  check(validateBank(backup), 'the backup on disk decodes all 128 patches before anything is written', `${backup} (${backup && existsSync(backup) ? statSync(backup).size : 0} bytes, ${Math.round((Date.now() - t0) / 1000)} s)`);
  check(/Replace 1 slot: 128/.test(armedText), 'it asks before writing and names slot 128');

  check(await press('2. Write 1 slot'), 'step 2 (write) can be pressed');
  const wrote = await app.waitMessage(/Wrote 1 slot|Stopped|failed/, 120000);
  check(/Wrote 1 slot .* matches/.test(wrote ?? ''), 'the slot is written and read back equal', String(wrote));
  check((await changedCount()) === 0, 'afterwards nothing differs from the instrument', await toolbar());

  // 4. Open the backup it made and write the original back
  await rightTab('Backups');
  await wait(800);
  const file = backup?.split('\\').pop() ?? '';
  check(await app.js(`(()=>{const b=[...document.querySelectorAll('[aria-label="Backup files"] button')].find(x=>x.textContent.includes(${JSON.stringify(file)})); if(b){b.click(); return true} return false})()`), 'the new backup is listed and can be opened in the grid', file);
  await wait(2500);
  check((await changedCount()) === 1, 'opened, the grid differs from the instrument in exactly slot 128', await toolbar());
  await rightTab('Write');
  check(await press('1. Back up all 128'), 'step 1 again, to put the original back');
  armedText = '';
  for (let i = 0; i < 400 && !/Backup saved/.test(armedText); i++) {
    await wait(500);
    armedText = (await app.js(`document.querySelector('.confirm')?.textContent?.replace(/\\s+/g,' ') ?? ''`)) ?? '';
  }
  check(/Replace 1 slot/.test(armedText), 'it asks again for one slot');
  check(await press('2. Write 1 slot'), 'step 2 again');
  const back = await app.waitMessage(/Wrote 1 slot|Stopped|failed/, 120000);
  check(/Wrote 1 slot .* matches/.test(back ?? ''), 'the original patch is written back and read back equal', String(back));

  // 5. Exports
  const before = new Set(readdirSync(app.saves));
  check(await press('Export .syx'), 'Export .syx can be pressed');
  await wait(2500);
  check(await press('Export .mid'), 'Export .mid can be pressed');
  await wait(2500);
  const files = readdirSync(app.saves).filter((f) => !before.has(f));
  const syx = files.find((f) => f.endsWith('.syx'));
  const mid = files.find((f) => f.endsWith('.mid'));
  const syxBytes = syx ? readFileSync(join(app.saves, syx)) : Buffer.alloc(0);
  const midBytes = mid ? readFileSync(join(app.saves, mid)) : Buffer.alloc(0);
  check(syx && validateBank(join(app.saves, syx)), 'the bank exports as a .syx decoding all 128 patches', `${syx} ${syxBytes.length} bytes`);
  check(midBytes.length > 128 * 589 && midBytes.toString('latin1', 0, 4) === 'MThd', 'the bank exports as a Standard MIDI File', `${mid} ${midBytes.length} bytes`);
  check(Buffer.from(smfToSyx(midBytes)).equals(syxBytes), 'the MIDI file contains exactly the exported bank SysEx');

  // 6. Back up all memory
  t0 = Date.now();
  const before2 = new Set(readdirSync(app.saves));
  check(await press('Back up all memory'), 'Back up all memory can be pressed');
  const done6 = await waitIdle(600000);
  await wait(3000);
  const mem = readdirSync(app.saves).find((f) => !before2.has(f) && f.endsWith('.syx'));
  const memBytes = mem ? readFileSync(join(app.saves, mem)) : Buffer.alloc(0);
  const frames = memBytes.filter((b) => b === 0xf0).length;
  check(done6 && validateMemory(memBytes), 'all memory backup decodes 128 patches, 32 performances and 2 rhythm sets', `${mem} ${memBytes.length} bytes, ${frames} messages, ${Math.round((Date.now() - t0) / 1000)} s`);
  check(app.pageErrors.length === 0, 'no script errors in the window', app.pageErrors.slice(0, 2).join(' | '));
} finally {
  app.stop();
}
console.log(results.every(Boolean) ? 'ALL PASSED' : 'SOME FAILED');
process.exit(results.every(Boolean) ? 0 : 1);
