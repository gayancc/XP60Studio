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

  const connect = (): Promise<void> => synth.connect(synth.input, synth.output, synth.device);

  function onThruChannel(i: number): void {
    void synth.setThruChannel(i <= 0 ? null : i - 1);
  }

  async function fetchSound(): Promise<void> {
    await store.whenQueueIdle();
    const rev = store.revision;
    const bytes = await synth.fetchTemp(store.kind);
    if (!bytes) return;
    if (rev !== store.revision) {
      store.error = 'The patch changed while reading the instrument; fetch again.';
      return;
    }
    store.openFromInstrument(bytes);
    if (!store.error) synth.say(store.kind === 'system' ? "Read the instrument's system settings." : `Fetched "${store.name.trimEnd()}" from the instrument.`);
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
    synth.say(store.verifyAgainst(bytes) ? "Verified: the instrument's temporary copy matches this sound." : "Mismatch: the instrument's temporary copy differs from this sound.");
  }
</script>

<!-- Layout mirrors FL Studio Settings → MIDI: compact group boxes, small buttons, dense labels (measured 2026-11-07). -->
<div class="synth" class:linked={synth.connected}>
  {#if synth.connected}
    <div class="box ok" role="status" aria-label="Connected to XP-60 ID {synth.info?.device_number}, {patchName}">
      <div class="box-head">
        <span class="led" aria-hidden="true"></span>
        <span class="box-title">XP-60 · ID {synth.info?.device_number}</span>
        <span class="box-meta">“{patchName}”</span>
      </div>
      <div class="btnrow">
        <button type="button" class="btn" disabled={synth.busy} onclick={fetchSound}>Fetch sound</button>
        {#if store.kind !== 'system'}
          <button type="button" class="btn" disabled={synth.busy} onclick={verify} title="Read the temporary copy back and compare it with this one">Verify</button>
        {/if}
        <button type="button" class="btn danger" onclick={disconnect}>Disconnect</button>
      </div>
    </div>

    <div class="box" aria-label="Send pacing">
      <div class="box-head"><span class="box-title">Send</span></div>
      <div class="btnrow wrap">
        <Stepper
          contrast
          value={synth.pacing}
          min={20}
          max={250}
          width={96}
          label="Write gap"
          name="Gap between writes in milliseconds"
          context="Synth"
          format={(v) => `${v} ms`}
          onchange={(v) => void synth.setPacing(v)}
        />
        <Stepper
          contrast
          value={sendOptions.chunk}
          min={MIN_CHUNK}
          max={MAX_CHUNK}
          width={48}
          label="Chunk"
          name="Most data bytes in one message when a whole sound is sent"
          context="Synth"
          format={(v) => `${v} B`}
          onchange={(v) => setChunk(v)}
        />
      </div>
    </div>

    <div class="box" aria-label="Controller pass-through">
      <div class="box-head"><span class="box-title">Controller</span><span class="box-meta">pass-through</span></div>
      <div class="grid">
        <span class="lab">Input</span>
        <Dropdown
          options={thruChoices.length ? thruChoices.map((p) => (controllerInputs.includes(p) ? p : `${p} (unavailable)`)) : ['(no other MIDI inputs)']}
          selected={Math.max(0, thruChoices.indexOf(synth.thruInput))}
          name="Controller MIDI input"
          width={200}
          onchange={(i) => (synth.thruInput = thruChoices[i] ?? '')}
        />
        <span class="lab">Channel</span>
        <div class="inline">
          <Dropdown options={channelLabels} selected={channelIndex} name="Pass-through channel" width={72} onchange={onThruChannel} />
          {#if synth.thru.active}
            <ToggleLed
              checked={synth.thru.enabled && !synth.thru.held}
              label={synth.thru.held ? 'Thru (held)' : 'Thru'}
              name="Forward controller notes to the XP-60"
              context="Synth"
              onchange={(on) => void synth.setThruEnabled(on)}
            />
            <button type="button" class="btn" onclick={() => void synth.stopThru()} title="Close the controller input">Stop</button>
          {:else}
            <button
              type="button"
              class="btn accent"
              disabled={synth.busy || !synth.thruInput || !controllerInputs.includes(synth.thruInput)}
              onclick={() => void synth.startThru()}
              title="Route Note/CC/pitch-bend from this keyboard to the XP-60 output"
            >Start thru</button>
          {/if}
        </div>
      </div>
    </div>
  {:else}
    <div class="box" aria-label="MIDI input">
      <div class="box-head"><span class="box-title">Input</span><span class="box-meta">from XP-60</span></div>
      <Dropdown
        options={inputs.length ? inputs.map((p) => (synth.ports.inputs.includes(p) ? p : `${p} (unavailable)`)) : ['(no MIDI inputs)']}
        selected={inputs.indexOf(synth.input)}
        name="MIDI input"
        width={260}
        onchange={(i) => (synth.input = inputs[i] ?? '')}
      />
    </div>

    <div class="box" aria-label="MIDI output">
      <div class="box-head"><span class="box-title">Output</span><span class="box-meta">to XP-60</span></div>
      <Dropdown
        options={outputs.length ? outputs.map((p) => (synth.ports.outputs.includes(p) ? p : `${p} (unavailable)`)) : ['(no MIDI outputs)']}
        selected={outputs.indexOf(synth.output)}
        name="MIDI output"
        width={260}
        onchange={(i) => (synth.output = outputs[i] ?? '')}
      />
    </div>

    <div class="box" aria-label="Device">
      <div class="box-head"><span class="box-title">Device</span></div>
      <div class="inline">
        <span class="lab">ID</span>
        <Stepper contrast bind:value={synth.device} min={16} max={31} name="Device ID" context="Synth" format={(v) => String(v + 1)} />
        <span class="hint">Panel number 17–32</span>
      </div>
    </div>

    {#if missing}
      <p class="note warn">A chosen port is offline — refresh after reconnecting the interface.</p>
    {/if}

    <div class="btnrow footer">
      <button type="button" class="btn" onclick={() => synth.refreshPorts()} title="Look for MIDI ports again" aria-label="Rescan">Refresh</button>
      <button type="button" class="btn accent" disabled={synth.busy || missing || !synth.input || !synth.output} onclick={connect}>
        {synth.busy ? 'Connecting…' : 'Connect'}
      </button>
    </div>
  {/if}
</div>

<style>
  /*
   * FL Studio Settings → MIDI pattern (live capture 2026-11-07):
   * charcoal group boxes, 11–12px labels, ~22px chrome buttons, no oversized CTAs.
   * Colours locked to FL graphite so light theme menus do not wash the strip out.
   */
  .synth {
    --fl-ink: #d5dce0;
    --fl-dim: #8b969c;
    --fl-panel: #353e44;
    --fl-box: #3c454b;
    --fl-edge: #252c30;
    --fl-chrome: #4c545a;
    --fl-btn: #5a6164;
    --fl-btn-hi: #6f7679;
    --fl-led: #97db3a;
    --fl-accent-ink: #1c1208;

    display: flex;
    flex-direction: column;
    gap: 6px;
    width: 300px;
    padding: 8px;
    background: var(--fl-panel);
    border: 1px solid var(--fl-edge);
    color: var(--fl-ink);
    font: 12px/1.25 var(--font);
  }

  .box {
    display: flex;
    flex-direction: column;
    gap: 5px;
    padding: 6px 7px 7px;
    background: var(--fl-box);
    border: 1px solid var(--fl-edge);
  }
  .box.ok {
    border-color: color-mix(in srgb, var(--fl-led) 35%, var(--fl-edge));
  }
  .box-head {
    display: flex;
    align-items: center;
    gap: 6px;
    min-width: 0;
  }
  .box-title {
    font: 600 10px var(--font-caps);
    letter-spacing: 0.06em;
    text-transform: uppercase;
    color: var(--fl-ink);
  }
  .box-meta {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font-size: 11px;
    color: var(--fl-dim);
  }
  .led {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    background: var(--fl-led);
    box-shadow: 0 0 5px color-mix(in srgb, var(--fl-led) 55%, transparent);
  }

  .grid {
    display: grid;
    grid-template-columns: 48px minmax(0, 1fr);
    gap: 4px 6px;
    align-items: center;
  }
  .lab {
    font: 600 10px var(--font-caps);
    letter-spacing: 0.04em;
    text-transform: uppercase;
    color: var(--fl-dim);
    text-align: right;
  }
  .hint {
    font-size: 10px;
    color: var(--fl-dim);
  }
  .note {
    margin: 0;
    font-size: 11px;
    color: var(--fl-dim);
  }
  .note.warn {
    color: #e0a060;
  }

  .inline,
  .btnrow {
    display: flex;
    align-items: center;
    gap: 5px;
    flex-wrap: nowrap;
  }
  .btnrow.wrap,
  .inline {
    flex-wrap: wrap;
  }
  .btnrow.footer {
    justify-content: flex-end;
    padding-top: 2px;
  }

  /* FL chrome button: ~22px tall, subtle face, never a banner CTA. */
  .btn {
    appearance: none;
    height: 22px;
    padding: 0 9px;
    border: 1px solid #2a3135;
    border-radius: 2px;
    background: linear-gradient(180deg, var(--fl-btn-hi), var(--fl-btn));
    color: var(--fl-ink);
    font: 600 11px var(--font);
    cursor: pointer;
    white-space: nowrap;
  }
  .btn:hover:not(:disabled) {
    background: linear-gradient(180deg, #7a8184, #636a6d);
  }
  .btn:focus-visible {
    outline: none;
    background-image: var(--focus-fill), linear-gradient(180deg, var(--fl-btn-hi), var(--fl-btn));
  }
  .btn:disabled {
    opacity: 0.45;
    cursor: default;
  }
  .btn.accent {
    background: linear-gradient(180deg, #ffb054, var(--accent));
    color: var(--fl-accent-ink);
    border-color: color-mix(in srgb, var(--accent) 50%, #000);
  }
  .btn.accent:hover:not(:disabled) {
    background: linear-gradient(180deg, #ffc078, #ff9a38);
  }
  .btn.accent:focus-visible {
    background-image: var(--focus-fill), linear-gradient(180deg, #ffb054, var(--accent));
  }
  .btn.danger {
    color: #ffc8c4;
  }
  .btn.danger:hover:not(:disabled) {
    background: color-mix(in srgb, var(--bad) 30%, var(--fl-btn));
  }
</style>
