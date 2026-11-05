<script lang="ts">
  import { KeyMapH, KeyRangeBar, type MapLane } from '../kit';
  import { TONE_BLOCKS, type BlockName } from '../model/catalog';
  import { blockTitle } from '../model/labels';
  import { useStore } from './context';
  import ParamControl from './ParamControl.svelte';

  interface Props {
    colors: readonly string[];
    selected: number;
    onselect: (index: number) => void;
  }

  let { colors, selected, onselect }: Props = $props();
  const store = useStore();

  const GUTTER = 108;
  const colorOf = (i: number): string => colors[i] ?? 'var(--accent)';
  /** Width of the velocity bars: whatever the row leaves after the tone name and the cross-fade knob. */
  let velBox = $state(0);
  const velW = $derived(Math.max(160, velBox || 560));

  const lanes = $derived<MapLane[]>(
    TONE_BLOCKS.map((b, i) => ({
      id: i,
      low: store.get(b, 'keyboard_range_lower'),
      high: store.get(b, 'keyboard_range_upper'),
      color: colorOf(i),
      name: blockTitle(b),
      selected: selected === i,
      dim: store.get(b, 'tone_switch') !== 1,
    })),
  );

  /** Write both ends of a range in an order that never leaves lower above upper, even for a moment. */
  function setRange(block: BlockName, prefix: 'keyboard_range' | 'velocity_range', r: { low: number; high: number }): void {
    const lowId = `${prefix}_lower`;
    const highId = `${prefix}_upper`;
    if (r.low > store.get(block, highId)) {
      store.set(block, highId, r.high);
      store.set(block, lowId, r.low);
    } else {
      store.set(block, lowId, r.low);
      store.set(block, highId, r.high);
    }
  }
</script>

<div class="zones">
  <section>
    <span class="caps">Key range · which keys each tone plays</span>
    <KeyMapH
      {lanes}
      gutterWidth={GUTTER}
      context="Zones"
      keyboardHeight={64}
      laneHeight={26}
      onselect={onselect}
      onbegin={(i) => store.begin(`${blockTitle(TONE_BLOCKS[i] as BlockName)} · Key range`)}
      onend={() => store.end()}
      onrange={(i, r) => setRange(TONE_BLOCKS[i] as BlockName, 'keyboard_range', r)}
    >
      {#snippet gutter(l)}
        <button type="button" class="tag" onclick={() => onselect(l.id)} aria-pressed={selected === l.id}><i></i>{l.name}</button>
      {/snippet}
    </KeyMapH>
  </section>

  <section>
    <span class="caps">Velocity range and cross-fade · how hard each tone is played</span>
    {#each TONE_BLOCKS as block, i (block)}
      {@const color = colorOf(i)}
      <div class="row" class:sel={selected === i} style:--tone={color} style:grid-template-columns="{GUTTER}px minmax(0, 1fr) auto">
        <button type="button" class="tag" onclick={() => onselect(i)} aria-pressed={selected === i}><i></i>{blockTitle(block)}</button>
        <div class="bar" bind:clientWidth={velBox}>
          <KeyRangeBar
            kind="velocity"
            floor={1}
            low={store.get(block, 'velocity_range_lower')}
            high={store.get(block, 'velocity_range_upper')}
            name="{blockTitle(block)} velocity range"
            context="{blockTitle(block)} > Zones"
            {color}
            width={velW}
            height={20}
            onbegin={() => store.begin(`${blockTitle(block)} · Velocity range`)}
            onend={() => store.end()}
            onchange={(r) => setRange(block, 'velocity_range', r)}
          />
        </div>
        <ParamControl {block} id="velocity_cross_fade" label="X-fade" size={20} {color} />
      </div>
    {/each}
  </section>
</div>

<style>
  .zones {
    display: flex;
    flex-direction: column;
    gap: 14px;
    min-width: 0;
  }
  section {
    display: flex;
    flex-direction: column;
    gap: 6px;
    min-width: 0;
  }
  .row {
    display: grid;
    align-items: center;
    column-gap: 8px;
    padding: 4px 6px 4px 8px;
    border-left: 3px solid var(--tone);
    border-radius: var(--radius);
    background: var(--body-deep);
  }
  .row.sel {
    background: color-mix(in srgb, var(--tone) 16%, var(--body-deep));
  }
  .bar {
    min-width: 0;
  }
  .tag {
    background: transparent;
    border: 0;
    cursor: pointer;
    font-family: var(--font-caps);
    color: var(--text);
    display: flex;
    gap: 6px;
    align-items: center;
    padding: 0;
  }
  .tag:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
  i {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--c, var(--tone, currentColor));
    flex: none;
  }
</style>
