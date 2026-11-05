<script lang="ts">
  import RoutingDiagram from '../kit/RoutingDiagram.svelte';
  import { EFX_TYPES, efxTypeLabel } from '../model/efx';
  import { routingOf, type RoutingInput, type ToneRouting } from '../model/routing';
  import { TONE_BLOCKS } from '../model/catalog';
  import { useStore } from './context';
  import ParamControl from './ParamControl.svelte';

  // Effects routing, the way the manual teaches it: first decide where each Tone's sound goes and how much is sent (Output Assign and the three send levels of every
  // Tone), then see the whole path. It is drawn from the settings, so a send that does nothing is dimmed, and its reason is in the hint bar.
  interface Props {
    /** Show the Effects tab (where the EFX, chorus and reverb themselves are set). */
    onjump?: (node: 'efx' | 'chorus' | 'reverb') => void;
  }
  let { onjump }: Props = $props();
  const store = useStore();
  const colors = ['var(--tone-1)', 'var(--tone-2)', 'var(--tone-3)', 'var(--tone-4)'] as const;

  const tone = (n: number): ToneRouting => {
    const b = TONE_BLOCKS[n] as (typeof TONE_BLOCKS)[number];
    return {
      assign: store.get(b, 'output_assign'),
      mixEfxSend: store.get(b, 'mix_efx_send_level'),
      chorusSend: store.get(b, 'chorus_send_level'),
      reverbSend: store.get(b, 'reverb_send_level'),
    };
  };
  const input = $derived<RoutingInput>({
    tones: [tone(0), tone(1), tone(2), tone(3)],
    structure: [store.get('common', 'structure_type_1_2'), store.get('common', 'structure_type_3_4')],
    efx: {
      assign: store.get('common', 'efx_output_assign'),
      mixSend: store.get('common', 'efx_mix_out_send_level'),
      chorusSend: store.get('common', 'efx_chorus_send_level'),
      reverbSend: store.get('common', 'efx_reverb_send_level'),
    },
    chorus: { level: store.get('common', 'chorus_level'), output: store.get('common', 'chorus_output') },
    reverb: { level: store.get('common', 'reverb_level') },
  });
  const routing = $derived(routingOf(input));
  const labels = $derived({
    efx: efxTypeLabel(EFX_TYPES[store.get('common', 'efx_type')] ?? (EFX_TYPES[0] as (typeof EFX_TYPES)[number])),
    chorus: `${store.formatValue('common', 'chorus_output', store.get('common', 'chorus_output'))} · ${store.get('common', 'chorus_level')}`,
    reverb: `${store.formatValue('common', 'reverb_type', store.get('common', 'reverb_type'))} · ${store.get('common', 'reverb_level')}`,
  });
  const inertOf = (i: number, id: string): string | undefined => routing.inert.find((x) => x.tone === i && x.id === id)?.why;
</script>

<div class="routing">
  <div class="rows">
    {#each [0, 1, 2, 3] as i (i)}
      {@const b = TONE_BLOCKS[i] as (typeof TONE_BLOCKS)[number]}
      {@const ignored = routing.ignoredTones.has(`tone${i + 1}` as 'tone1')}
      <div class="row" class:ignored style:--tone={colors[i]}>
        <span class="chip"><i></i>Tone {i + 1}</span>
        <ParamControl block={b} id="output_assign" label="Output" />
        <ParamControl block={b} id="mix_efx_send_level" label="Mix/EFX" size={30} color={colors[i]} />
        <ParamControl block={b} id="chorus_send_level" label="Chorus" size={30} color={colors[i]} inert={inertOf(i, 'chorus_send_level')} />
        <ParamControl block={b} id="reverb_send_level" label="Reverb" size={30} color={colors[i]} inert={inertOf(i, 'reverb_send_level')} />
      </div>
    {/each}
  </div>
  <RoutingDiagram {routing} {labels} {onjump} />
</div>

<style>
  .routing {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
  .rows {
    display: flex;
    flex-direction: column;
    gap: 3px;
  }
  .row {
    display: flex;
    align-items: center;
    gap: 22px;
    padding: 3px 10px;
    border-radius: var(--radius);
    background: color-mix(in srgb, var(--tone) 9%, var(--body));
  }
  .row.ignored {
    opacity: 0.45;
  }
  .chip {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    width: 64px;
    font: 600 11px var(--font-caps);
    letter-spacing: 0.06em;
    text-transform: uppercase;
  }
  .chip i {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--tone);
  }
</style>
