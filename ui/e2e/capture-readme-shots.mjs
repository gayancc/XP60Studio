// Capture fresh README screenshots from the built desktop app (no instrument).
// Requires: npm run build && cargo build -p xppatch-app --release --features custom-protocol
import { spawn, execFileSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, '..', '..');
const exe = join(root, 'target', 'release', 'xppatch-app.exe');
const bank = join(root, 'tests', 'golden', 'xp60-user-bank.syx');
const outDir = join(root, 'artifact', 'docs', 'screenshots');
const PORT = 9355;
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

mkdirSync(outDir, { recursive: true });
const profile = mkdtempSync(join(tmpdir(), 'xppatch-shots-'));
const saves = join(profile, 'saves');
mkdirSync(saves);
const app = spawn(exe, [], {
  env: {
    ...process.env,
    WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS: `--remote-debugging-port=${PORT}`,
    WEBVIEW2_USER_DATA_FOLDER: profile,
    XPPATCH_SAVE_DIR: saves,
  },
  stdio: 'ignore',
});
const stop = () => {
  try {
    execFileSync('taskkill', ['/pid', String(app.pid), '/t', '/f'], { stdio: 'ignore' });
  } catch {
    /* already gone */
  }
  try {
    rmSync(profile, { recursive: true, force: true });
  } catch {
    /* leftover temp is fine */
  }
};

async function connect() {
  for (let i = 0; i < 60; i++) {
    try {
      const list = await (await fetch(`http://127.0.0.1:${PORT}/json`)).json();
      const page = list.find((p) => p.type === 'page');
      if (page) return page.webSocketDebuggerUrl;
    } catch {
      /* not up yet */
    }
    await wait(500);
  }
  throw new Error('the app window did not open');
}

const ws = new WebSocket(await connect());
await new Promise((r) => (ws.onopen = r));
let id = 0;
const pending = new Map();
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
  } else if (m.method === 'Runtime.executionContextDestroyed') contexts.delete(m.params.executionContextId);
  else if (m.method === 'Runtime.executionContextsCleared') {
    contexts.clear();
    mainContext = undefined;
  }
};
const send = (method, params = {}) =>
  new Promise((res, reject) => {
    const i = ++id;
    const timeout = setTimeout(() => {
      pending.delete(i);
      reject(new Error(`Debugger timed out: ${method}`));
    }, 30000);
    pending.set(i, (response) => {
      clearTimeout(timeout);
      pending.delete(i);
      if (response.error) reject(new Error(JSON.stringify(response.error)));
      else res(response);
    });
    ws.send(JSON.stringify({ id: i, method, params }));
  });
const js = async (expr) => {
  const response = await send('Runtime.evaluate', {
    expression: expr,
    contextId: mainContext,
    awaitPromise: true,
    returnByValue: true,
  });
  if (response.result?.exceptionDetails) {
    throw new Error(response.result.exceptionDetails.exception?.description ?? response.result.exceptionDetails.text);
  }
  return response.result?.result?.value;
};

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
  const run = (w, h) =>
    execFileSync(
      'pwsh',
      [
        '-NoProfile',
        '-Command',
        WIN32.replace('${PID}', String(app.pid)) +
          `
[Q]::ShowWindow($h, 9) | Out-Null
$r = New-Object Q+R; $c = New-Object Q+R
[Q]::GetWindowRect($h, [ref]$r) | Out-Null; [Q]::GetClientRect($h, [ref]$c) | Out-Null
$fw = ($r.Rr - $r.L) - $c.Rr; $fh = ($r.B - $r.T) - $c.B
$s = [System.Windows.Forms.Screen]::AllScreens | Sort-Object { $_.Bounds.X } | Select-Object -Last 1
[Q]::SetWindowPos($h, [IntPtr]::Zero, $s.WorkingArea.X + 40, $s.WorkingArea.Y + 40, ${w} + $fw, ${h} + $fh, 0x0014) | Out-Null`,
      ],
      { stdio: 'ignore' },
    );
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
}

const tab = (name, scope = '') =>
  js(
    `(()=>{const t=[...document.querySelectorAll('${scope}[role=tab]')].find(x=>x.textContent.trim()===${JSON.stringify(name)}); if(t) t.click(); return !!t})()`,
  );
const setFile = async (label, file) => {
  const doc = (await send('DOM.getDocument', { depth: -1 })).result.root.nodeId;
  const node = (await send('DOM.querySelector', { nodeId: doc, selector: `input[type=file][aria-label^="${label}"]` })).result
    .nodeId;
  if (!node) throw new Error(`File input missing: ${label}`);
  await send('DOM.setFileInputFiles', { nodeId: node, files: [file] });
};
const dismissStatus = () =>
  js(`(()=>{for(let i=0;i<24;i++){const b=document.querySelector('footer .status-msg .x'); if(!b) break; b.click()}})()`);
const shot = async (name) => {
  await js('document.activeElement?.blur()');
  await dismissStatus();
  await send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: 4, y: 4 });
  await wait(250);
  const png = (await send('Page.captureScreenshot', { format: 'png', fromSurface: true, captureBeyondViewport: false }))
    .result.data;
  const path = join(outDir, name);
  writeFileSync(path, Buffer.from(png, 'base64'));
  console.log(`wrote ${name} (${Math.round(Buffer.from(png, 'base64').length / 1024)} KB)`);
};

try {
  await send('Runtime.enable');
  await send('DOM.enable');
  await send('Page.enable');
  mainFrame = (await send('Page.getFrameTree')).result.frameTree.frame.id;
  await wait(1500);
  mainContext = [...contexts.values()].find((c) => c.auxData?.frameId === mainFrame && c.auxData?.isDefault)?.id;
  if (!mainContext) throw new Error('Main desktop execution context missing');

  await resizeWindow(1400, 900);
  await wait(500);

  // --- Patch: Zones ---
  await tab('Patch', 'header ');
  await setFile('Open a .syx', bank);
  await wait(2000);
  await js(`[...document.querySelectorAll('.left [role=tab]')].find(t=>t.textContent.trim()==='Bank')?.click()`);
  await wait(300);
  await js(`document.querySelectorAll('ol[aria-label^="Patches in"] button')[17]?.click()`);
  await wait(500);
  await tab('Zones', '.stage ');
  await wait(400);
  await shot('patch-zones.png');

  // --- Patch: Filter and amp ---
  await tab('Filter and amp', '.stage ');
  await wait(500);
  await shot('patch-amp.png');

  // --- Light theme Zones ---
  await js(`document.documentElement.dataset.theme='light'; try{localStorage.setItem('xppatch.theme','light')}catch{}`);
  await wait(350);
  await tab('Zones', '.stage ');
  await wait(300);
  await shot('patch-zones-light.png');
  // back to dark for remaining shots
  await js(`delete document.documentElement.dataset.theme; try{localStorage.setItem('xppatch.theme','dark')}catch{}`);
  await wait(300);

  // --- Performance ---
  await tab('Performance', 'header ');
  await wait(800);
  await shot('performance.png');

  // --- Library ---
  await tab('Library', 'header ');
  await wait(600);
  await setFile('Import a .syx', bank);
  await wait(1500);
  await shot('library.png');

  // --- Bank builder (loaded bulk) ---
  await tab('Bank', 'header ');
  await wait(600);
  await tab('Library', '.right ');
  await wait(250);
  await setFile('Import a .syx or .mid bulk file', bank);
  await wait(400);
  await js(`([...document.querySelectorAll('.right button')].find(b=>/^Import 128 as bulk$/.test(b.textContent.trim()))||{}).click?.()`);
  await wait(700);
  await js(`document.querySelector('.right li.bulk .head button.name')?.click()`);
  await wait(250);
  await js(`([...document.querySelectorAll('.right li.bulk .tools button')].find(b=>b.textContent.trim()==='Load into bank')||{}).click?.()`);
  await wait(150);
  await js(`document.querySelector('.right li.bulk .ask button.go')?.click()`);
  await wait(900);
  await tab('Slot', '.right ');
  await wait(300);
  await shot('bank.png');

  // --- Normalize browser ---
  await tab('Normalize', '.right ');
  await wait(300);
  await js(`([...document.querySelectorAll('.norm button')].find(b=>/^Preview$/.test(b.textContent.trim()))||{}).click?.()`);
  await wait(1500);
  await shot('bank-normalize.png');

  console.log('README screenshots updated in', outDir);
} catch (e) {
  console.error(e);
  process.exitCode = 1;
} finally {
  stop();
}
