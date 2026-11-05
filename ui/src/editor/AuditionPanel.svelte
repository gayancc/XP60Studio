<script lang="ts">
  import { onDestroy } from 'svelte';
  import { PianoKeys, Stepper } from '../kit';
  import { MAX_OCTAVE, MIN_OCTAVE, noteForCode, velocityAt } from '../model/keyboard';
  import { midiNoteName } from '../model/labels';
  import type { SynthService } from '../model/synth.svelte';

  interface Props {
    synth?: SynthService;
  }

  let { synth }: Props = $props();

  const KEYS = 37; // three octaves and a C
  let channel = $state(0);
  let octave = $state(3);
  /** Each key that is down (by a pointer id or a computer key code), with the note and the channel it was sent on. */
  const heldBy = new Map<string, { note: number; channel: number }>();
  let down = $state(new Set<number>());
  const connected = $derived(synth?.connected ?? false);
  const first = $derived((octave + 1) * 12);
  const UNIT = 9.4; // pixels per semitone
  const H = 84;

  const sync = (): void => {
    down = new Set([...heldBy.values()].map((h) => h.note));
  };

  function press(id: string, note: number, velocity: number): void {
    if (heldBy.has(id)) return;
    heldBy.set(id, { note, channel });
    sync();
    void synth?.note(channel, note, velocity, true);
  }

  function release(id: string): void {
    const h = heldBy.get(id);
    if (!h) return;
    heldBy.delete(id);
    sync();
    // Another key may still be holding the same note on the same channel; only the last release ends it.
    const stillHeld = [...heldBy.values()].some((o) => o.note === h.note && o.channel === h.channel);
    if (!stillHeld) void synth?.note(h.channel, h.note, 0, false);
  }

  /** Release everything on the channels it was played on, with All Notes Off as a backstop. */
  function releaseAll(): void {
    const channels = new Set([...heldBy.values()].map((h) => h.channel));
    for (const h of heldBy.values()) void synth?.note(h.channel, h.note, 0, false);
    heldBy.clear();
    sync();
    for (const c of channels) void synth?.allNotesOff(c);
  }

  function panic(): void {
    releaseAll();
    void synth?.allNotesOff(channel);
  }

  onDestroy(releaseAll);

  function pointerDown(note: number, e: PointerEvent): void {
    if (e.button !== 0) return;
    const el = e.currentTarget as SVGElement;
    const r = el.getBoundingClientRect();
    el.setPointerCapture(e.pointerId);
    press(`p${e.pointerId}`, note, velocityAt((e.clientY - r.top) / (r.height || 1)));
  }

  function keyDown(e: KeyboardEvent): void {
    if (e.repeat || e.ctrlKey || e.metaKey || e.altKey) return;
    const note = noteForCode(e.code, octave);
    if (note === null) return;
    e.preventDefault();
    press(`k${e.code}`, note, 90);
  }

  function keyUp(e: KeyboardEvent): void {
    // Released by the key, not by recomputing the note: the octave may have changed while it was held.
    release(`k${e.code}`);
  }
</script>

<svelte:window onblur={releaseAll} />

<div class="aud">
  {#if !connected}
    <p class="dim">Connect the XP-60 to play it from here. Notes make sound but never write anything.</p>
  {/if}
  <div class="row">
    <Stepper bind:value={channel} min={0} max={15} label="Channel" name="Audition channel" context="Audition" format={(v) => String(v + 1)} onchange={releaseAll} />
    <button type="button" onclick={() => (octave = Math.max(MIN_OCTAVE, octave - 1))} aria-label="Octave down">Oct −</button>
    <span class="caps">{midiNoteName(first)}</span>
    <button type="button" onclick={() => (octave = Math.min(MAX_OCTAVE, octave + 1))} aria-label="Octave up">Oct +</button>
    <button type="button" onclick={panic} title="Silence everything on this channel">Panic</button>
  </div>
  <div
    class="kb"
    role="toolbar"
    aria-label="On-screen keyboard. With this focused, Z to M and Q to I play notes."
    tabindex="0"
    onkeydown={keyDown}
    onkeyup={keyUp}
    onfocusout={releaseAll}
  >
    <PianoKeys
      first={first}
      last={first + KEYS - 1}
      width={KEYS * UNIT}
      height={H}
      held={down}
      ondown={pointerDown}
      onup={(e) => release(`p${e.pointerId}`)}
    />
  </div>
  <p class="dim">Click a key (lower is louder) or focus the keyboard and use Z to M and Q to I.</p>
</div>

<style>
  .row {
    display: flex;
    gap: 8px;
    align-items: flex-end;
    margin-bottom: 8px;
    flex-wrap: wrap;
  }
  button {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 3px 10px;
    cursor: pointer;
    color: inherit;
    font: inherit;
  }
  button:hover {
    background: var(--well);
  }
  button:focus-visible,
  .kb:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
  .kb {
    overflow-x: auto;
    border-radius: var(--radius);
  }
  .dim {
    color: var(--text-dim);
    font-size: 11px;
    margin: 4px 0 0;
  }
</style>
