<script lang="ts">
  import { Dropdown } from '../kit';
  import { bankNote, BANK_TITLES, EXPANSION_NOTE, FACTORY_BANKS, factoryBank, expansionBoards, isFixedBank, type FactoryBank } from '../model/factory';

  // Roland's own lists of the factory sounds: names only, for finding out what a number is called. The sound data itself is in the
  // instrument; read it with the Bank workspace.
  let bank = $state<FactoryBank>('USER');
  let find = $state('');

  const rows = $derived(
    factoryBank(bank)
      .map((name, i) => ({ number: i + 1, name }))
      .filter((r) => r.name.trim() !== '' && (find === '' || r.name.toLowerCase().includes(find.toLowerCase()))),
  );
</script>

<div class="fp">
  <div class="row">
    <Dropdown
      options={FACTORY_BANKS.map((b) => BANK_TITLES[b])}
      selected={FACTORY_BANKS.indexOf(bank)}
      name="Factory bank"
      context="Library > Factory lists"
      width={176}
      onchange={(i) => (bank = FACTORY_BANKS[i] ?? 'USER')}
    />
    <input type="text" placeholder="Find" aria-label="Find in the factory list" bind:value={find} />
  </div>
  <p class="note" class:fixed={isFixedBank(bank)}><b>{isFixedBank(bank) ? 'Fixed' : 'Yours to change'}</b> · {bankNote(bank)}</p>
  <ol aria-label="{BANK_TITLES[bank]} patch names">
    {#each rows as r (r.number)}
      <li><span class="n">{String(r.number).padStart(3, '0')}</span>{r.name}</li>
    {:else}
      <li class="dim">No name matches.</li>
    {/each}
  </ol>
  <p class="dim">{EXPANSION_NOTE} {expansionBoards.length} boards are known; wave names are held for {expansionBoards.filter((b) => b.waves).length} of them (Roland publishes no list for Dance, House or the promotional boards).</p>
</div>

<style>
  .fp {
    display: flex;
    flex-direction: column;
    gap: 6px;
    flex: 1;
    min-height: 0;
  }
  .row {
    display: flex;
    gap: 6px;
    align-items: flex-end;
  }
  input {
    flex: 1;
    min-width: 0;
    background: var(--track);
    color: var(--text);
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    padding: 4px 6px;
  }
  input:focus {
    outline: none;
    border-color: var(--accent);
  }
  ol {
    margin: 0;
    padding: 0;
    list-style: none;
    flex: 1;
    min-height: 60px;
    overflow-y: auto;
    border: 1px solid var(--edge);
    border-radius: var(--radius);
    background: var(--body-deep);
    font: 11px var(--font-mono);
  }
  li {
    padding: 2px 8px;
    white-space: nowrap;
  }
  li:nth-child(even) {
    background: color-mix(in srgb, var(--track) 40%, transparent);
  }
  .n {
    color: var(--text-faint);
    margin-right: 10px;
  }
  .note {
    margin: 0;
    color: var(--text-dim);
    font-size: 10.5px;
    line-height: 1.35;
  }
  .note b {
    color: var(--accent);
    font-weight: 600;
  }
  .note.fixed b {
    color: var(--text);
  }
  .dim {
    margin: 0;
    color: var(--text-dim);
    font-size: 10.5px;
  }
</style>
