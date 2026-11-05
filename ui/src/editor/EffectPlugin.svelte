<script lang="ts">
  import { Dropdown, Knob, Segmented } from '../kit';
  import { EFX_TYPES, efxChoices, efxShortName, efxTypeLabel, efxUnitNote, efxValueText, type EfxParam } from '../model/efx';
  import { efxBlurb, EFX_FAMILIES, familyOf, sectionsOf } from '../model/efxFamilies';
  import { useStore } from './context';
  import EfxDisplay from './EfxDisplay.svelte';
  import ParamControl from './ParamControl.svelte';

  // The EFX unit drawn as one effect plug-in: a header with the effect's name and a browser of the nine families, a sketch of what the effect does, and its
  // parameters in labelled sections (a combination type shows one section per stage). Only the parameters the XP-60 gives the effect are drawn, each with its
  // own range. Values are stored exactly as before: efx_type and efx_parameter_1 to 12.
  interface Props {
    /** The ids of the effect section (the type, the twelve parameters and the routing and control settings). */
    ids: readonly string[];
  }
  let { ids }: Props = $props();
  const store = useStore();

  const TYPE_NAMES = EFX_TYPES.map(efxTypeLabel);
  const TYPE_GROUPS = EFX_FAMILIES.map((f) => ({ label: f.label, indices: f.types.map((n) => n - 1) }));

  const stored = $derived(store.get('common', 'efx_type'));
  const type = $derived(EFX_TYPES[stored] ?? (EFX_TYPES[0] as (typeof EFX_TYPES)[number]));
  const family = $derived(familyOf(type.type));
  const values = $derived(Array.from({ length: 12 }, (_, i) => store.get('common', `efx_parameter_${i + 1}`)));
  const sections = $derived(sectionsOf(type));
  const extras = $derived(ids.filter((id) => id !== 'efx_type' && !id.startsWith('efx_parameter_')));

  function pick(n: number): void {
    if (n === stored) return;
    store.begin('Effects · Type');
    store.set('common', 'efx_type', n);
    store.end();
  }

  const put = (p: EfxParam, v: number): void => store.set('common', `efx_parameter_${p.index}`, v);
  const bipolar = (name: string): boolean => /\bGain$|Feedback Level$|^Fine Pitch|\bPan( [AB])?$/.test(name);
</script>

<div class="plugin" style:--fx={family.color}>
  <header>
    <button type="button" class="step" aria-label="Previous EFX type" disabled={stored <= 0} onclick={() => pick(stored - 1)}>−</button>
    <Dropdown
      options={TYPE_NAMES}
      groups={TYPE_GROUPS}
      selected={stored}
      name="EFX type"
      context="Effects"
      width={250}
      onchange={(i) => pick(i)}
    />
    <button type="button" class="step" aria-label="Next EFX type" disabled={stored >= EFX_TYPES.length - 1} onclick={() => pick(stored + 1)}>+</button>
    <p class="blurb"><b>{family.label}</b> · {efxBlurb(type)}</p>
  </header>

  <div class="body">
    <EfxDisplay {type} {values} />
    <div class="sections">
      {#each sections as s (s.title)}
        <section>
          <h4>{s.title}</h4>
          <div class="row">
            {#each s.params as p (p.index)}
              {#if p.raw}
                {@const r = p.raw}
                {@const choices = efxChoices(p.name, r)}
                {@const v = values[p.index - 1] ?? r[0]}
                {#if choices && choices.length <= 4}
                  <div class="choice">
                    <span class="caps">{efxShortName(p.name)}</span>
                    <Segmented options={choices} selected={v} name={p.name} context="Effects > {p.name}" onchange={(i) => {
                      store.begin(`Effects · ${p.name}`);
                      put(p, i);
                      store.end();
                    }} />
                  </div>
                {:else if choices}
                  <Dropdown
                    options={choices}
                    selected={v}
                    label={efxShortName(p.name)}
                    name={p.name}
                    context="Effects > {p.name}"
                    width={96}
                    onchange={(i) => {
                      store.begin(`Effects · ${p.name}`);
                      put(p, i);
                      store.end();
                    }}
                  />
                {:else}
                  <Knob
                    value={v}
                    min={r[0]}
                    max={r[1]}
                    size={30}
                    color="var(--fx)"
                    origin={bipolar(p.name) ? 0.5 : null}
                    defaultValue={bipolar(p.name) ? Math.round((r[0] + r[1]) / 2) : r[0]}
                    label={efxShortName(p.name)}
                    name={efxUnitNote(p.name, r) ? `${p.name} (${efxUnitNote(p.name, r)})` : p.name}
                    context="Effects > Parameter {p.index}"
                    format={(x) => efxValueText(p.name, r, x)}
                    onchange={(x) => put(p, x)}
                    onbegin={() => store.begin(`Effects · ${p.name}`)}
                    onend={() => store.end()}
                  />
                {/if}
              {/if}
            {/each}
          </div>
        </section>
      {/each}
    </div>
  </div>


    {#if extras.length}
    <section class="routing">
      <h4>Routing and control</h4>
      <div class="row">{#each extras as id (id)}<ParamControl block="common" {id} size={20} />{/each}</div>
    </section>
    {/if}
</div>

<style>
  .plugin {
    display: flex;
    flex-direction: column;
    gap: 4px;
  }
  header {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 3px 8px;
    background: linear-gradient(var(--caption), var(--body));
    border-left: 3px solid var(--fx);
    border-radius: var(--radius);
  }
  .blurb {
    margin: 0 0 0 10px;
    min-width: 0;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    font-size: 12px;
    color: var(--text-dim);
  }
  .blurb b {
    color: var(--fx);
    font: 600 11px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
  }
  .step {
    width: 22px;
    align-self: stretch;
    border: 0;
    border-radius: var(--radius);
    background: var(--caption);
    color: var(--text-dim);
    font: 600 15px/1 var(--font-caps);
    cursor: pointer;
  }
  .step:hover:not(:disabled) {
    color: var(--fx);
    background: var(--well);
  }
  .step:disabled {
    opacity: 0.35;
    cursor: default;
  }
  .body {
    display: flex;
    gap: 14px;
    align-items: flex-start;
  }
  .sections {
    display: flex;
    flex-wrap: wrap;
    gap: 8px 18px;
    min-width: 0;
  }
  h4 {
    margin: 0 0 2px;
    font: 600 10px var(--font-caps);
    letter-spacing: 0.1em;
    text-transform: uppercase;
    color: var(--fx);
  }
  .row {
    display: flex;
    flex-wrap: wrap;
    gap: 3px 10px;
    align-items: flex-end;
  }
  .choice {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .caps {
    font: 600 10px var(--font-caps);
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-dim);
  }
  .routing .row {
    gap: 0 8px;
    align-items: center;
  }
</style>
