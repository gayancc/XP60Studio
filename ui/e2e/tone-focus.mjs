// Live check: left Tone cards focus the matching multi-tone editor (All 4 Tones + Zones).
import { spawn, execFileSync } from 'node:child_process';
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, '..', '..');
const exe = join(root, 'target', 'release', 'xppatch-app.exe');
const PORT = 9334;
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const profile = mkdtempSync(join(tmpdir(), 'xppatch-tone-focus-'));
const app = spawn(exe, [], {
  env: { ...process.env, WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS: `--remote-debugging-port=${PORT}`, WEBVIEW2_USER_DATA_FOLDER: profile },
  stdio: 'ignore',
});
const stop = () => {
  try {
    execFileSync('taskkill', ['/pid', String(app.pid), '/t', '/f'], { stdio: 'ignore' });
  } catch {
    /* gone */
  }
  try {
    rmSync(profile, { recursive: true, force: true });
  } catch {
    /* leftover ok */
  }
};
process.on('SIGINT', () => {
  stop();
  process.exit(130);
});
process.on('SIGTERM', () => {
  stop();
  process.exit(143);
});

async function connect() {
  for (let i = 0; i < 40; i++) {
    try {
      const list = await (await fetch(`http://127.0.0.1:${PORT}/json`)).json();
      const page = list.find((p) => p.type === 'page');
      if (page) return page.webSocketDebuggerUrl;
    } catch {
      /* wait */
    }
    await wait(500);
  }
  throw new Error('the app window did not open');
}

const ws = new WebSocket(await connect());
await Promise.race([
  new Promise((r) => {
    ws.onopen = r;
  }),
  new Promise((_, rej) => {
    ws.onerror = () => rej(new Error('WebSocket connection failed'));
    ws.onclose = () => rej(new Error('WebSocket closed before open'));
  }),
]);
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
  const response = await send('Runtime.evaluate', { expression: expr, contextId: mainContext, awaitPromise: true, returnByValue: true });
  if (response.result?.exceptionDetails) throw new Error(response.result.exceptionDetails.exception?.description ?? response.result.exceptionDetails.text);
  return response.result?.result?.value;
};
const tab = (name, scope = '') => js(`(()=>{const t=[...document.querySelectorAll('${scope}[role=tab]')].find(x=>x.textContent.trim()===${JSON.stringify(name)}); if(t) t.click(); return !!t})()`);

const failures = [];
const check = (ok, what, detail = '') => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${what}${ok || !detail ? '' : '  -> ' + detail}`);
  if (!ok) failures.push(what);
};

try {
  await send('Runtime.enable');
  await send('Page.enable');
  mainFrame = (await send('Page.getFrameTree')).result.frameTree.frame.id;
  await wait(1500);
  mainContext = [...contexts.values()].find((c) => c.auxData?.frameId === mainFrame && c.auxData?.isDefault)?.id;
  if (!mainContext) throw new Error('Main desktop execution context missing');

  await tab('Patch', 'header ');
  await wait(300);
  await tab('Wave and pitch');
  await wait(200);
  // Switch to All 4 Tones.
  check(await js(`(()=>{const b=[...document.querySelectorAll('.toneview [role=radio], .toneview button, .view button')].find(x=>/All 4 Tones/i.test(x.textContent)); if(!b) return false; b.click(); return true})()`), 'All 4 Tones is available');
  await wait(300);

  for (const i of [0, 2, 1, 3]) {
    check(await js(`(()=>{const cards=[...document.querySelectorAll('.left .cards .card')]; const c=cards[${i}]; if(!c) return false; c.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,cancelable:true})); return true})()`), `clicked left Tone ${i + 1}`);
    await wait(250);
    const sel = await js(`(()=>{const cols=[...document.querySelectorAll('.all .col')]; return cols.map((c,n)=>({n,sel:c.classList.contains('sel'),focus:c.getAttribute('data-tone-focus')}));})()`);
    check(sel?.[i]?.sel === true, `All 4 Tones column ${i + 1} is focused after left click`, JSON.stringify(sel));
  }

  // Editing a right column selects that tone on the left.
  check(await js(`(()=>{const col=document.querySelectorAll('.all .col')[2]; col.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,cancelable:true,composed:true})); return !!col})()`), 'pressed Tone 3 column body');
  await wait(200);
  check(await js(`document.querySelectorAll('.left .cards .card')[2]?.classList.contains('selected')`), 'left Tone 3 selected after right edit press');
  check(await js(`document.querySelectorAll('.all .col')[2]?.classList.contains('sel')`), 'Tone 3 column stays focused');

  // Zones tab: left click focuses the velocity row.
  check(await tab('Zones'), 'opened Zones');
  await wait(250);
  for (const i of [3, 0, 2]) {
    check(await js(`(()=>{const cards=[...document.querySelectorAll('.left .cards .card')]; cards[${i}]?.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,cancelable:true})); return !!cards[${i}]})()`), `Zones: clicked left Tone ${i + 1}`);
    await wait(200);
    check(await js(`document.querySelector(\`.zones .row[data-tone-focus="${i}"]\`)?.classList.contains('sel')`), `Zones velocity row ${i + 1} focused`);
  }

  if (failures.length) {
    console.error(`\n${failures.length} check(s) failed`);
    process.exitCode = 1;
  } else {
    console.log('\nAll tone-focus checks passed');
  }
} catch (e) {
  console.error(e);
  process.exitCode = 1;
} finally {
  try {
    ws.close();
  } catch {
    /* */
  }
  stop();
}
