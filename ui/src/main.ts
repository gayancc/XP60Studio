import { mount } from 'svelte';
import './theme/tokens.css';
import Editor from './editor/Editor.svelte';
import { applyTheme, storedTheme } from './theme/theme';
import { parseCatalog } from './model/catalog';
import { PatchStore } from './model/store.svelte';
import { runningInDesktopApp, SynthService, TauriTransport } from './model/synth.svelte';
import { tauriBackend } from './model/backend';
import { BankService } from './model/bank.svelte';
import { controls, tauriControlDeps } from './model/controls.svelte';
import { installDesktopChrome } from './model/desktopChrome';
import { LibraryService } from './model/library.svelte';
import { preview } from './model/preview.svelte';
import { cutFrames } from './model/sendOptions.svelte';
import { splitFrames } from './model/frames';
import { SaveFlow } from './model/save.svelte';
import { MemoryTransport, PacedTransport } from './model/transport';
import Storybook from './storybook/Storybook.svelte';
import init, { catalogJson, PatchDoc } from './wasm/pkg/xppatch.js';
import wasmUrl from './wasm/pkg/xppatch_bg.wasm?url';

applyTheme(storedTheme());

const target = document.getElementById('app');
if (!target) throw new Error('missing #app element');

async function start(host: HTMLElement): Promise<void> {
  // `#kit` opens the control-kit storybook; everything else opens the patch editor.
  if (location.hash === '#kit') {
    mount(Storybook, { target: host });
    return;
  }
  await init({ module_or_path: wasmUrl });
  // In the desktop app live edits go to the instrument through Rust; in a browser they are only recorded.
  const desktop = runningInDesktopApp();
  // This is a desktop application: no browser menu, reload, print or page zoom.
  if (desktop) installDesktopChrome();
  // Every store sends through one gate so their queues cannot jointly outrun the instrument.
  const transport = desktop ? new PacedTransport(new TauriTransport()) : new MemoryTransport();
  const tauri = desktop ? (transport as PacedTransport).inner : undefined;
  const catalog = parseCatalog(catalogJson());
  const stores = {
    patch: new PatchStore(catalog, transport),
    performance: new PatchStore(catalog, transport, 0x10, undefined, 'performance'),
    rhythm: new PatchStore(catalog, transport, 0x10, undefined, 'rhythm'),
    system: new PatchStore(catalog, transport, 0x10, undefined, 'system'),
  };
  // The three sounds start from the neutral sound (full ranges, first tone on), not from all-minimum values that play
  // nothing. System settings stay as they are: they stand for the instrument's own memory.
  for (const kind of ['patch', 'performance', 'rhythm'] as const) stores[kind].newPatch();
  const synth = tauri instanceof TauriTransport ? new SynthService(tauri, transport instanceof PacedTransport ? transport : undefined) : undefined;
  // A Program Change from the panel makes every temporary copy out of date (System settings are unaffected).
  synth?.startStalePolling(() => {
    for (const [kind, s] of Object.entries(stores)) if (kind !== 'system') s.markStale();
  });
  // Every document kind shares the connection: tell them all when it opens or closes.
  synth?.onconnection((info) => {
    for (const s of Object.values(stores)) {
      if (info) s.setDevice(info.device_id);
      s.connectionChanged(info !== null);
    }
  });
  if (synth) {
    controls.attach(tauriControlDeps(() => synth.connected, () => synth.busy), stores, catalog);
    controls.start();
  }
  const services = desktop ? { library: new LibraryService(tauriBackend), save: new SaveFlow(tauriBackend), bank: new BankService(tauriBackend) } : undefined;
  if (synth && services) {
    // Hover audition: a stored patch goes to the instrument's temporary patch and the instrument's own sound is put back.
    preview.attach({
      connected: () => synth.connected,
      readTemp: () => synth.fetchTemp('patch'),
      readStored: (id) => services.library.fetchBlocks(id),
      sendTemp: async (blocks) => {
        for (const frame of splitFrames(cutFrames(PatchDoc.fromBlocks(blocks).patchFrames(stores.patch.deviceId)))) await transport.send(frame);
      },
      markStale: () => stores.patch.markStale(),
    });
  }
  mount(Editor, { target: host, props: { stores, synth, services } });
  // Development only: lets tests and the preview load a bank without a file dialog.
  if (import.meta.env.DEV) (window as unknown as { __store: PatchStore; __stores: typeof stores }).__store = stores.patch;
  if (import.meta.env.DEV) (window as unknown as { __stores: typeof stores }).__stores = stores;
}

start(target).catch((e: unknown) => {
  target.textContent = `Could not start: ${e instanceof Error ? e.message : String(e)}`;
});
