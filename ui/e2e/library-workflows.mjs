// Native desktop library checks in a copy chosen through Settings. Never connects MIDI.
// Original storage pointer and original database contents are verified/restored in finally.
import {copyFileSync,existsSync,mkdirSync,mkdtempSync,readFileSync,unlinkSync,writeFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {join,resolve} from 'node:path';
import {initSync,PatchDoc,smfToSyx} from '../src/wasm/pkg/xppatch.js';
import {root,startApp,wait} from './hw-lib.mjs';
initSync({module:readFileSync(new URL('../src/wasm/pkg/xppatch_bg.wasm',import.meta.url))});
const evidence=join(root,'target','hardware-acceptance','2026-11-07-codex');
mkdirSync(evidence,{recursive:true});
const sandbox=mkdtempSync(join(root,'target','library-acceptance-'));
const pointer=join(process.env.APPDATA,'studio.xppatch.app','storage.json');
const originalPointer=existsSync(pointer)?readFileSync(pointer):null;
writeFileSync(join(sandbox,'storage-pointer-before.json'),JSON.stringify({pointer,bytes:originalPointer?.toString('base64')??null}));
const assert=(ok,text)=>{if(!ok)throw new Error(text);console.log(`PASS ${text}`)};
const logicalHash=dir=>execFileSync('python',['-c',`import sqlite3,hashlib,sys;from pathlib import Path;c=sqlite3.connect(Path(sys.argv[1]).as_uri()+'?mode=ro',uri=True);print(hashlib.sha256('\\n'.join(c.iterdump()).encode()).hexdigest());c.close()`,join(dir,'library.sqlite')]).toString().trim();
let app,originalFolder,originalHash;
const input=async(selector,text,key)=>{
  assert(await app.js(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});if(!e)return false;e.focus();e.value=${JSON.stringify(text)};e.dispatchEvent(new Event('input',{bubbles:true}));${key?`e.dispatchEvent(new KeyboardEvent('keydown',{key:${JSON.stringify(key)},bubbles:true}));`:''}return true})()`),`input ${selector}`);
};
const setFile=async(selector,file)=>{
  const doc=(await app.send('DOM.getDocument',{depth:-1})).result.root.nodeId;
  const node=(await app.send('DOM.querySelector',{nodeId:doc,selector})).result.nodeId;
  assert(!!node,'file picker available');await app.send('DOM.setFileInputFiles',{nodeId:node,files:[file]});
};
const rows=()=>app.js(`[...document.querySelectorAll('[aria-label="Stored patches"] li .n')].map(e=>e.textContent.trim())`);
const menu=async title=>{assert(await app.click(title,'header'),`${title} menu opens`);await wait(400)};
try{
  app=await startApp(9335);await menu('Settings');
  originalFolder=await app.text('.storage .now b');assert(!!originalFolder,'original library folder observed');
  originalHash=logicalHash(originalFolder);writeFileSync(join(sandbox,'original-library.sha256'),originalHash);
  await input('[aria-label="Folder for the library and backups"]',sandbox);
  assert(await app.click('Use this folder','.storage'),'storage copy selected through native Settings');
  assert(!!(await app.waitMessage(/library and backups were copied/)),'library and backup copy confirmed');
  assert(existsSync(join(sandbox,'library.sqlite')),'copied database exists');
  assert(logicalHash(sandbox)===originalHash,'copied database contains exactly the original logical data');
  app.stop();app=undefined;
  app=await startApp(9335);await menu('Settings');
  assert(resolve(await app.text('.storage .now b'))===resolve(sandbox),'restarted desktop app uses the copied library');
  await app.click('Library','header');await wait(700);
  const importName=`Codex-Acceptance-${process.pid}`;
  const fixture=join(sandbox,importName+'.syx');copyFileSync(join(root,'tests','golden','xp60-user-bank.syx'),fixture);
  await setFile('input[type=file][aria-label^="Import a .syx"]',fixture);await wait(500);
  assert(await app.click('Import 128','.staged'),'128-patch staged import confirmed');
  for(let i=0;i<100;i++){if(!(await app.js(`!!document.querySelector('.staged')`)))break;await wait(100)}
  await wait(800);
  assert(await app.js(`(()=>{const b=[...document.querySelectorAll('[aria-label="Bulks"] button')].find(e=>e.textContent.trim()===${JSON.stringify(importName)});b?.click();return !!b})()`),'imported bank appears as a named bulk');
  await wait(700);
  const counts=JSON.parse(execFileSync('python',['-c',`import sqlite3,json,sys;c=sqlite3.connect(sys.argv[1]);print(json.dumps(c.execute('select count(*),count(distinct patch_id),min(position),max(position) from bulk_patch where bulk_id=(select id from bulk where name=?)',(sys.argv[2],)).fetchone()));c.close()`,join(sandbox,'library.sqlite'),importName]).toString());
  assert(counts[0]===128&&counts[2]===0&&counts[3]===127,'imported bulk retains every slot position');
  assert((await rows()).length===counts[1],'Library shows each distinct patch once, sharing repeated slots');
  assert(await app.js(`(()=>{const b=document.querySelector('[aria-label="Stored patches"] li .name');b?.click();return !!b})()`),'stored patch selected');await wait(300);
  await input('input[aria-label="Rename patch"]','Codex Test','Enter');await wait(500);
  assert((await rows()).includes('Codex Test'),'patch rename is reflected in the library');
  const star=await app.js(`document.querySelector('[aria-label="Stored patches"] li .star')?.getAttribute('aria-pressed')`);
  assert(await app.js(`(()=>{const b=document.querySelector('[aria-label="Stored patches"] li .star');b?.click();return !!b})()`),'favourite toggled');await wait(400);
  assert(await app.js(`document.querySelector('[aria-label="Stored patches"] li .star')?.getAttribute('aria-pressed')`)!==star,'favourite state changed');
  await input('input[aria-label="Add a tag"]','A-Codex/Verified','Enter');await wait(600);
  assert((await app.text('.detail'))?.includes('A-Codex/Verified'),'nested folder tag assigned');
  await input('input[aria-label="Search names"]','Codex Test');await wait(700);
  assert(JSON.stringify(await rows())===JSON.stringify(['Codex Test']),'name search returns the renamed patch');
  assert(await app.click('Export shown','.work .list'),'filtered SysEx export selected');await wait(700);
  assert(await app.click('Export .mid','.work .list'),'filtered MIDI export selected');await wait(700);
  const syx=readFileSync(join(app.saves,'xppatch-library-bank.syx'));
  const doc=PatchDoc.fromSyxSlot(syx,0);assert(doc.name().trimEnd()==='Codex Test','filtered SysEx contains the renamed patch');doc.free();
  assert(Buffer.from(smfToSyx(readFileSync(join(app.saves,'xppatch-library.mid')))).equals(syx),'library MIDI export decodes byte-identical to SysEx');
  assert(await app.click('Open in editor','.detail'),'library patch opened in Patch editor');await wait(500);
  await menu('File');await app.click('Export patch as .syx');
  assert(!!(await app.waitMessage(/^Saved Codex Test.syx/)),'opened library patch exports under its new name');
  assert(readFileSync(join(app.saves,'Codex Test.syx')).equals(syx),'opened editor patch equals library export');
  // Bulk management and drag/drop use only this copied library and local Bank grid.
  await app.click('Bank','header');await wait(500);
  const libraryTab=async()=>{assert(await app.js(`(()=>{const t=[...document.querySelectorAll('.right [role=tab]')].find(e=>e.textContent.trim()==='Library');t?.click();return !!t})()`),'Bank Library tab opens');await wait(400)};
  await libraryTab();
  const bulkNames=()=>app.js(`[...document.querySelectorAll('.tree .bulk .head .name')].map(e=>e.textContent.trim())`);
  const bulkAction=(name,label,ask=false)=>app.js(`(()=>{const row=[...document.querySelectorAll('.tree .bulk')].find(e=>e.querySelector('.head')?.getAttribute('aria-label')===${JSON.stringify('Bulk '+name)});const b=row&&[...row.querySelectorAll(${JSON.stringify(ask?'.ask button':'button')})].find(e=>e.textContent.trim()===${JSON.stringify(label)}&&!e.disabled);b?.click();return !!b})()`);
  assert(await bulkAction(importName,'Rename'),'bulk Rename opens');
  const renamed=`Codex Bank ${process.pid}`;await input('input[aria-label="Bulk name"]',renamed,'Enter');await wait(500);
  assert((await bulkNames()).includes(renamed),'bulk rename persisted');
  const beforeCopy=await bulkNames();assert(await bulkAction(renamed,'Copy'),'bulk Copy selected');await wait(700);
  const copies=(await bulkNames()).filter(n=>!beforeCopy.includes(n));assert(copies.length===1,'bulk copy creates one separately named bulk');
  const copied=copies[0];
  await app.js(`(()=>{const b=[...document.querySelectorAll('.tree .bulk .chev')].find(e=>e.getAttribute('aria-label')===${JSON.stringify('Open '+copied)});b?.click()})()`);await wait(400);
  assert(await bulkAction(copied,'Export .syx'),'bulk SysEx export selected');
  const exportMessage=await app.waitMessage(new RegExp('^Saved '+copied.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')));assert(!!exportMessage,'bulk export saved');
  const bulkBytes=readFileSync(join(app.saves,exportMessage.slice(6)));
  const originalNames=[];for(let slot=0;slot<128;slot++){const p=PatchDoc.fromSyxSlot(bulkBytes,slot);originalNames.push(p.name().trimEnd());p.free()}
  assert(originalNames.includes('Codex Test'),'bulk copy/export retains the renamed patch');
  const loadBulk=async()=>{assert(await bulkAction(copied,'Load into bank'),'bulk load asks for replacement');assert(await bulkAction(copied,'Replace',true),'bulk load explicitly confirmed');await wait(500)};
  await loadBulk();
  const bankNames=()=>app.js(`[...document.querySelectorAll('.grid .slot .nm')].map(e=>e.textContent.trim())`);
  assert(JSON.stringify(await bankNames())===JSON.stringify(originalNames.slice(0,64)),'loaded bank preserves slot order');
  assert(await app.js(`(()=>{const dt=new DataTransfer(),s=document.querySelectorAll('.grid .slot');s[0].dispatchEvent(new DragEvent('dragstart',{dataTransfer:dt,bubbles:true}));s[1].dispatchEvent(new DragEvent('dragover',{dataTransfer:dt,bubbles:true,cancelable:true}));s[1].dispatchEvent(new DragEvent('drop',{dataTransfer:dt,bubbles:true,cancelable:true}));return dt.getData('text/xp-slot')==='0'})()`),'bank slot drag/drop uses the actual drag payload');await wait(300);
  const swapped=[originalNames[1],originalNames[0],...originalNames.slice(2,64)];
  assert(JSON.stringify(await bankNames())===JSON.stringify(swapped),'bank drag/drop swaps exactly the two slots');
  assert(await app.click('Save','[aria-label="The bank in the library"]'),'changed bank can be saved to its bulk');
  assert(await app.click('Save over','[aria-label="The bank in the library"]'),'save-over confirmation accepted');await wait(600);
  assert(await bulkAction(copied,'Versions'),'earlier bulk versions opens');await wait(500);
  assert(await bulkAction(copied,'Restore'),'earlier bulk contents can be restored');await wait(500);
  await loadBulk();assert(JSON.stringify(await bankNames())===JSON.stringify(originalNames.slice(0,64)),'restored version reloads the original slot order');
  assert(await app.js(`(()=>{const head=[...document.querySelectorAll('.tree .bulk .head')].find(e=>e.getAttribute('aria-label')===${JSON.stringify('Bulk '+copied)}),slot=document.querySelector('.grid .slot'),dt=new DataTransfer();if(!head||!slot)return false;head.dispatchEvent(new DragEvent('dragstart',{dataTransfer:dt,bubbles:true}));slot.dispatchEvent(new DragEvent('dragover',{dataTransfer:dt,bubbles:true,cancelable:true}));slot.dispatchEvent(new DragEvent('drop',{dataTransfer:dt,bubbles:true,cancelable:true}));return dt.types.includes('application/x-xp-drag')})()`),'whole-bulk drag creates the app drag payload');await wait(200);
  assert(await bulkAction(copied,'Replace',true),'dragged bulk asks before replacing the bank');await wait(400);
  assert(JSON.stringify(await bankNames())===JSON.stringify(originalNames.slice(0,64)),'whole-bulk drop retains all shown slots');
  assert(await bulkAction(copied,'Delete'),'copied test bulk delete asks first');
  assert(await bulkAction(copied,'Delete',true),'copied test bulk delete confirmed');await wait(600);
  assert(!(await bulkNames()).includes(copied)&&(await bulkNames()).includes(renamed),'only the copied test bulk disappears');
  assert(app.pageErrors.length===0,'no script errors in library workflows');
}finally{
  app?.stop();
  // Restore only our own setting; never overwrite an independent concurrent user change.
  const now=existsSync(pointer)?readFileSync(pointer):null;
  if(now&&!now.equals(originalPointer??Buffer.alloc(0))){
    const configured=JSON.parse(now.toString()).dir;
    if(resolve(configured)!==resolve(sandbox))throw new Error(`Storage changed concurrently; original pointer backup retained in ${sandbox}`);
    if(originalPointer)writeFileSync(pointer,originalPointer);else unlinkSync(pointer);
  }
  if(originalFolder&&originalHash)assert(logicalHash(originalFolder)===originalHash,'original user library logical data unchanged');
  console.log(`Copied test library retained at ${sandbox}`);
}
console.log('NATIVE LIBRARY WORKFLOWS PASSED');
