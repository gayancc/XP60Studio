// Capture the actual desktop Tools > Init result without connecting to MIDI.
import { readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { initSync, PatchDoc } from '../src/wasm/pkg/xppatch.js';
import { startApp, wait } from './hw-lib.mjs';

const target = process.argv[2];
if (!target) throw new Error('pass captured .blocks output path');
initSync({ module: readFileSync(new URL('../src/wasm/pkg/xppatch_bg.wasm', import.meta.url)) });
const app = await startApp(9337);
try {
  if (!(await app.click('Patch', 'header'))) throw new Error('Patch workspace missing');
  if (!(await app.click('File', 'header'))) throw new Error('File menu missing');
  if (!(await app.click('New'))) throw new Error('New patch missing');
  if (!(await app.js(`(()=>{const t=[...document.querySelectorAll('.left [role=tab]')].find(t=>t.textContent.trim()==='Tools');t?.click();return !!t})()`))) throw new Error('Tools tab missing');
  if (!(await app.click('Init', '.tools'))) throw new Error('Init tool missing');
  await wait(300);
  await app.js(`(()=>{for(let i=0;i<32;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
  if (!(await app.click('File', 'header'))) throw new Error('File menu missing');
  if (!(await app.click('Export patch as .syx'))) throw new Error('Export patch missing');
  const saved = await app.waitMessage(/^Saved /, 2000);
  const file = saved ? saved.slice(6) : readdirSync(app.saves).find((name) => name.endsWith('.syx'));
  if (!file) throw new Error(`export did not finish; notices=${JSON.stringify(await app.messages())}; saves=${JSON.stringify(readdirSync(app.saves))}`);
  const doc = PatchDoc.fromSyxSlot(readFileSync(join(app.saves, file)), 0);
  try {
    const bytes = Buffer.from(doc.toBlocks());
    writeFileSync(target, bytes, { flag: 'wx' });
    console.log(`DESKTOP NEUTRAL INIT: '${doc.name().trimEnd()}', ${bytes.length} bytes captured to ${target}`);
  } finally {
    doc.free();
  }
  if (app.pageErrors.length) throw new Error(`desktop script errors: ${app.pageErrors.join('; ')}`);
} finally {
  app.stop();
}
process.exit(0);
