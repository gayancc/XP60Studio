<script lang="ts">
  import { onMount } from 'svelte';
  import { Dropdown, Stepper, ToggleLed } from '../kit';
  import { MAX_CHUNK, MIN_CHUNK, sendOptions, setChunk } from '../model/sendOptions.svelte';
  import type { SynthService } from '../model/synth.svelte';
  import { useStore } from './context';

  interface Props {
    synth: SynthService;
  }

  let { synth }: Props = $props();
  const store = useStore();

  const inputs = $derived(synth.input && !synth.ports.inputs.includes(synth.input) ? [synth.input, ...synth.ports.inputs] : synth.ports.inputs);
  const outputs = $derived(synth.output && !synth.ports.outputs.includes(synth.output) ? [synth.output, ...synth.ports.outputs] : synth.ports.outputs);
  const missing = $derived(!!synth.input && !!synth.output && (!synth.ports.inputs.includes(synth.input) || !synth.ports.outputs.includes(synth.output)));
  const patchName = $derived((synth.info?.patch_name ?? '').trimEnd() || '—');
  const controllerInputs = $derived(synth.controllerInputs());
  const thruChoices = $derived(
    synth.thruInput && !controllerInputs.includes(synth.thruInput)
      ? [synth.thruInput, ...controllerInputs]
      : controllerInputs,
  );
  /** Direct + MIDI channels 1–16 (stored as null / 0–15). */
  const channelLabels = ['Direct', ...Array.from({ length: 16 }, (_, i) => `Ch ${i + 1}`)];
  const channelIndex = $derived(synth.thruChannel === null ? 0 : synth.thruChannel + 1);

  onMount(() => {
    void synth.refreshPorts();
  });

  const connect = (): Promise<void> =>
    synth.connect(synth.input, synth.output, synth.device);

  function onThruChannel(i: number): void {
    void synth.setThruChannel(i <= 0 ? null : i - 1);
  }

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

<div class="synth" class:linked={synth.connected}>
  {#if synth.connected}
    <div class="link ok" role="status" aria-label="Connected to XP-60 ID {synth.info?.device_number}, {patchName}">
      <span class="led" aria-hidden="true"></span>
      <div class="who">
        <span class="dev">XP-60 · ID {synth.info?.device_number}</span>
        <span class="patch">“{patchName}”</span>
      </div>
    </div>

    <div class="acts">
      <button type="button" class="act" disabled={synth.busy} onclick={fetchSound}>Fetch sound</button>
      {#if store.kind !== 'system'}
        <button type="button" class="act" disabled={synth.busy} onclick={verify} title="Read the temporary copy back and compare it with this one">Verify</button>
      {/if}
      <button type="button" class="act danger" onclick={disconnect}>Disconnect</button>
    </div>

    <div class="pace" aria-label="Send pacing">
      <Stepper
        contrast
        value={synth.pacing}
        min={20}
        max={250}
        width={100}
        label="Write gap"
        name="Gap between writes in milliseconds"
        context="Synth"
        format={(v) => `${v} ms · ${Math.round(1000 / v)}/s`}
        onchange={(v) => void synth.setPacing(v)}
      />
      <Stepper
        contrast
        value={sendOptions.chunk}
        min={MIN_CHUNK}
        max={MAX_CHUNK}
        width={56}
        label="Chunk"
        name="Most data bytes in one message when a whole sound is sent"
        context="Synth"
        format={(v) => `${v} B`}
        onchange={(v) => setChunk(v)}
      />
    </div>

    <div class="thru" aria-label="Controller pass-through">
      <span class="thru-label">Controller</span>
      <Dropdown
        options={thruChoices.length ? thruChoices.map((p) => controllerInputs.includes(p) ? p : `${p} (unavailable)`) : ['(no other MIDI inputs)']}
        selected={Math.max(0, thruChoices.indexOf(synth.thruInput))}
        name="Controller MIDI input"
        width={150}
        onchange={(i) => (synth.thruInput = thruChoices[i] ?? '')}
      />
      <Dropdown
        options={channelLabels}
        selected={channelIndex}
        name="Pass-through channel"
        width={72}
        onchange={onThruChannel}
      />
      {#if synth.thru.active}
        <ToggleLed
          checked={synth.thru.enabled && !synth.thru.held}
          label={synth.thru.held ? 'Thru (held)' : 'Thru'}
          name="Forward controller notes to the XP-60"
          context="Synth"
          onchange={(on) => void synth.setThruEnabled(on)}
        />
        <button type="button" class="act" onclick={() => void synth.stopThru()} title="Close the controller input">Stop</button>
      {:else}
        <button
          type="button"
          class="act primary"
          disabled={synth.busy || !synth.thruInput || !controllerInputs.includes(synth.thruInput)}
          onclick={() => void synth.startThru()}
          title="Route Note/CC/pitch-bend from this keyboard to the XP-60 output"
        >Start thru</button>
      {/if}
    </div>
  {:else}
    <div class="ports">
      <Dropdown options={inputs.length ? inputs.map((p) => synth.ports.inputs.includes(p) ? p : `${p} (unavailable)`) : ['(no MIDI inputs)']} selected={inputs.indexOf(synth.input)} name="MIDI input" width={150} onchange={(i) => (synth.input = inputs[i] ?? '')} />
      <Dropdown options={outputs.length ? outputs.map((p) => synth.ports.outputs.includes(p) ? p : `${p} (unavailable)`) : ['(no MIDI outputs)']} selected={outputs.indexOf(synth.output)} name="MIDI output" width={150} onchange={(i) => (synth.output = outputs[i] ?? '')} />
      <Stepper contrast bind:value={synth.device} min={16} max={31} name="Device ID" context="Synth" format={(v) => String(v + 1)} />
    </div>
    <div class="acts">
      <button type="button" class="act primary" disabled={synth.busy || missing || !synth.input || !synth.output} onclick={connect}>{synth.busy ? 'Connecting…' : 'Connect'}</button>
      <button type="button" class="act" onclick={() => synth.refreshPorts()} title="Look for MIDI ports again">Rescan</button>
    </div>
  {/if}
</div>

<style>
  /*
   * Instrument strip: locked graphite palette. Light ink on deep wells so contrast
   * never depends on the surrounding theme (light panels used to wash out lime text).
   */
  .synth {
    --sb-ink: #eef3f6;
    --sb-ink-dim: #a3b1b9;
    --sb-bg0: #2a3339;
    --sb-bg1: #1c2429;
    --sb-face: #3d4a52;
    --sb-face-hover: #4d5c66;
    --sb-edge: #0b1013;
    --sb-led: #b7ec4a;
    --sb-accent-ink: #1c1208;

    display: flex;
    align-items: center;
    flex-wrap: wrap;
    gap: 10px 12px;
    padding: 7px 9px;
    border: 1px solid var(--sb-edge);
    border-radius: var(--radius-lg);
    background: linear-gradient(180deg, var(--sb-bg0), var(--sb-bg1));
    box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.05);
    color: var(--sb-ink);
  }

  .link {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    min-width: 0;
    max-width: 260px;
    padding: 4px 10px 4px 8px;
    border-radius: 999px;
    background: #12181c;
    border: 1px solid #303a40;
  }
  .led {
    flex: none;
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--sb-led);
    box-shadow: 0 0 0 2px color-mix(in srgb, var(--sb-led) 28%, transparent), 0 0 6px color-mix(in srgb, var(--sb-led) 55%, transparent);
  }
  .who {
    min-width: 0;
    display: flex;
    flex-direction: column;
    gap: 1px;
    line-height: 1.15;
  }
  .dev {
    font: 600 10px var(--font-caps);
    letter-spacing: 0.07em;
    text-transform: uppercase;
    color: var(--sb-ink-dim);
  }
  .patch {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font: 600 12px var(--font);
    color: var(--sb-ink);
  }

  .acts,
  .pace,
  .ports,
  .thru {
    display: inline-flex;
    align-items: flex-end;
    flex-wrap: wrap;
    gap: 6px;
  }
  .acts,
  .thru {
    align-items: center;
  }
  .thru-label {
    font: 600 10px var(--font-caps);
    letter-spacing: 0.07em;
    text-transform: uppercase;
    color: var(--sb-ink-dim);
  }

  .act {
    appearance: none;
    border: 1px solid var(--sb-edge);
    border-radius: var(--radius);
    padding: 5px 11px;
    background: var(--sb-face);
    color: var(--sb-ink);
    font: 600 12px var(--font);
    cursor: pointer;
  }
  .act:hover:not(:disabled) {
    background: var(--sb-face-hover);
  }
  .act:focus-visible {
    outline: none;
    background-image: var(--focus-fill);
  }
  .act:disabled {
    opacity: 0.45;
    cursor: default;
  }
  .act.primary {
    background: var(--accent);
    color: var(--sb-accent-ink);
    border-color: color-mix(in srgb, var(--accent) 60%, #000);
  }
  .act.primary:hover:not(:disabled) {
    background: color-mix(in srgb, var(--accent) 88%, #fff);
  }
  .act.danger {
    color: #ffd4d0;
    border-color: color-mix(in srgb, var(--bad) 45%, var(--sb-edge));
  }
  .act.danger:hover:not(:disabled) {
    background: color-mix(in srgb, var(--bad) 28%, var(--sb-face));
  }

  .pace {
    margin-left: auto;
    padding-left: 10px;
    border-left: 1px solid #303a40;
  }
</style>
