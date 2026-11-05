<script lang="ts">
  import { Knob } from '../kit';
  import type { BlockName } from '../model/catalog';
  import { efxParam, efxShortName, efxValueText } from '../model/efx';
  import { useStore } from './context';

  // One of the twelve EFX parameter knobs, named and ranged for the effect type that is selected. A slot the type does not use is not drawn.
  interface Props {
    block: BlockName;
    /** The stored parameter id, `efx_parameter_<1..12>`. */
    id: string;
    size?: 20 | 30 | 38 | 64;
    color?: string;
  }
  let { block, id, size = 30, color = 'var(--accent)' }: Props = $props();
  const store = useStore();

  const index = $derived(Number(id.replace('efx_parameter_', '')));
  const param = $derived(efxParam(store.get(block, 'efx_type'), index));
  const value = $derived(store.get(block, id));
</script>

{#if param && param.raw}
  {@const range = param.raw}
  <Knob
    {value}
    min={range[0]}
    max={range[1]}
    {size}
    {color}
    origin={null}
    defaultValue={range[0]}
    label={efxShortName(param.name)}
    name={param.name}
    context="Effects > Parameter {index}"
    format={(v) => efxValueText(param.name, range, v)}
    onchange={(v) => store.set(block, id, v)}
    onbegin={() => store.begin(`Effects · ${param.name}`)}
    onend={() => store.end()}
  />
{/if}
