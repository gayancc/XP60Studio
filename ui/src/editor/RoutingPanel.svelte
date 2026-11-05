<script lang="ts">
  import RoutingDiagram from '../kit/RoutingDiagram.svelte';
  import { EFX_TYPES, efxTypeLabel } from '../model/efx';
  import { showHint, clearHint } from '../kit/hint.svelte';
  import { routingOf, type RoutingInput, type SendLine, type ToneRouting } from '../model/routing';
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
  /** The Tone being traced: the one the pointer is over in the list or the diagram. */
  let trace = $state<number | null>(null);
  const NAME = { efx: 'EFX', mix: 'MIX OUTPUT', dir: 'DIRECT OUT' } as const;
  const SHORT = { efx: 'EFX', mix: 'MIX', dir: 'DIR' } as const;
  const lineWhy = (what: string, l: SendLine): string =>
    l.state === 'on' ? `Sending ${l.level} of 127 to ${what}` : l.state === 'off' ? `Level 0: nothing is sent to ${what}` : l.state === 'ignored' ? 'Ignored: with Structure Type 2 to 10 this Tone is combined into the next one' : `No effect: ${l.why ?? ''}`;
  const inertOf = (i: number, id: string): string | undefined => routing.inert.find((x) => x.tone === i && x.id === id)?.why;
</script>

<div class="routing">
  <div class="rows">
    {#each [0, 1, 2, 3] as i (i)}
      {@const b = TONE_BLOCKS[i] as (typeof TONE_BLOCKS)[number]}
      {@const ignored = routing.ignoredTones.has(`tone${i + 1}` as 'tone1')}
      {@const route = routing.tones[i]}
      <div class="row" class:ignored style:--tone={colors[i]} onpointerenter={() => (trace = i)} onpointerleave={() => (trace = null)} role="group" aria-label="Tone {i + 1} route">
        <span class="chip"><i></i>Tone {i + 1}</span>
        <ParamControl block={b} id="output_assign" label="Output" />
        <ParamControl block={b} id="mix_efx_send_level" label="Mix/EFX" size={30} color={colors[i]} />
        <ParamControl block={b} id="chorus_send_level" label="Chorus" size={30} color={colors[i]} inert={inertOf(i, 'chorus_send_level')} />
        <ParamControl block={b} id="reverb_send_level" label="Reverb" size={30} color={colors[i]} inert={inertOf(i, 'reverb_send_level')} />
        <!-- the Tone's three send lines as the manual draws them (p.61-64): where the Mix/EFX line goes, then the chorus and reverb lines -->
        {#if route}
          <span class="route" aria-label="Where Tone {i + 1} sends its sound">
            {#each [{ k: 'main', what: route.main.to ? NAME[route.main.to] : 'nowhere', short: route.main.to ? SHORT[route.main.to] : '?', l: route.main as SendLine }, { k: 'chorus', what: 'CHORUS', short: 'CHO', l: route.chorus }, { k: 'reverb', what: 'REVERB', short: 'REV', l: route.reverb }] as c (c.k)}
              <span
                class="pill {c.l.state}"
                role="img"
                aria-label="{c.what}: {c.l.state}"
                onpointerenter={() => showHint({ context: 'Routing', name: `Tone ${i + 1} to ${c.what}`, value: lineWhy(c.what, c.l) })}
                onpointerleave={clearHint}>→ {c.short}{#if c.l.state === 'on'}<b>{c.l.level}</b>{/if}</span>
            {/each}
          </span>
        {/if}
      </div>
    {/each}
  </div>
  <RoutingDiagram {routing} {labels} {onjump} focus={trace} ontrace={(i) => (trace = i)} />
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
    gap: 12px;
    padding: 3px 10px;
    border-radius: var(--radius);
    background: color-mix(in srgb, var(--tone) 9%, var(--body));
  }
  .row.ignored {
    opacity: 0.45;
  }
  .route {
    display: inline-flex;
    gap: 5px;
    margin-left: auto;
  }
  .pill {
    padding: 1px 7px;
    border-radius: 9px;
    font: 600 9.5px var(--font-caps);
    letter-spacing: 0.06em;
    white-space: nowrap;
    background: var(--well-deep);
    color: var(--text-dim);
  }
  .pill b {
    margin-left: 5px;
    font-family: var(--font-mono);
  }
  .pill.on {
    background: color-mix(in srgb, var(--tone) 32%, var(--body));
    color: var(--text);
  }
  .pill.off,
  .pill.ignored {
    opacity: 0.45;
  }
  .pill.inert {
    text-decoration: line-through;
    opacity: 0.6;
  }
  .chip {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    flex: none;
    width: 58px;
    white-space: nowrap;
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
