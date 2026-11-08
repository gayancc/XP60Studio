// Read-only Library workflow: search Brass in the user's actual library and export the ten shown patches as SysEx.
import { existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { initSync, syxNames } from '../src/wasm/pkg/xppatch.js';
import { startApp, wait } from './hw-lib.mjs';

initSync({module:readFileSync(new URL('../src/wasm/pkg/xppatch_bg.wasm',import.meta.url))});
const app=await startApp();
const assert=(ok,label)=>{if(!ok)throw new Error(label);console.log(`PASS ${label}`)};
try{
  assert(await app.click('Library','header'),'native Library workspace opens');
  for(let i=0;i<80;i++){
    if(await app.js(`document.querySelectorAll('ul.rows[aria-label="Stored patches"] li:not(.empty)').length`))break;
    await wait(250);
  }
  assert(await app.js(`(()=>{const q=document.querySelector('input[aria-label="Search names"]');if(!q)return false;q.value='Brass';q.dispatchEvent(new Event('input',{bubbles:true}));return true})()`),'Library search entered');
  await wait(1000);
  const names=await app.js(`[...document.querySelectorAll('ul.rows[aria-label="Stored patches"] li:not(.empty) .name .n')].map(e=>e.textContent.trim())`);
  assert(names.length===10,`search shows ten patches (${names.join(', ')})`);
  assert(await app.click('Export shown','.work .list .bar'),'Library export requested');
  const notice=await app.waitMessage(/^Saved /,20000);
  assert(!!notice,'Library SysEx export saved');
  const file=join(app.saves,notice.slice(6));
  assert(existsSync(file),'export file exists');
  const list=JSON.parse(syxNames(readFileSync(file)));
  assert(list.length===10&&list.every(([slot])=>slot>=0&&slot<10),'export decodes as ten User patches in slots 1-10');
  console.log('LIBRARY EXPORT FILE:',file);
  assert(app.pageErrors.length===0,'no script errors during Library export');
}finally{app.stop()}
