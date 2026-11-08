<script lang="ts">
  import { KeyMapH, KeyRangeBar, type MapLane } from '../kit';
  import { TONE_BLOCKS, type BlockName } from '../model/catalog';
  import { blockTitle } from '../model/labels';
  import { useStore } from './context';
  import { focusToneEditor } from './focusTone';
  import ParamControl from './ParamControl.svelte';
  import { resetBlock } from './toolActions';
  import { resetChanges } from '../model/tools';

  interface Props {
    colors: readonly string[];
    selected: number;
    onselect: (index: number) => void;
  }

  let { colors, selected, onselect }: Props = $props();
  const store = useStore();
  let root: HTMLDivElement | undefined = $state();

  $effect(() => {
    const i = selected;
    const signal = { cancelled: false };
    void focusToneEditor(root, i, signal);
    return () => {
      signal.cancelled = true;
    };
  });

  const pick = (i: number): void => {
    if (selected !== i) onselect(i);
  };

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

  const KEY_IDS = ['keyboard_range_lower', 'keyboard_range_upper'] as const;
  const VEL_IDS = ['velocity_range_lower', 'velocity_range_upper', 'velocity_cross_fade'] as const;
  const resetKeys = (i: number): void => resetBlock(store, TONE_BLOCKS[i] as BlockName, `${blockTitle(TONE_BLOCKS[i] as BlockName)} key range`, KEY_IDS);
  const resetVel = (i: number): void => resetBlock(store, TONE_BLOCKS[i] as BlockName, `${blockTitle(TONE_BLOCKS[i] as BlockName)} velocity`, VEL_IDS);
  const resetAll = (): void =>
    store.applyMany(
      TONE_BLOCKS.flatMap((b) => resetChanges(store.catalog, store.kind, b, [...KEY_IDS, ...VEL_IDS])),
      'Reset all zones',
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

<div class="zones" bind:this={root}>
  <section>
    <div class="cap">
      <span class="caps">Key range · which keys each tone plays</span>
      <button type="button" class="all" onclick={resetAll} title="Put every Tone's key range, velocity range and cross-fade back at neutral (one undo step)">Reset all zones</button>
    </div>
    <KeyMapH
      {lanes}
      gutterWidth={GUTTER}
      context="Zones"
      keyboardHeight={64}
      laneHeight={26}
      onselect={pick}
      onbegin={(i) => store.begin(`${blockTitle(TONE_BLOCKS[i] as BlockName)} · Key range`)}
      onend={() => store.end()}
      onrange={(i, r) => setRange(TONE_BLOCKS[i] as BlockName, 'keyboard_range', r)}
      onreset={resetKeys}
    >
      {#snippet gutter(l)}
        <button type="button" class="tag" onclick={() => pick(l.id)} aria-pressed={selected === l.id}><i></i>{l.name}</button>
        <button type="button" class="rs" title="Reset {l.name} key range to the whole keyboard (double-click the lane does the same)" onclick={() => resetKeys(l.id)}>↺</button>
      {/snippet}
    </KeyMapH>
  </section>

  <section>
    <span class="caps">Velocity range and cross-fade · how hard each tone is played</span>
    {#each TONE_BLOCKS as block, i (block)}
      {@const color = colorOf(i)}
      <!-- svelte-ignore a11y_no_static_element_interactions -->
      <div
        class="row"
        class:sel={selected === i}
        style:--tone={color}
        style:grid-template-columns="{GUTTER}px minmax(0, 1fr) auto"
        data-tone-focus={i}
        onpointerdowncapture={() => pick(i)}
      >
        <button type="button" class="tag" onclick={() => pick(i)} aria-pressed={selected === i}><i></i>{blockTitle(block)}</button>
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
        <div class="end">
          <ParamControl {block} id="velocity_cross_fade" label="X-fade" size={20} {color} />
          <button type="button" class="rs" title="Reset {blockTitle(block)} velocity range and cross-fade" onclick={() => resetVel(i)}>↺</button>
        </div>
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
    background: color-mix(in srgb, var(--tone) 26%, var(--body-deep));
  }
  .row:not(.sel) {
    opacity: 0.8;
  }
  .row.sel .tag {
    font-weight: 700;
  }
  .bar {
    min-width: 0;
  }
  .cap {
    display: flex;
    align-items: center;
    justify-content: space-between;
  }
  .end {
    display: flex;
    align-items: center;
    gap: 6px;
  }
  .rs,
  .all {
    background: var(--well-deep);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    color: var(--text-dim);
    cursor: pointer;
    font: inherit;
  }
  .rs {
    margin-left: auto;
    padding: 0 5px;
    line-height: 16px;
  }
  .all {
    padding: 1px 8px;
    font-size: 11px;
  }
  .rs:hover,
  .all:hover {
    background: var(--well);
    color: var(--text);
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
    background-image: var(--focus-fill);
  }
  i {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--c, var(--tone, currentColor));
    flex: none;
  }
</style>
