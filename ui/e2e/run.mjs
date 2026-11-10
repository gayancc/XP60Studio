// End-to-end check of the built desktop app. It starts target/release/xppatch-app.exe with a throw-away profile, drives the real window over the
// WebView2 debugging port, and checks what a person would do: open a bank, step through it, edit and undo, export and open the export again, open
// Performances and Rhythm Sets, import into the Library, open the save panels, and look for clipped or overflowing layout in every tab at two window sizes.
//
//   npm run installer-free build first:  npm run build && cargo build -p xppatch-app --release --features custom-protocol
//   then:                                 npm run e2e
//
// It needs no instrument and writes nothing outside a temporary folder. Exit code 0 means every check passed.

import { spawn, execFileSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, '..', '..');
const exe = join(root, 'target', 'release', 'xppatch-app.exe');
const bank = join(root, 'tests', 'golden', 'xp60-user-bank.syx');
const PORT = 9333;
const failures = [];
const check = (ok, what, detail = '') => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${what}${ok || !detail ? '' : '  -> ' + detail}`);
  if (!ok) failures.push(what);
};
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

const profile = mkdtempSync(join(tmpdir(), 'xppatch-e2e-'));
// Exports normally ask where to save with the system Save dialog; this folder makes the app write them straight there instead (for these checks only).
const saves = join(profile, 'saves');
mkdirSync(saves);
const app = spawn(exe, [], {
  env: { ...process.env, WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS: `--remote-debugging-port=${PORT}`, WEBVIEW2_USER_DATA_FOLDER: profile, XPPATCH_SAVE_DIR: saves },
  stdio: 'ignore',
});
const stop = () => {
  try {
    execFileSync('taskkill', ['/pid', String(app.pid), '/t', '/f'], { stdio: 'ignore' });
  } catch {
    // already gone
  }
  try {
    rmSync(profile, { recursive: true, force: true });
  } catch {
    // the profile is in a temp folder; a leftover is harmless
  }
};

async function connect() {
  for (let i = 0; i < 40; i++) {
    try {
      const list = await (await fetch(`http://127.0.0.1:${PORT}/json`)).json();
      const page = list.find((p) => p.type === 'page');
      if (page) return page.webSocketDebuggerUrl;
    } catch {
      // not up yet
    }
    await wait(500);
  }
  throw new Error('the app window did not open');
}

const ws = new WebSocket(await connect());
await new Promise((r) => (ws.onopen = r));
let id = 0;
const pending = new Map();
const pageErrors = [];
const contexts = new Map();
let mainFrame;
let mainContext;
ws.onmessage = (e) => {
  const m = JSON.parse(e.data);
  if (m.id && pending.has(m.id)) pending.get(m.id)(m);
  else if (m.method === 'Runtime.executionContextCreated') {
    const c = m.params.context;
    contexts.set(c.id, c);
    if (c.auxData?.frameId === mainFrame && c.auxData?.isDefault) mainContext = c.id;
  }
  else if (m.method === 'Runtime.executionContextDestroyed') contexts.delete(m.params.executionContextId);
  else if (m.method === 'Runtime.executionContextsCleared') { contexts.clear(); mainContext = undefined; }
  else if (m.method === 'Runtime.exceptionThrown') pageErrors.push(m.params.exceptionDetails.exception?.description ?? m.params.exceptionDetails.text);
  else if (m.method === 'Runtime.consoleAPICalled' && m.params.type === 'error') pageErrors.push(m.params.args.map((a) => a.value ?? a.description).join(' '));
};
const send = (method, params = {}) => new Promise((res, reject) => {
  const i = ++id;
  const timeout = setTimeout(() => { pending.delete(i); reject(new Error(`Debugger timed out: ${method}`)); }, 30000);
  pending.set(i, (response) => {
    clearTimeout(timeout); pending.delete(i);
    if (response.error) reject(new Error(JSON.stringify(response.error))); else res(response);
  });
  ws.send(JSON.stringify({ id: i, method, params }));
});
const js = async (expr) => {
  const response = await send('Runtime.evaluate', { expression: expr, contextId: mainContext, awaitPromise: true, returnByValue: true });
  if (response.result?.exceptionDetails) throw new Error(response.result.exceptionDetails.exception?.description ?? response.result.exceptionDetails.text);
  return response.result?.result?.value;
};
// A keyboard-focused control can keep its hint over the status line. Move away as a user would,
// then let earlier queued messages pass so the result of the action under test becomes visible.
const waitStatus = async (re) => {
  await js('document.activeElement?.blur()');
  await send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: 2, y: 2 });
  for (let i = 0; i < 30; i++) {
    const shown = await js(`document.querySelector('footer .status-msg .msg-text')?.textContent?.trim() ?? ''`);
    if (re.test(shown)) return shown;
    if (shown) await js(`document.querySelector('footer .status-msg .x')?.click()`);
    await wait(100);
  }
  return '';
};
const tab = (name, scope = '') => js(`(()=>{const t=[...document.querySelectorAll('${scope}[role=tab]')].find(x=>x.textContent.trim()===${JSON.stringify(name)}); if(t) t.click(); return !!t})()`);
const setFile = async (label, file) => {
  for (let attempt = 0; attempt < 3; attempt++) {
    try {
      const doc = (await send('DOM.getDocument', { depth: -1 })).result.root.nodeId;
      const node = (await send('DOM.querySelector', { nodeId: doc, selector: `input[type=file][aria-label^="${label}"]` })).result.nodeId;
      if (!node) throw new Error(`File input ${label} is absent`);
      await send('DOM.setFileInputFiles', { nodeId: node, files: [file] });
      return;
    } catch (e) {
      if (attempt === 2) throw e;
      await wait(150);
    }
  }
};

// Resize the real application window with the operating system (SetWindowPos on its top-level window), so the page is laid out exactly as a user sees it.
// The window is also moved to the right-most screen so a test run never covers the main one. The DevTools call Browser.setWindowBounds and Emulation.setDeviceMetricsOverride must NOT be used for this: the first only shrinks the web view inside an
// unchanged window and the second fakes the viewport, and both leave white, offset content in the visible window.
const WIN32 = `Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System; using System.Runtime.InteropServices;
public class Q {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct R { public int L, T, Rr, B; }
}
'@
[Q]::SetProcessDPIAware() | Out-Null
$p = Get-Process -Id ${'$'}{PID} -ErrorAction Stop
$h = $p.MainWindowHandle
`;
async function resizeWindow(width, height) {
  const dpr = (await js('devicePixelRatio')) || 1;
  const run = (w, h) => execFileSync('pwsh', ['-NoProfile', '-Command', WIN32.replace('${PID}', String(app.pid)) + `
[Q]::ShowWindow($h, 9) | Out-Null
$r = New-Object Q+R; $c = New-Object Q+R
[Q]::GetWindowRect($h, [ref]$r) | Out-Null; [Q]::GetClientRect($h, [ref]$c) | Out-Null
$fw = ($r.Rr - $r.L) - $c.Rr; $fh = ($r.B - $r.T) - $c.B
# The test window opens on the right-most screen, never on the main one the owner works on.
$s = [System.Windows.Forms.Screen]::AllScreens | Sort-Object { $_.Bounds.X } | Select-Object -Last 1
[Q]::SetWindowPos($h, [IntPtr]::Zero, $s.WorkingArea.X + 40, $s.WorkingArea.Y + 40, ${w} + $fw, ${h} + $fh, 0x0014) | Out-Null`], { stdio: 'ignore' });
  let w = Math.round(width * dpr);
  let h = Math.round(height * dpr);
  for (let i = 0; i < 4; i++) {
    run(w, h);
    await wait(400);
    const inner = await js('[innerWidth, innerHeight]');
    if (inner[0] === width && inner[1] === height) return;
    w += Math.round((width - inner[0]) * dpr);
    h += Math.round((height - inner[1]) * dpr);
  }
  console.log(`note: the window settled at ${await js('innerWidth + "x" + innerHeight')}, not ${width}x${height}`);
}
// The real window's client area in physical pixels, to prove the page fills the window a user sees (no white, no offset).
const clientSize = () =>
  execFileSync('pwsh', ['-NoProfile', '-Command', WIN32.replace('${PID}', String(app.pid)) + `
$c = New-Object Q+R; [Q]::GetClientRect($h, [ref]$c) | Out-Null; "$($c.Rr)x$($c.B)"`]).toString().trim();
const alerts = () => js(`[...document.querySelectorAll('[role=alert]')].map(e=>e.textContent.trim()).filter(Boolean)`);
const LIST = 'ol[aria-label^="Patches in"] button';
const layoutProbe = `(()=>{const out=[];const doc=document.scrollingElement; if(doc.scrollWidth>innerWidth+1||doc.scrollHeight>innerHeight+1) out.push('page scrolls');
  for(const el of document.querySelectorAll('body *')){const cs=getComputedStyle(el); if(cs.display==='none'||cs.visibility==='hidden') continue; const tag=el.tagName.toLowerCase(); if(['svg','path','line','rect','circle','g','polygon','text','defs','canvas'].includes(tag)) continue;
    const clipsY=el.scrollHeight>el.clientHeight+1&&['hidden','clip'].includes(cs.overflowY)&&el.clientHeight>0; if(clipsY) out.push('clipped '+tag+'.'+String(el.className).split(' ')[0]+' '+el.scrollHeight+'>'+el.clientHeight);}
  return out})()`;

try {
  await send('Runtime.enable');
  await send('DOM.enable');
  await send('Page.enable');
  mainFrame = (await send('Page.getFrameTree')).result.frameTree.frame.id;
  await wait(1500);
  mainContext = [...contexts.values()].find(c => c.auxData?.frameId === mainFrame && c.auxData?.isDefault)?.id;
  if (!mainContext) throw new Error('Main desktop execution context missing');

  // 1. Open a bank as a patch file, list and sweep every slot
  await resizeWindow(1280, 800);
  await tab('Patch', 'header ');
  await setFile('Open a .syx', bank);
  await wait(2000);
  await js(`[...document.querySelectorAll('.left [role=tab]')].find(t=>t.textContent.trim()==='Bank')?.click()`);
  await wait(300);
  const count = await js(`document.querySelectorAll('${LIST}').length`);
  check(count === 128, 'the bank lists 128 patches', String(count));
  const bad = [];
  for (let i = 0; i < count; i++) {
    await js(`document.querySelectorAll('${LIST}')[${i}].click()`);
    await wait(40);
    const origin = await js(`document.querySelector('.origin')?.textContent`);
    const a = await alerts();
    if (a.length || !origin?.includes(`slot ${i + 1}`)) bad.push(i + 1);
  }
  check(bad.length === 0, 'every patch opens without a warning', bad.slice(0, 5).join(','));

  // 2. Previous and next
  await js(`document.querySelectorAll('${LIST}')[10].click()`);
  await wait(200);
  await js(`document.querySelector('header button[aria-label="Next patch"]').click()`);
  await wait(250);
  const next = await js(`document.querySelector('.origin')?.textContent`);
  check(next?.includes('slot 12'), 'next patch moves one slot', next);

  // 3. Edit and undo
  const level = () => js(`document.querySelector('[aria-label="Patch Level"]')?.getAttribute('aria-valuetext')`);
  await js(`[...document.querySelectorAll('.stage [role=tab]')].find(t=>t.textContent.trim()==='Patch')?.click()`);
  await wait(300);
  const before = await level();
  await js(`(()=>{const k=document.querySelector('[aria-label="Patch Level"]'); k.focus(); k.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowDown',bubbles:true}))})()`);
  await wait(250);
  const edited = await level();
  await js(`[...document.querySelectorAll('header button')].find(b=>b.textContent.trim()==='Undo')?.click()`);
  await wait(250);
  check(edited !== before && (await level()) === before, 'an edit changes the value and undo puts it back', `${before} -> ${edited} -> ${await level()}`);

  // 4. Export, open the export, export again: the same bytes
  // A download started by the page would raise the browser's "download multiple files" permission prompt; exports must not use one.
  await js(`window.__downloads=0; const oc=HTMLAnchorElement.prototype.click; HTMLAnchorElement.prototype.click=function(){ if(this.download) window.__downloads++; return oc.call(this) }`);
  const exportOpen = async (menuText) => {
    for (const f of readdirSync(saves)) rmSync(join(saves, f));
    await js(`[...document.querySelectorAll('header button')].find(b=>b.textContent.trim()==='File')?.click()`);
    await wait(250);
    await js(`[...document.querySelectorAll('button,[role=menuitem]')].find(b=>b.textContent.trim().startsWith(${JSON.stringify(menuText)}))?.click()`);
    await wait(1200);
    const files = readdirSync(saves);
    return files.length === 1 ? readFileSync(join(saves, files[0])).toString('base64') : null;
  };
  await js(`document.querySelectorAll('${LIST}')[77].click()`);
  await wait(300);
  const first = await exportOpen('Export patch as .syx');
  check(!!first && Buffer.from(first, 'base64').length > 500, 'a patch exports as a .syx file');
  const tmpFile = join(profile, 'export.syx');
  writeFileSync(tmpFile, Buffer.from(first ?? '', 'base64'));
  await setFile('Open a .syx', tmpFile);
  await wait(1500);
  const second = await exportOpen('Export patch as .syx');
  check(first === second && !!first, 'the export opens again and exports the same bytes');
  const notice = await waitStatus(/Saved /);
  check(/Saved .+\.syx/.test(notice) && !/[\/]/.test(notice), 'after saving, the message shows the file name only, never a folder path', notice);
  const inHintBar = await js(`(()=>{const m=document.querySelector('footer[aria-label="Hint bar"] .status-msg'); if(!m) return false; const bar=document.querySelector('footer[aria-label="Hint bar"]'); const r=bar.getBoundingClientRect(); const fixed=getComputedStyle(m).position==='fixed'; return !fixed && r.bottom>innerHeight-8 && r.top>innerHeight-80})()`);
  check(inHintBar === true, 'the message is in the bottom hint bar, not a floating overlay');
  check(((await js(`document.querySelector('header .note, header .err, header .learn, header [role=alert]')?.textContent`)) ?? '') === '', 'the top bar shows no message');
  check((await js('window.__downloads')) === 0, 'exports are saved by the app, not by a browser download');

  // 5. Performances and Rhythm Sets from the same whole-memory file
  for (const [mode, expected] of [['Performance', 32], ['Rhythm set', 2]]) {
    await tab(mode, 'header ');
    await wait(500);
    await setFile('Open a .syx', bank);
    await wait(1800);
    // The picker is a drop-down list: open it, count its entries, close it again.
    await js(`document.querySelector('header .slot [role=combobox]')?.click()`);
    await wait(250);
    const n = await js(`document.querySelectorAll('[role=listbox] [role=option]').length`);
    await js(`document.querySelector('[role=listbox]')?.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))`);
    await wait(150);
    check(n === expected, `${mode}: the file's ${expected} entries are listed`, String(n));
    const tabs = await js(`[...document.querySelectorAll('.stage [role=tab]')].map(x=>x.textContent.trim())`);
    const problems = [];
    for (const t of tabs) {
      await tab(t, '.stage ');
      await wait(200);
      problems.push(...((await js(layoutProbe)) ?? []).map((p) => `${t}: ${p}`));
    }
    check(problems.length === 0, `${mode}: every tab lays out cleanly`, problems.slice(0, 3).join(' | '));
  }

  // 5b. Menus hang fully visible under the top bar (a clipping parent once cut them to one row)
  for (const title of ['File', 'Edit', 'View']) {
    await js(`[...document.querySelectorAll('nav button.title')].find(b=>b.textContent.trim()===${JSON.stringify(title)})?.click()`);
    await wait(300);
    const whole = await js(`(()=>{const m=document.querySelector('[role=menu]'); if(!m) return false; const items=[...m.querySelectorAll('button')]; const last=items[items.length-1]; const r=last.getBoundingClientRect(); const e=document.elementFromPoint(r.left+r.width/2, r.top+r.height/2); return !!e?.closest('[role=menu]')})()`);
    check(whole, `the ${title} menu shows all its items`);
    await js(`document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true})); window.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))`);
    await wait(150);
  }

  // 5b2. No Patch tab needs a scroll bar in the smallest window (the tabs are merged by what belongs together so that each fits)
  await resizeWindow(1100, 700);
  await tab('Patch', 'header ');
  await wait(400);
  const centreTabs = (await js(`[...document.querySelectorAll('.center [role=tab]')].map(x=>x.textContent.trim())`)) ?? [];
  const scrolling = [];
  for (const t of centreTabs) {
    await tab(t, '.center ');
    await wait(250);
    const over = await js(`(()=>{const p=document.querySelector('.center [role=tabpanel]'); return p.scrollHeight-p.clientHeight})()`);
    if (over > 1) {
      // Name the lowest elements so a failure says what to shrink.
      const who = await js(`(()=>{const p=document.querySelector('.center [role=tabpanel]'); const pb=p.getBoundingClientRect().bottom; return [...p.querySelectorAll('*')].map(e=>({e,b:e.getBoundingClientRect().bottom+(parseFloat(getComputedStyle(e).marginBottom)||0)})).filter(x=>x.b>pb-0.5).sort((a,b)=>b.b-a.b).slice(0,3).map(x=>x.e.tagName.toLowerCase()+'.'+String(x.e.className?.baseVal ?? x.e.className).split(' ')[0]+' +'+Math.round(x.b-pb)).join(' | ')})()`);
      scrolling.push(`${t} (+${over}px: ${who})`);
    }
  }
  check(centreTabs.length >= 6 && centreTabs.length <= 8 && scrolling.length === 0, `Patch: ${centreTabs.length} tabs (${centreTabs.join(', ')}), none scrolls at 1100x700`, scrolling.join(', '));
  await resizeWindow(1280, 800);

  // 5c. Keyboard focus on a knob, slider or switch must not draw a box, ring, border or tint around it
  const pressTab = async () => {
    for (const type of ['keyDown', 'keyUp']) await send('Input.dispatchKeyEvent', { type, key: 'Tab', code: 'Tab', windowsVirtualKeyCode: 9 });
    await wait(40);
  };
  const focusAudit = async (where) => {
    await js(`document.activeElement?.blur()`);
    let checked = 0;
    const bad = [];
    for (let i = 0; i < 70; i++) {
      await pressTab();
      const r = await js(`(()=>{const e=document.activeElement; if(!e) return null; const role=e.getAttribute('role'); if(!['slider','switch','spinbutton'].includes(role)) return null; const sig=(x)=>{const cs=getComputedStyle(x); return {shadow:cs.boxShadow, outline:cs.outlineStyle==='none'||parseFloat(cs.outlineWidth)===0?'none':cs.outlineStyle+' '+cs.outlineColor, image:cs.backgroundImage, fill:cs.backgroundColor, border:cs.borderTopStyle==='none'||parseFloat(cs.borderTopWidth)===0?'none':cs.borderTopWidth+' '+cs.borderTopColor}}; const on=sig(e); const svgOn=e.querySelector('svg')?getComputedStyle(e.querySelector('svg')).filter:''; e.blur(); const off=sig(e); e.focus({focusVisible:true}); const parts=Object.keys(on).filter(k=>on[k]!==off[k]).map(k=>k+': '+off[k]+' -> '+on[k]); return {label:e.getAttribute('aria-label'), problems:parts}})()`);
      if (!r) continue;
      checked++;
      if (r.problems.length) bad.push(`${r.label}: ${r.problems.join(', ')}`);
    }
    check(checked >= 3 && bad.length === 0, `${where}: focusing ${checked} knobs, sliders and switches draws no ring, box or border`, bad.slice(0, 3).join(' | '));
  };
  await tab('Patch', 'header ');
  check(await tab('Filter and amp', '.stage '), 'the Filter and amp tab opens');
  await wait(300);
  await focusAudit('Patch > Filter and amp');
  for (const t of ['Zones', 'Wave and pitch', 'LFO and modulation', 'Routing']) {
    check(await tab(t, '.stage '), `the ${t} tab opens`);
    await wait(250);
    await focusAudit(`Patch > ${t}`);
  }
  await tab('Performance', 'header ');
  await wait(400);
  await focusAudit('Performance');
  await tab('Rhythm set', 'header ');
  await wait(400);
  await focusAudit('Rhythm set');
  await tab('System', 'header ');
  await js(`[...document.querySelectorAll('.stage [role=tab]')].find(t=>t.textContent.trim()==='Scale tune')?.click()`);
  await wait(300);
  await focusAudit('System > Scale tune');

  // 6. Save panels are visible (not clipped by the top bar) and write nothing without an instrument
  for (const [mode, buttonText, panel] of [['Patch', 'Save to synth…', '#save-panel'], ['System', 'Write to synth…', '#system-save-panel']]) {
    await tab(mode, 'header ');
    await wait(500);
    await js(`[...document.querySelectorAll('header button')].find(b=>b.textContent.trim()===${JSON.stringify(buttonText)})?.click()`);
    await wait(500);
    const visible = await js(`(()=>{const p=document.querySelector('${panel}'); if(!p) return false; const r=p.getBoundingClientRect(); return [[r.left+20,r.top+20],[r.right-20,r.bottom-20]].every(([x,y])=>!!document.elementFromPoint(x,y)?.closest('${panel}'))})()`);
    check(visible, `${mode}: the save panel is fully visible`);
    await js(`[...document.querySelectorAll('${panel} button')].find(b=>/^1\\./.test(b.textContent.trim()))?.click()`);
    await wait(500);
    const msg = await waitStatus(/not connected/i);
    check(/not connected/i.test(msg ?? ''), `${mode}: arming without an instrument is refused in the hint bar`, msg);
    check(((await js(`document.querySelector('${panel} .err, ${panel} .error')?.textContent`)) ?? '') === '', `${mode}: the panel itself shows no error line`);
    await js(`[...document.querySelectorAll('${panel} button')].find(b=>b.textContent.trim()==='Close')?.click()`);
    await wait(200);
  }

  // 7. Library import and the Bank workspace
  await tab('Library', 'header ');
  await wait(600);
  await setFile('Import a .syx', bank);
  await wait(2000);
  const staged = await js(`[...document.querySelectorAll('button')].map(b=>b.textContent.trim()).find(t=>/^Import \\d+/.test(t))`);
  check(staged === 'Import 128', 'the Library previews 128 patches for import', staged);
  await tab('Bank', 'header ');
  await wait(600);
  check(await tab('Library', '.right '), 'the Bank library panel opens');
  await wait(250);
  await setFile('Import a .syx or .mid bulk file', bank);
  await wait(400);
  const importBulk = await js(`(()=>{const b=[...document.querySelectorAll('.right button')].find(b=>/^Import 128 as bulk$/.test(b.textContent.trim())); b?.click(); return !!b})()`);
  check(importBulk, 'the Bank library imports the 128-patch bulk');
  await wait(700);
  // Importing already opens the new bulk; only click its name when it is still closed (a click on an open bulk closes it).
  const openedBulk = await js(`(()=>{const b=document.querySelector('.right li.bulk .head button.name'); if (b && !document.querySelector('.right li.bulk .tools')) b.click(); return !!b})()`);
  check(openedBulk, 'the imported bulk opens in the Bank library');
  await wait(250);
  const loadBulk = await js(`(()=>{const b=[...document.querySelectorAll('.right li.bulk .tools button')].find(b=>b.textContent.trim()==='Load into bank'); b?.click(); return !!b})()`);
  check(loadBulk, 'the bulk offers Load into bank');
  await wait(150);
  const replaced = await js(`(()=>{const b=document.querySelector('.right li.bulk .ask button.go'); b?.click(); return !!b})()`);
  check(replaced, 'the bulk replaces the local bank after confirmation');
  await wait(900);
  const slots = await js(`document.querySelectorAll('.grid[aria-label="User patches a"] button.slot .nm').length`);
  check(slots === 64, 'the Bank workspace shows 64 loaded slots in subgroup a', String(slots));

  // 8. Layout: every tab of every workspace at two window sizes
  for (const [w, h] of [[1100, 700], [1280, 800]]) {
    await resizeWindow(w, h);
    await wait(500);
    const dprNow = await js('devicePixelRatio');
    const client = clientSize();
    check(client === `${Math.round(w * dprNow)}x${Math.round(h * dprNow)}`, `the real window is ${w}x${h} and the page fills it`, client);
    const origin = await js('(() => { const r = document.body.getBoundingClientRect(); return r.left + "," + r.top })()');
    check(origin === '0,0', `the page starts at the window's top-left at ${w}x${h}`, origin);
    const problems = [];
    for (const mode of ['Patch', 'Performance', 'Rhythm set', 'System', 'Library', 'Bank']) {
      await tab(mode, 'header ');
      await wait(400);
      const inner = (await js(`[...document.querySelectorAll('.stage [role=tab]')].map(x=>x.textContent.trim())`)) ?? [];
      for (const t of ['', ...inner]) {
        if (t) {
          await tab(t, '.stage ');
          await wait(200);
        }
        problems.push(...((await js(layoutProbe)) ?? []).map((p) => `${mode}/${t || 'main'}: ${p}`));
      }
    }
    check(problems.length === 0, `layout is clean at ${w}x${h}`, problems.slice(0, 3).join(' | '));
  }

  check(pageErrors.length === 0, 'no script errors in the window', [...new Set(pageErrors)].slice(0, 2).join(' | '));
} catch (e) {
  check(false, 'the run finished', String(e?.stack ?? e));
} finally {
  ws.close();
  stop();
}

console.log(failures.length ? `\n${failures.length} check(s) failed` : '\nAll checks passed');
process.exit(failures.length ? 1 : 0);
