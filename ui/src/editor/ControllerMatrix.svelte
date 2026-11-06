<script lang="ts">
  import { GroupFrame } from '../kit';
  import type { BlockName } from '../model/catalog';
  import ParamControl from './ParamControl.svelte';

  interface Props {
    block: BlockName;
    color: string;
  }

  let { block, color }: Props = $props();

  const switches = ['redamper_control_switch', 'volume_control_switch', 'hold_1_control_switch', 'bender_control_switch'];
  const controllers = [1, 2, 3];
  const slots = [1, 2, 3, 4];
</script>

<GroupFrame title="Controllers" {color}>
  <div class="switches">
    <div class="toggles">
      {#each switches as id (id)}<ParamControl {block} {id} {color} />{/each}
    </div>
    <ParamControl {block} id="pan_control_switch" {color} width={120} />
  </div>
  <table class="matrix" aria-label="Controller routing">
    <thead>
      <tr>
        <th scope="col" rowspan="2" class="corner"></th>
        {#each slots as s (s)}<th scope="colgroup" colspan="2" class="slot">Slot {s}</th>{/each}
      </tr>
      <tr>
        {#each slots as s (s)}
          <th scope="col" class="sub dest">Dest</th>
          <th scope="col" class="sub depth">Depth</th>
        {/each}
      </tr>
    </thead>
    <tbody>
      {#each controllers as c (c)}
        <tr>
          <th scope="row" class="caps rowhead">Controller {c}</th>
          {#each slots as s (s)}
            <td class="dest"><ParamControl {block} id="controller_{c}_destination_{s}" label="" {color} width={66} /></td>
            <td class="depth"><ParamControl {block} id="controller_{c}_depth_{s}" label="" size={30} {color} /></td>
          {/each}
        </tr>
      {/each}
    </tbody>
  </table>
</GroupFrame>

<style>
  .switches {
    display: flex;
    flex-wrap: wrap;
    align-items: flex-end;
    justify-content: space-between;
    gap: 8px 24px;
    width: 100%;
    padding-bottom: 8px;
    border-bottom: 1px solid var(--edge);
  }
  .toggles {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 6px 18px;
  }
  /* One row per controller, one band per slot: a "Dest" list and its "Depth" dial share a row, and the captions are said once in the header instead of on every cell. */
  .matrix {
    border-collapse: collapse;
    width: 100%;
    margin-top: 4px;
  }
  th,
  td {
    vertical-align: middle;
    padding: 0;
  }
  .corner {
    width: 1%;
  }
  .slot {
    padding: 3px 0 2px 8px;
    text-align: left;
    font: 600 10px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text);
    border-left: 1px solid var(--edge);
  }
  .sub {
    padding: 0 0 4px;
    text-align: left;
    font: 600 9px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-faint);
  }
  .sub.dest {
    padding-left: 8px;
    border-left: 1px solid var(--edge);
  }
  .sub.depth {
    text-align: center;
  }
  .rowhead {
    padding: 0 12px 0 2px;
    text-align: left;
    white-space: nowrap;
    font-weight: normal;
    color: var(--text-dim);
  }
  tbody tr:nth-child(odd) {
    background: color-mix(in srgb, var(--text) 4%, transparent);
  }
  td {
    height: 40px;
  }
  td.dest {
    padding-left: 8px;
    border-left: 1px solid var(--edge);
  }
  td.depth {
    text-align: center;
    padding-right: 2px;
  }
</style>
