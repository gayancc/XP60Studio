<script lang="ts">
  import { GroupFrame } from '../kit';
  import type { BlockName } from '../model/catalog';
  import LfoGraph from './LfoGraph.svelte';
  import ParamControl from './ParamControl.svelte';

  // One LFO laid out in three tidy rows, then its preview: what the LFO is (Form, Key Trigger, Rate), how it sits and syncs (Offset, External Sync), and how it
  // comes in (Fade Mode with its Delay and Fade times). Rows share one baseline so nothing floats.
  interface Props {
    block: BlockName;
    lfo: 1 | 2;
    color: string;
  }

  let { block, lfo, color }: Props = $props();
  const id = (name: string): string => `lfo${lfo}_${name}`;
</script>

<GroupFrame title="LFO {lfo}" {color}>
  <div class="lfo">
    <div class="row">
      <ParamControl {block} id={id('waveform')} {color} width={120} />
      <ParamControl {block} id={id('key_trigger')} {color} />
      <span class="end"><ParamControl {block} id={id('rate')} {color} /></span>
    </div>
    <div class="row">
      <ParamControl {block} id={id('offset')} {color} />
      <ParamControl {block} id={id('external_sync')} {color} />
    </div>
    <div class="row">
      <ParamControl {block} id={id('fade_mode')} {color} />
      <span class="end knobs">
        <ParamControl {block} id={id('delay_time')} {color} />
        <ParamControl {block} id={id('fade_time')} {color} />
      </span>
    </div>
    <LfoGraph {block} {lfo} {color} />
  </div>
</GroupFrame>

<style>
  .lfo {
    display: flex;
    flex-direction: column;
    gap: 10px;
    width: 100%;
  }
  .row {
    display: flex;
    flex-wrap: wrap;
    align-items: flex-end;
    gap: 8px 16px;
  }
  .end {
    margin-left: auto;
    display: inline-flex;
  }
  .knobs {
    gap: 14px;
  }
</style>
