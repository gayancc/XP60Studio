// Shared helpers for the hardware checks: start the built desktop app against the REAL instrument, drive its window over the WebView2 debugging
// port, and read what a person would see (messages, status text, buttons).
//
// Unlike run.mjs this talks to the connected XP-60, so only functions that were agreed with the owner are run, each one backed up first.
// The app's own data folder (library, backups) is the real one: nothing here imports into the library.

import { spawn, execFileSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
export const root = resolve(here, '..', '..');
export const wait = (ms) => new Promise((r) => setTimeout(r, ms));

export async function startApp(port = 9334) {
  const exe = join(root, 'target', 'release', 'xppatch-app.exe');
  const profile = mkdtempSync(join(tmpdir(), 'xppatch-hw-'));
  const saves = process.env.XPPATCH_HW_SAVES ? resolve(process.env.XPPATCH_HW_SAVES) : join(profile, 'saves');
  mkdirSync(saves, { recursive: true });
  const app = spawn(exe, [], {
    env: { ...process.env, WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS: `--remote-debugging-port=${port}`, WEBVIEW2_USER_DATA_FOLDER: profile, XPPATCH_SAVE_DIR: saves },
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
      // a leftover temp folder is harmless
    }
  };
  let wsUrl;
  for (let i = 0; i < 60 && !wsUrl; i++) {
    try {
      const list = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
      wsUrl = list.find((p) => p.type === 'page')?.webSocketDebuggerUrl;
    } catch {
      // not up yet
    }
    if (!wsUrl) await wait(500);
  }
  if (!wsUrl) {
    stop();
    throw new Error('the app window did not open');
  }
  const ws = new WebSocket(wsUrl);
  await new Promise((r) => (ws.onopen = r));
  let id = 0;
  const pending = new Map();
  const pageErrors = [];
  const midiTrace = [];
  const contexts = new Map();
  let mainContext;
  let mainFrame;
  let traceError = '';
  ws.onmessage = async (e) => {
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
    else if (m.method === 'Debugger.paused') {
      try {
        const call = await send('Debugger.evaluateOnCallFrame', { callFrameId: m.params.callFrames[0].callFrameId, expression: '[arguments[0], arguments[1]]', returnByValue: true });
        const args = call.result?.result?.value;
        if (!Array.isArray(args)) throw new Error('Cannot inspect invoked command');
        if (typeof args[0] === 'string' && args[0].startsWith('midi_')) midiTrace.push({ command: args[0], args: args[1] });
      } catch (error) { traceError = String(error); }
      finally { await send('Debugger.resume'); }
    }
  };
  const send = (method, params = {}) => new Promise((res, reject) => {
    const i = ++id;
    const timeout = setTimeout(() => {
      pending.delete(i);
      reject(new Error(`Desktop debugger timed out: ${method}`));
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
    if (response.result?.exceptionDetails) {
      throw new Error(response.result.exceptionDetails.exception?.description ?? response.result.exceptionDetails.text);
    }
    return response.result?.result?.value;
  };
  await send('Runtime.enable');
  await send('DOM.enable');
  await send('Page.enable');
  mainFrame = (await send('Page.getFrameTree')).result.frameTree.frame.id;
  await wait(1500);
  mainContext = [...contexts.values()].find(c => c.auxData?.frameId === mainFrame && c.auxData?.isDefault)?.id;
  if (!mainContext) { stop(); throw new Error('Main desktop execution context missing'); }

  const api = {
    app, saves, profile, send, js, wait, stop, pageErrors,
    startMidiTrace: async () => {
      await send('Debugger.enable');
      const fn = await send('Runtime.evaluate', { expression: 'window.__TAURI_INTERNALS__.invoke', contextId: mainContext });
      const objectId = fn.result?.result?.objectId;
      if (!objectId) throw new Error('Tauri invoke function missing');
      await send('Debugger.setBreakpointOnFunctionCall', { objectId });
    },
    midiTrace: async () => { if (traceError) throw new Error(traceError); return midiTrace; },
    /** Text of the current hint-bar status message, plus any still waiting in the queue (+N). */
    messages: () => js(`(()=>{const t=document.querySelector('footer .status-msg .msg-text')?.textContent?.trim(); return t ? [t] : []})()`),
    /** Click the first button (or menu item) whose text starts with `text`; false when none. */
    click: (text, scope = 'body') => js(`(()=>{const b=[...document.querySelector(${JSON.stringify(scope)}).querySelectorAll('button,[role=menuitem],[role=tab],[role=radio]')].find(x=>x.textContent.trim().startsWith(${JSON.stringify(text)})&&!x.disabled); if(b){b.click(); return true} return false})()`),
    /** Wait up to `ms` for a message matching `re`; resolves to the matching text or null. */
    waitMessage: async (re, ms = 20000) => {
      for (let t = 0; t < ms; t += 250) {
        const m = (await api.messages()) ?? [];
        const hit = m.find((x) => re.test(x));
        if (hit) return hit;
        // Skip an unrelated current line so a later queued match can surface.
        if (m[0] && !re.test(m[0])) await js(`document.querySelector('footer .status-msg .x')?.click()`);
        await wait(250);
      }
      return null;
    },
    /** Pick the entry named `text` of the drop-down labelled `name` (the kit's drop-downs are buttons that open a list). */
    pick: async (name, text) => {
      const opened = await js(`(()=>{const c=document.querySelector('[role=combobox][aria-label=${JSON.stringify(name)}]'); if(!c) return false; c.click(); return true})()`);
      if (!opened) return false;
      await wait(250);
      const ok = await js(`(()=>{const o=[...document.querySelectorAll('[role=listbox] [role=option]')].find(x=>x.textContent.includes(${JSON.stringify(text)})); if(o){o.click(); return true} return false})()`);
      await wait(250);
      return ok;
    },
    /** Make sure the Synth menu (the connection controls) is open. */
    openSynth: async () => {
      if (await js(`!!document.querySelector('.synth')`)) return true;
      await js(`[...document.querySelectorAll('nav button.title, header button')].find(x=>x.textContent.trim()==='Synth')?.click()`);
      await wait(400);
      return js(`!!document.querySelector('.synth')`);
    },
    text: (selector) => js(`document.querySelector(${JSON.stringify(selector)})?.textContent?.trim() ?? null`),
  };
  return api;
}
