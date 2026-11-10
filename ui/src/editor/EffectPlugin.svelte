<script lang="ts">
  import { Dropdown, Knob, Segmented, StepSelect } from '../kit';
  import { EFX_TYPES, efxChoices, efxDefault, efxDefaults, efxShortName, efxTypeLabel, efxUnitNote, efxValueText, type EfxParam } from '../model/efx';
  import { efxBlurb, EFX_FAMILIES, familyOf, masterOf, sectionsOf } from '../model/efxFamilies';
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
  const master = $derived(masterOf(type));
  /** The sections grouped by the row they sit in, top row first. */
  const rows = $derived.by(() => {
    const by = new Map<number, typeof sections>();
    for (const sec of sections) by.set(sec.row ?? 0, [...(by.get(sec.row ?? 0) ?? []), sec]);
    return [...by.entries()].sort((a, b) => a[0] - b[0]).map(([, v]) => v);
  });
  /** Short names for the routing and control settings, so each fits one line under its knob. */
  const EXTRA_LABELS: Record<string, string> = {
    efx_output_assign: 'Output',
    efx_mix_out_send_level: 'Out level',
    efx_chorus_send_level: 'To chorus',
    efx_reverb_send_level: 'To reverb',
    efx_control_source_1: 'Control 1',
    efx_control_depth_1: 'Depth 1',
    efx_control_source_2: 'Control 2',
    efx_control_depth_2: 'Depth 2',
    efx_control_hold_peak: 'Hold / peak',
  };
  /** Where the effect's sound goes, then what moves it: the way an engineer reads a channel's output section and its controller section. */
  const ROUTING_GROUPS = [
    { title: 'Effect output', ids: ['efx_output_assign', 'efx_mix_out_send_level', 'efx_chorus_send_level', 'efx_reverb_send_level'] },
    { title: 'Controllers', ids: ['efx_control_source_1', 'efx_control_depth_1', 'efx_control_source_2', 'efx_control_depth_2', 'efx_control_hold_peak'] },
  ] as const;
  const SKETCH_W = 164;
  let sketchH = $state(0);
  const extras = $derived(ids.filter((id) => id !== 'efx_type' && !id.startsWith('efx_parameter_')));

  function pick(n: number): void {
    if (n === stored) return;
    // Type + its twelve defaults in one undo step and one whole-patch live send (type alone leaves the old slot meanings on the XP-60).
    store.applyMany(
      [
        { block: 'common', id: 'efx_type', value: n },
        ...efxDefaults(n).map((value, i) => ({ block: 'common' as const, id: `efx_parameter_${i + 1}`, value })),
      ],
      'Effects · Type',
    );
  }

  const put = (p: EfxParam, v: number): void => store.set('common', `efx_parameter_${p.index}`, v);
  const bipolar = (name: string): boolean => /\bGain$|Feedback Level$|^Fine Pitch|\bPan( [AB])?$/.test(name);
</script>

<div class="plugin" style:--fx={family.color}>
  <header>
    <StepSelect
      names={TYPE_NAMES}
      groups={TYPE_GROUPS}
      selected={stored}
      name="EFX type"
      prevLabel="Previous EFX type"
      nextLabel="Next EFX type"
      context="Effects"
      width={280}
      onselect={(i) => pick(i)}
    />
    <p class="blurb"><b>{family.label}</b> · {efxBlurb(type)}</p>
  </header>

  <div class="body">
    <div class="left">
      <div class="sketch" bind:clientHeight={sketchH}><EfxDisplay {type} {values} width={SKETCH_W} height={Math.max(sketchH, 84)} /></div>
      <div class="master" aria-label="Master controls">
        {#each master as p (p.index)}
          {#if p.raw}
            {@const r = p.raw}
            <Knob
              value={values[p.index - 1] ?? r[0]}
              min={r[0]}
              max={r[1]}
              size={30}
              color="var(--fx)"
              defaultValue={efxDefault(stored, p.index)}
              label={p.name === 'Output Pan' ? 'Pan' : /Level$/.test(p.name) && !/Mix/.test(p.name) ? 'Level' : 'Dry / wet'}
              name={p.name}
              context="Effects > Parameter {p.index}"
              format={(x) => efxValueText(p.name, r, x)}
              onchange={(x) => put(p, x)}
              onbegin={() => store.begin(`Effects · ${p.name}`)}
              onend={() => store.end()}
            />
          {/if}
        {/each}
      </div>
    </div>
    <div class="sections">
     {#each rows as line, li (li)}
      <div class="line">
      {#each line as s (s.title)}
        <section>
          <h4>{s.title}</h4>
          <div class="row" class:strip={s.strip} class:two={s.strip && s.params.length >= 4}>
            {#each s.params as p, k (p.index)}
              {#if p.raw}
                {@const r = p.raw}
                {@const choices = efxChoices(p.name, r)}
                {@const v = values[p.index - 1] ?? r[0]}
                {#if choices && choices.length <= 2}
                  <div class="choice">
                    <span class="caps">{s.labels?.[k] ?? efxShortName(p.name)}</span>
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
                    label={s.labels?.[k] ?? efxShortName(p.name)}
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
                    defaultValue={efxDefault(stored, p.index)}
                    label={s.labels?.[k] ?? efxShortName(p.name)}
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
     {/each}
    {#if extras.length}
      <div class="routing">
        {#each ROUTING_GROUPS as grp (grp.title)}
          {@const present = grp.ids.filter((id) => extras.includes(id))}
          {#if present.length}
            <section>
              <h4>{grp.title}</h4>
              <div class="row">{#each present as id (id)}<ParamControl block="common" {id} size={20} label={EXTRA_LABELS[id]} />{/each}</div>
            </section>
          {/if}
        {/each}
      </div>
    {/if}
    </div>
  </div>
</div>

<style>
  .plugin {
    display: flex;
    flex-direction: column;
    /* Fill the group it sits in, and never be wider than it: its contents wrap instead. */
    width: 100%;
    min-width: 0;
    gap: 4px;
  }
  header {
    --step-accent: var(--fx);
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
  .body {
    display: flex;
    gap: 14px;
    align-items: flex-start;
  }
  .left {
    flex: none;
    width: 164px;
    align-self: stretch;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .sketch {
    flex: 1;
    position: relative;
    min-height: 84px;
  }
  .master {
    display: flex;
    justify-content: center;
    gap: 8px;
  }
  .sketch :global(svg) {
    position: absolute;
    inset: 0;
  }
  .sections {
    display: flex;
    flex-direction: column;
    gap: 8px;
    min-width: 0;
    flex: 1;
  }
  .line {
    display: flex;
    flex-wrap: wrap;
    align-items: flex-start;
    gap: 8px 14px;
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
    gap: 3px 6px;
    align-items: flex-end;
  }
  /* A channel strip: one control above the other, as a band, tap or voice is laid out on a mixer. */
  .row.strip {
    flex-direction: column;
    align-items: center;
    gap: 6px;
  }
  section {
    display: flex;
    flex-direction: column;
  }
  /* Four or more in a strip would stand too tall: two columns. */
  .row.strip.two {
    display: grid;
    grid-template-columns: repeat(2, auto);
    justify-items: center;
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
  .routing {
    display: flex;
    flex-wrap: wrap;
    align-items: flex-start;
    gap: 4px 18px;
  }
  .routing .row {
    gap: 0 8px;
    align-items: center;
  }
</style>
