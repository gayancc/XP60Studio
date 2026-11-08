// H-076: keep the native app open while the owner plays a secondary USB controller.
// stdin commands: status, ch2, direct, read, stop. No permanent writes.
import { createInterface } from 'node:readline';
import { startApp, wait } from './hw-lib.mjs';

const assert = (ok, label) => {
  if (!ok) throw new Error(label);
  console.log(`PASS ${label}`);
};
const app = await startApp(9336);
const status = () => app.js(`window.__TAURI_INTERNALS__.invoke('midi_thru_status')`);
try {
  assert(await app.click('Patch', 'header'), 'Patch workspace');
  await app.openSynth();
  assert(await app.pick('MIDI input', 'FCB1010 IN'), 'XP-60 input selected');
  assert(await app.pick('MIDI output', 'FCB1010 OUT'), 'XP-60 output selected');
  assert(await app.click('Connect', '.synth'), 'Connect clicked');
  for (let i = 0; i < 20; i++) {
    await wait(500);
    await app.openSynth();
    if (/XP-60 · ID 17/.test((await app.text('.synth .ok')) ?? '')) break;
  }
  assert(/XP-60 · ID 17/.test((await app.text('.synth .ok')) ?? ''), 'XP-60 connected');
  assert(await app.pick('Controller MIDI input', 'KL Essential 49 mk3 MIDI'), 'KL controller input selected');
  assert(await app.pick('Pass-through channel', 'Direct'), 'Direct channel selected');
  assert(await app.click('Start thru', '.synth'), 'Start thru clicked');
  await wait(800);
  let s = await status();
  assert(s.active && s.enabled && !s.held && s.input?.includes('KL Essential 49 mk3 MIDI') && s.target_channel === null, `Direct pass-through active: ${JSON.stringify(s)}`);
  console.log('H076 READY: play KL keyboard now; stdin commands are status, ch2, direct, read, stop.');
  for await (const line of createInterface({ input: process.stdin })) {
    const cmd = line.trim().toLowerCase();
    if (cmd === 'status') {
      console.log('STATUS', JSON.stringify(await status()));
    } else if (cmd === 'ch2') {
      assert(await app.pick('Pass-through channel', 'Ch 2'), 'Ch 2 selected in app');
      await wait(350);
      s = await status();
      assert(s.target_channel === 1 && s.active && s.enabled, `channel remap 2 active: ${JSON.stringify(s)}`);
    } else if (cmd === 'direct') {
      assert(await app.pick('Pass-through channel', 'Direct'), 'Direct selected in app');
      await wait(350);
      s = await status();
      assert(s.target_channel === null && s.active, 'Direct channel restored');
    } else if (cmd === 'read') {
      await app.js(`(()=>{window.__h76result=null;window.__TAURI_INTERNALS__.invoke('midi_read_bank').then(x=>window.__h76result={ok:true,count:x.length},e=>window.__h76result={ok:false,error:String(e)});return true})()`);
      await wait(500);
      s = await status();
      assert(s.held, `pass-through held during bank SysEx: ${JSON.stringify(s)}`);
      let result;
      for (let i = 0; i < 90; i++) {
        result = await app.js('window.__h76result');
        if (result) break;
        await wait(1000);
      }
      assert(result?.ok && result.count === 128, `bank read while controller thru active: ${JSON.stringify(result)}`);
      s = await status();
      assert(!s.held && s.active && s.enabled, 'pass-through resumed after bank read');
      console.log('H076 BANK READ PASS');
    } else if (cmd === 'stop' || cmd === 'exit') {
      assert(await app.click('Stop', '.synth'), 'Stop thru clicked');
      await wait(350);
      s = await status();
      assert(!s.active, 'pass-through stopped');
      break;
    } else {
      console.log(`Unknown command ${JSON.stringify(cmd)}`);
    }
  }
  assert(app.pageErrors.length === 0, 'no desktop script errors');
} finally {
  app.stop();
}
// The WebView debugger socket can outlive the stopped native process on Windows.
process.exit(0);
