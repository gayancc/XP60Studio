<script lang="ts">
  import { TONE_BLOCKS } from '../model/catalog';
  import { summarisePatch } from '../model/overview';
  import { useStore } from './context';

  interface Props {
    colors: readonly string[];
    selected: number;
    onselect: (index: number) => void;
  }

  let { colors, selected, onselect }: Props = $props();
  const store = useStore();
  // Reading the store inside the derived keeps the summary in step with every edit.
  const s = $derived.by(() => {
    void store.rev;
    return summarisePatch(store);
  });
</script>

<div class="overview">
  <div class="head">
    <b>{s.name}</b>
    <span class="dim">Level {s.level}</span>
  </div>
  <p class="dim">{s.structure}</p>
  <table aria-label="Tones at a glance">
    <thead>
      <tr><th scope="col"></th><th scope="col" class="caps">Wave</th><th scope="col" class="caps">Level</th><th scope="col" class="caps">Keys</th><th scope="col" class="caps">Velocity</th></tr>
    </thead>
    <tbody>
      {#each s.tones as t, i (t.number)}
        <tr class:off={!t.on} class:sel={selected === i} style:--tone={colors[i] ?? 'var(--accent)'}>
          <th scope="row">
            <button type="button" onclick={() => onselect(i)} aria-pressed={selected === i} aria-label="Edit Tone {t.number}, {t.on ? 'on' : 'off'}">
              <i></i>{t.number}
            </button>
          </th>
          <td>{t.wave}</td>
          <td>{t.level}</td>
          <td>{t.keys}</td>
          <td>{t.velocity}</td>
        </tr>
      {/each}
    </tbody>
  </table>
  <dl>
    {#each s.effects as e (e.label)}
      <dt class="caps">{e.label}</dt>
      <dd>{e.value}</dd>
    {/each}
  </dl>
  <p class="dim">{TONE_BLOCKS.length} Tones; a dimmed row is switched off.</p>
</div>

<style>
  .overview {
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .head {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
  }
  .head b {
    font: 600 14px var(--font-mono);
    color: var(--accent);
  }
  .dim {
    color: var(--text-dim);
    margin: 0;
    font-size: 11px;
  }
  table {
    width: 100%;
    border-collapse: collapse;
  }
  th,
  td {
    text-align: left;
    padding: 3px 4px;
    font-weight: normal;
    font-size: 11px;
    white-space: nowrap;
  }
  tbody tr {
    border-left: 3px solid var(--tone);
    background: var(--body-deep);
  }
  tbody tr.sel {
    background: color-mix(in srgb, var(--tone) 16%, var(--body-deep));
  }
  tbody tr.off td {
    opacity: 0.4;
  }
  button {
    display: flex;
    align-items: center;
    gap: 5px;
    background: transparent;
    border: 0;
    cursor: pointer;
    font-family: var(--font-mono);
  }
  button:focus-visible {
    outline: none;
    box-shadow: var(--focus);
  }
  i {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--tone);
  }
  dl {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 2px 10px;
    margin: 0;
  }
  dd {
    margin: 0;
    font-size: 11px;
  }
</style>
