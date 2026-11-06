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
        <th scope="col"></th>
        {#each slots as s (s)}<th scope="col" class="caps">Slot {s}</th>{/each}
      </tr>
    </thead>
    <tbody>
      {#each controllers as c (c)}
        <tr>
          <th scope="row" class="caps">Controller {c}</th>
          {#each slots as s (s)}
            <td>
              <div class="cell">
                <ParamControl {block} id="controller_{c}_destination_{s}" label="Dest" {color} width={62} />
                <ParamControl {block} id="controller_{c}_depth_{s}" label="Depth" size={30} {color} />
              </div>
            </td>
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
  .matrix {
    border-collapse: collapse;
    width: 100%;
  }
  th.caps {
    white-space: nowrap;
    padding-right: 6px;
  }
  th {
    text-align: left;
    font-weight: normal;
    padding: 2px 6px 2px 0;
  }
  td {
    padding: 4px 4px 4px 0;
    vertical-align: bottom;
  }
  .cell {
    display: flex;
    align-items: flex-end;
    gap: 2px;
  }
</style>
