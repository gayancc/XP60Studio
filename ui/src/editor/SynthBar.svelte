<script lang="ts">
  import { onMount } from 'svelte';
  import { Dropdown, Stepper } from '../kit';
  import { MAX_CHUNK, MIN_CHUNK, sendOptions, setChunk } from '../model/sendOptions.svelte';
  import type { SynthService } from '../model/synth.svelte';
  import { useStore } from './context';

  interface Props {
    synth: SynthService;
  }

  let { synth }: Props = $props();
  const store = useStore();

  let input = $state(0);
  let output = $state(0);
  let device = $state(16);

  onMount(() => {
    void synth.refreshPorts();
  });

  const connect = (): Promise<void> =>
    synth.connect(synth.ports.inputs[input] ?? '', synth.ports.outputs[output] ?? '', device);

  async function fetchSound(): Promise<void> {
    await store.whenQueueIdle();
    const rev = store.revision;
    const bytes = await synth.fetchTemp(store.kind);
    if (!bytes) return;
    // Edits made while the read was in flight would be thrown away by opening the instrument's sound.
    if (rev !== store.revision) {
      store.error = 'The patch changed while reading the instrument; fetch again.';
      return;
    }
    store.openFromInstrument(bytes);
    if (!store.error) synth.say(store.kind === 'system' ? 'Read the instrument\'s system settings.' : `Fetched "${store.name.trimEnd()}" from the instrument.`);
  }

  async function disconnect(): Promise<void> {
    await synth.disconnect();
  }

  async function verify(): Promise<void> {
    await store.whenQueueIdle();
    const rev = store.revision;
    const bytes = await synth.fetchTemp(store.kind);
    if (!bytes) return;
    if (rev !== store.revision || store.queue.pending > 0) {
      store.error = 'The patch changed while verifying; verify again.';
      return;
    }
    synth.say(store.verifyAgainst(bytes) ? 'Verified: the instrument\'s temporary copy matches this sound.' : 'Mismatch: the instrument\'s temporary copy differs from this sound.');
  }
</script>

<div class="synth">
  {#if synth.connected}
    <span class="ok" role="status">XP-60 · ID {synth.info?.device_number} · “{synth.info?.patch_name}”</span>
    <button type="button" disabled={synth.busy} onclick={fetchSound}>Fetch sound</button>
    <Stepper
      value={synth.pacing}
      min={20}
      max={250}
      label="Write gap"
      name="Gap between writes in milliseconds"
      context="Synth"
      format={(v) => `${v} ms · ${Math.round(1000 / v)}/s`}
      onchange={(v) => void synth.setPacing(v)}
    />
    <Stepper
      value={sendOptions.chunk}
      min={MIN_CHUNK}
      max={MAX_CHUNK}
      label="Chunk"
      name="Most data bytes in one message when a whole sound is sent"
      context="Synth"
      format={(v) => `${v} B`}
      onchange={(v) => setChunk(v)}
    />
    {#if store.kind !== 'system'}
      <button type="button" disabled={synth.busy} onclick={verify} title="Read the temporary copy back and compare it with this one">Verify</button>
    {/if}
    <button type="button" onclick={disconnect}>Disconnect</button>
  {:else}
    <Dropdown options={synth.ports.inputs.length ? synth.ports.inputs : ['(no MIDI inputs)']} selected={input} name="MIDI input" width={150} onchange={(i) => (input = i)} />
    <Dropdown options={synth.ports.outputs.length ? synth.ports.outputs : ['(no MIDI outputs)']} selected={output} name="MIDI output" width={150} onchange={(i) => (output = i)} />
    <Stepper bind:value={device} min={16} max={31} name="Device ID" context="Synth" format={(v) => String(v + 1)} />
    <button type="button" disabled={synth.busy || !synth.ports.inputs.length} onclick={connect}>{synth.busy ? 'Connecting…' : 'Connect'}</button>
    <button type="button" onclick={() => synth.refreshPorts()} title="Look for MIDI ports again">Rescan</button>
  {/if}
  {#if synth.error}<span class="err" role="alert">{synth.error}</span>{/if}
</div>

<style>
  .synth {
    display: flex;
    align-items: flex-end;
    gap: 6px;
    flex-wrap: wrap;
  }
  button {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
  }
  button:hover:not(:disabled) {
    background: var(--well);
  }
  button:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
  button:disabled {
    opacity: 0.4;
    cursor: default;
  }
  .ok {
    color: var(--ok);
    align-self: center;
  }
  .err {
    color: var(--bad);
    align-self: center;
  }
</style>
