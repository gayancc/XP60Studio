// Hardware check 1 (reads only): connect to the XP-60 from the real app, fetch the temporary sound, verify it, disconnect and reconnect.
//   npm run build && cargo build -p xppatch-app --release --features custom-protocol   (first)
//   node e2e/hw-connect.mjs
import { startApp, wait } from './hw-lib.mjs';

const results = [];
const check = (ok, what, detail = '') => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${what}${detail ? '  -> ' + detail : ''}`);
  results.push(ok);
};

const app = await startApp();
try {
  await app.click('Patch', 'header');
  check(await app.openSynth(), 'the Synth menu opens with the connection controls');

  check(await app.pick('MIDI input', 'FCB1010 IN'), 'the MIDI input list offers FCB1010 IN');
  check(await app.pick('MIDI output', 'FCB1010 OUT'), 'the MIDI output list offers FCB1010 OUT');
  check(await app.click('Connect', '.synth'), 'Connect can be pressed');
  await wait(4000);
  const status = await app.text('.synth .ok');
  check(/XP-60 · ID 17/.test(status ?? ''), 'the app shows the connected XP-60 at ID 17', String(status));

  await app.openSynth();
  check(await app.click('Fetch sound', '.synth'), 'Fetch sound can be pressed');
  const fetched = await app.waitMessage(/Fetched/);
  check(!!fetched, 'fetching the temporary sound reports it', String(fetched));

  await app.openSynth();
  check(await app.click('Verify', '.synth'), 'Verify can be pressed');
  const verified = await app.waitMessage(/Verified|Mismatch/);
  check(/Verified/.test(verified ?? ''), 'the instrument\'s temporary copy matches what the app holds', String(verified));
  console.log('all messages:', JSON.stringify(await app.messages()));
  console.log('state chip:', await app.js(`[...document.querySelectorAll('[class*=chip]')].map(e=>e.textContent.trim()).join(' | ')`));

  check(await app.click('Bank', 'header'), 'switch to Bank while connected');
  check(await app.click('Patch', 'header'), 'switch back to Patch while connected');

  await app.openSynth();
  check(await app.click('Disconnect', '.synth'), 'Disconnect can be pressed');
  await wait(1500);
  await app.openSynth();
  check(!(await app.text('.synth .ok')), 'after Disconnect the app shows no instrument');
  check((await app.text('[role=combobox][aria-label="MIDI output"]'))?.includes('FCB1010 OUT'), 'workspace changes and Disconnect preserve the selected hardware output');
  await app.openSynth();
  check(await app.click('Connect', '.synth'), 'Connect can be pressed again');
  await wait(4000);
  await app.openSynth();
  check(/XP-60 · ID 17/.test((await app.text('.synth .ok')) ?? ''), 'it reconnects to the XP-60');
  check(app.pageErrors.length === 0, 'no script errors in the window', app.pageErrors.slice(0, 2).join(' | '));
} finally {
  app.stop();
}
console.log(results.every(Boolean) ? 'ALL PASSED' : 'SOME FAILED');
process.exit(results.every(Boolean) ? 0 : 1);
