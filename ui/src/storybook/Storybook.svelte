<script lang="ts">
  import {
    ContextMenu,
    DockPanel,
    Dropdown,
    EnvelopeGraph,
    GroupFrame,
    HintBar,
    KeyRangeBar,
    Knob,
    Led,
    Meter,
    Segmented,
    Slider,
    StateChip,
    Stepper,
    ToggleLed,
  } from '../kit';
  import { SYNC_ORDER, SYNC_STATES, type SyncState } from '../kit/syncState';
  import type { Stage } from '../kit/envelope';

  const tones = ['var(--tone-1)', 'var(--tone-2)', 'var(--tone-3)', 'var(--tone-4)'];

  let cutoff = $state(64);
  let resonance = $state(20);
  let pan = $state(0);
  let octave = $state(1);
  let level = $state(100);
  let tiny = $state(40);
  let small = $state(70);
  let big = $state(90);
  let toneKnobs = $state([100, 80, 60, 40]);
  let slider = $state(80);
  let fader = $state(100);
  let sliderPan = $state(0);
  let on1 = $state(true);
  let on2 = $state(false);
  let mode = $state(0);
  let wave = $state(0);
  let step = $state(3);
  let focused = $state('tone');
  let collapsed = $state(false);
  let sound = $state<SyncState>('tempVerified');
  let queued = $state(12);
  let range = $state({ low: 36, high: 84 });
  let stages = $state<Stage[]>([
    { time: 10, level: 127 },
    { time: 50, level: 90 },
    { time: 30, level: 90 },
    { time: 70, level: 0 },
  ]);
  const stageNames = ['Time 1 / Level 1', 'Time 2 / Level 2', 'Time 3 / Level 3', 'Time 4 / Level 4'];

  const fmtPan = (v: number): string => (v === 0 ? 'Center' : v < 0 ? `L${-v}` : `R${v}`);
  const fmtOct = (v: number): string => (v > 0 ? `+${v} oct` : `${v} oct`);
</script>

<ContextMenu />

<div class="page">
  <header class="top">
    <h1>Control kit</h1>
    <p class="caps">XPPatchStudio · every control, every state · hover, drag, wheel, right-click, arrows</p>
  </header>

  <main>
    <GroupFrame title="Knobs: four sizes">
      <Knob bind:value={tiny} size={20} label="Tiny" name="Tiny knob" context="Kit > Knobs" />
      <Knob bind:value={small} size={30} label="Small" name="Small knob" context="Kit > Knobs" />
      <Knob bind:value={cutoff} size={38} label="Cutoff" name="Cutoff Frequency" context="Tone 1 > TVF" defaultValue={127} />
      <Knob bind:value={big} size={64} label="Master" name="Master level" context="Kit > Knobs" />
    </GroupFrame>

    <GroupFrame title="Knobs: bipolar, formatted, disabled" color="var(--tone-2)">
      <Knob bind:value={pan} min={-50} max={50} origin={0.5} defaultValue={0} label="Pan" name="Pan" context="Tone 2 > TVA" format={fmtPan} color="var(--tone-2)" />
      <Knob bind:value={octave} min={-3} max={3} origin={0.5} defaultValue={0} label="Octave" name="Octave" context="Tone 2 > Pitch" format={fmtOct} color="var(--tone-2)" />
      <Knob bind:value={resonance} label="Reso" name="Resonance" context="Tone 2 > TVF" color="var(--tone-2)" />
      <Knob value={55} label="Locked" disabled />
    </GroupFrame>

    <GroupFrame title="Tone identity colours" color="var(--tone-3)">
      {#each tones as c, i (i)}
        <Knob bind:value={toneKnobs[i] as number} label="Tone {i + 1}" name="Tone {i + 1} level" context="Patch > Mixer" color={c} />
      {/each}
    </GroupFrame>

    <GroupFrame title="Sliders" color="var(--tone-4)">
      <Slider bind:value={slider} label="Horizontal" name="Slider" context="Kit > Sliders" />
      <Slider bind:value={sliderPan} min={-50} max={50} origin={0.5} defaultValue={0} label="Pan" format={fmtPan} context="Kit > Sliders" color="var(--tone-4)" />
      <Slider bind:value={fader} orientation="vertical" length={90} label="Fader" context="Kit > Sliders" />
      <Slider value={30} disabled label="Locked" />
    </GroupFrame>

    <GroupFrame title="Switches and choices">
      <ToggleLed bind:checked={on1} label="Tone 1" color="var(--tone-1)" context="Patch > Mixer" />
      <ToggleLed bind:checked={on2} label="Solo" color="var(--warn)" context="Patch > Mixer" />
      <ToggleLed checked={true} label="Locked" disabled />
      <Segmented options={['Mono', 'Poly', 'Legato']} bind:selected={mode} name="Voice mode" context="Patch > Common" />
      <Stepper bind:value={step} min={0} max={15} label="Channel" name="Part channel" context="Perf > Part" format={(v) => String(v + 1)} />
      <Dropdown options={['Sawtooth', 'Square', 'Triangle', 'Sine']} bind:selected={wave} label="LFO shape" context="Tone 1 > LFO" />
    </GroupFrame>

    <GroupFrame title="Indicators">
      <span class="ind"><Led on={true} label="OK" /> <span class="caps">On</span></span>
      <span class="ind"><Led on={false} label="Off" /> <span class="caps">Off</span></span>
      <span class="ind"><Led on={true} color="var(--warn)" /> <span class="caps">Busy</span></span>
      <span class="ind"><Led on={true} color="var(--bad)" /> <span class="caps">Fault</span></span>
      <span class="ind"><Meter level={0.3} label="Low" /> <span class="caps">30%</span></span>
      <span class="ind"><Meter level={0.78} label="High" /> <span class="caps">78%</span></span>
      <span class="ind"><Meter level={0.96} label="Peak" /> <span class="caps">96%</span></span>
    </GroupFrame>

    <GroupFrame title="Sound state chip" color="var(--info)">
      {#each SYNC_ORDER as s (s)}
        <StateChip state={s} />
      {/each}
    </GroupFrame>

    <GroupFrame title="Envelope graph" color="var(--tone-1)">
      <EnvelopeGraph bind:stages {stageNames} name="TVF envelope" context="Tone 1 > TVF" color="var(--tone-1)" />
      <EnvelopeGraph stages={[{ time: 20, level: 100 }, { time: 60, level: 40 }]} name="Locked envelope" disabled color="var(--tone-3)" width={160} height={90} />
    </GroupFrame>

    <GroupFrame title="Key range" color="var(--tone-1)">
      <KeyRangeBar bind:low={range.low} bind:high={range.high} name="Tone 1 key range" context="Tone 1 > Zone" color="var(--tone-1)" />
    </GroupFrame>

    <div class="panels">
      <DockPanel title="Tone 1" meta="focused panel" focused={focused === 'tone'} onfocus={() => (focused = 'tone')}>
        <p class="note">The focused panel’s caption lights in the accent colour; clicking the caption collapses it.</p>
      </DockPanel>
      <DockPanel title="Browser" meta="click to collapse" bind:collapsed focused={focused === 'browser'} onfocus={() => (focused = 'browser')}>
        <p class="note">Second panel body.</p>
      </DockPanel>
    </div>
  </main>

  <div class="bar">
    <HintBar
      sound={sound}
      midiIn={true}
      midiOut={on1}
      {queued}
      queueState={queued > 40 ? 'behind' : 'ok'}
    />
    <div class="demo">
      <Slider bind:value={queued} min={0} max={64} length={90} label="Demo: queue" />
      <Segmented options={SYNC_ORDER.map((s) => SYNC_STATES[s].label)} selected={SYNC_ORDER.indexOf(sound)} onchange={(i) => (sound = SYNC_ORDER[i] ?? 'local')} name="Demo: sound state" />
    </div>
  </div>
</div>

<style>
  .page {
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    padding-bottom: 96px;
  }
  .top {
    padding: 10px 14px;
    background: var(--caption);
    border-bottom: 1px solid var(--edge);
  }
  h1 {
    margin: 0;
    font: 600 15px var(--font-caps);
    letter-spacing: 0.04em;
  }
  main {
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
    padding: 14px;
    align-items: flex-start;
  }
  .ind {
    display: inline-flex;
    gap: 5px;
    align-items: center;
  }
  .panels {
    display: flex;
    flex-direction: column;
    gap: 8px;
    min-width: 260px;
  }
  .note {
    margin: 0;
    color: var(--text-dim);
    max-width: 240px;
  }
  .bar {
    position: fixed;
    left: 0;
    right: 0;
    bottom: 0;
  }
  .demo {
    display: flex;
    gap: 14px;
    align-items: center;
    padding: 4px 10px;
    background: var(--body-deep);
    border-top: 1px solid var(--edge);
  }
</style>
