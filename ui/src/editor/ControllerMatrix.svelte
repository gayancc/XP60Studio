<script lang="ts">
  import { GroupFrame } from '../kit';
  import type { BlockName } from '../model/catalog';
  import ParamControl from './ParamControl.svelte';

  interface Props {
    block: BlockName;
    color: string;
  }

  let { block, color }: Props = $props();

  const switches = ['redamper_control_switch', 'volume_control_switch', 'hold_1_control_switch', 'bender_control_switch', 'pan_control_switch'];
  const controllers = [1, 2, 3];
  const slots = [1, 2, 3, 4];
</script>

<GroupFrame title="Controllers" {color}>
  <div class="switches">
    {#each switches as id (id)}<ParamControl {block} {id} {color} />{/each}
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
    gap: 6px 14px;
    flex-wrap: wrap;
    align-items: flex-end;
    width: 100%;
  }
  .matrix {
    border-collapse: collapse;
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
