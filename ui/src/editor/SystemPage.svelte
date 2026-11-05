<script lang="ts">
  import { GroupFrame } from '../kit';
  import type { SystemSection } from '../model/layout';
  import { SYSTEM_LABELS, SYSTEM_PAGES } from '../model/systemLayout';
  import ParamControl from './ParamControl.svelte';

  interface Props {
    section: SystemSection;
  }

  let { section }: Props = $props();
  const blocks = $derived(SYSTEM_PAGES[section]);
</script>

<div class="page">
  {#each blocks as b (b.title)}
    {#if b.kind === 'group'}
      <GroupFrame title={b.title}>
        <div class="controls" class:switches={b.switches}>
          {#each b.ids as id (id)}<ParamControl block="common" {id} label={SYSTEM_LABELS[id]} />{/each}
        </div>
      </GroupFrame>
    {:else}
      <GroupFrame title={b.title}>
        <table class="grid" aria-label={b.title} style:--cols={b.columns.length}>
          <thead>
            <tr>
              <th scope="col"></th>
              {#each b.columns as c (c)}<th scope="col" class="caps">{c}</th>{/each}
            </tr>
          </thead>
          <tbody>
            {#each b.rows as r (r.label)}
              <tr>
                <th scope="row">{r.label}</th>
                {#each r.cells as id, i (i)}
                  <td>{#if id}<ParamControl block="common" {id} label="" width={120} />{:else}<span class="none" aria-label="not available">—</span>{/if}</td>
                {/each}
              </tr>
            {/each}
          </tbody>
        </table>
      </GroupFrame>
    {/if}
  {/each}
</div>

<style>
  .page {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 10px;
    align-items: start;
  }
  /* A table fills the row: it is the whole point of the tab. */
  .page :global(fieldset:has(table)) {
    grid-column: 1 / -1;
  }
  .controls {
    display: flex;
    flex-wrap: wrap;
    gap: 8px 16px;
    align-items: flex-end;
  }
  .controls.switches {
    display: grid;
    grid-template-columns: repeat(2, minmax(210px, 1fr));
    gap: 10px 28px;
    align-items: center;
  }
  .grid {
    width: 100%;
    max-width: 760px;
    border-collapse: collapse;
    table-layout: fixed;
  }
  th,
  td {
    padding: 3px 6px;
    text-align: center;
    vertical-align: middle;
    border-bottom: 1px solid var(--edge);
    height: 40px;
  }
  tbody tr:last-child th,
  tbody tr:last-child td {
    border-bottom: 0;
  }
  thead th {
    height: 22px;
    border-bottom: 1px solid var(--edge-light);
  }
  /* The first column names the row and stays narrow; the rest share what is left equally. */
  th[scope='row'] {
    width: 96px;
    text-align: left;
    font-weight: normal;
    font-family: var(--font-caps);
    color: var(--text);
  }
  tbody tr:hover {
    background: rgba(255, 255, 255, 0.03);
  }
  td :global(.pc) {
    justify-content: center;
  }
  .none {
    color: var(--text-faint);
  }
</style>
