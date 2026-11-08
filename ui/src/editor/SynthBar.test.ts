import { afterEach, beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/svelte';
import { readFileSync } from 'node:fs';
import { invoke } from '@tauri-apps/api/core';
import { initSync, catalogJson } from '../wasm/pkg/xppatch.js';
import { parseCatalog } from '../model/catalog';
import { PatchStore } from '../model/store.svelte';
import { MemoryTransport } from '../model/transport';
import { SynthService, TauriTransport } from '../model/synth.svelte';
import SynthHarness from './SynthHarness.svelte';

vi.mock('@tauri-apps/api/core', () => ({ invoke: vi.fn(), isTauri: () => true }));
beforeAll(() => initSync({ module: readFileSync('src/wasm/pkg/xppatch_bg.wasm') }));
afterEach(cleanup);
let outputs: string[];
beforeEach(() => {
  outputs = ['Microsoft GS Wavetable Synth', 'FCB1010 OUT'];
  vi.mocked(invoke).mockReset();
  vi.mocked(invoke).mockImplementation(async (command) => {
    if (command === 'midi_ports') return { inputs: ['FCB1010 IN'], outputs: [...outputs] };
    if (command === 'midi_connect') return { device_id: 17, device_number: 18, patch_name: 'Test' };
    return undefined;
  });
});

const setup = () => {
  const store = new PatchStore(parseCatalog(catalogJson()), new MemoryTransport());
  const synth = new SynthService(new TauriTransport());
  return { store, synth };
};
const pickOutput = async () => {
  await waitFor(() => expect(screen.getByRole('combobox', { name: 'MIDI input' }).textContent).toContain('FCB1010 IN'));
  await fireEvent.click(screen.getByRole('combobox', { name: 'MIDI output' }));
  await fireEvent.click(screen.getByRole('option', { name: 'FCB1010 OUT' }));
};

describe('MIDI connection selection', () => {
  it('keeps selected ports and device ID when the Synth panel is recreated', async () => {
    const props = setup();
    const view = render(SynthHarness, { props });
    await pickOutput();
    await fireEvent.keyDown(screen.getByRole('spinbutton', { name: 'Device ID' }), { key: 'ArrowUp' });
    await view.rerender({ ...props, visible: false });
    await view.rerender({ ...props, visible: true });
    await waitFor(() => expect(screen.getByRole('combobox', { name: 'MIDI output' }).textContent).toContain('FCB1010 OUT'));
    expect(screen.getByRole('spinbutton', { name: 'Device ID' }).getAttribute('aria-valuenow')).toBe('17');
    await fireEvent.click(screen.getByRole('button', { name: 'Connect' }));
    await waitFor(() => expect(invoke).toHaveBeenCalledWith('midi_connect', { input: 'FCB1010 IN', output: 'FCB1010 OUT', device: 17 }));
  });

  it('keeps the chosen port by name when discovery changes the list order', async () => {
    render(SynthHarness, { props: setup() });
    await pickOutput();
    outputs.reverse();
    await fireEvent.click(screen.getByRole('button', { name: 'Rescan' }));
    await waitFor(() => expect(invoke).toHaveBeenCalledTimes(2));
    expect(screen.getByRole('combobox', { name: 'MIDI output' }).textContent).toContain('FCB1010 OUT');
    await fireEvent.click(screen.getByRole('button', { name: 'Connect' }));
    await waitFor(() => expect(invoke).toHaveBeenCalledWith('midi_connect', { input: 'FCB1010 IN', output: 'FCB1010 OUT', device: 16 }));
  });

  it('does not replace an unavailable output with another port', async () => {
    render(SynthHarness, { props: setup() });
    await pickOutput();
    outputs = ['Microsoft GS Wavetable Synth'];
    await fireEvent.click(screen.getByRole('button', { name: 'Rescan' }));
    await waitFor(() => expect(screen.getByRole('combobox', { name: 'MIDI output' }).textContent).toContain('unavailable'));
    expect(screen.getByRole('combobox', { name: 'MIDI output' }).textContent).toContain('FCB1010 OUT');
    expect((screen.getByRole('button', { name: 'Connect' }) as HTMLButtonElement).disabled).toBe(true);
    expect(vi.mocked(invoke).mock.calls.some(([command]) => command === 'midi_connect')).toBe(false);
    outputs = ['FCB1010 OUT', 'Microsoft GS Wavetable Synth'];
    await fireEvent.click(screen.getByRole('button', { name: 'Rescan' }));
    await waitFor(() => expect((screen.getByRole('button', { name: 'Connect' }) as HTMLButtonElement).disabled).toBe(false));
    expect(screen.getByRole('combobox', { name: 'MIDI output' }).textContent).toContain('FCB1010 OUT');
  });
});
