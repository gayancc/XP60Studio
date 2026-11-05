<script lang="ts">
  import { Dropdown } from '../kit';
  import type { BlockName } from '../model/catalog';
  import { EFX_TYPES, efxTypeLabel } from '../model/efx';
  import { useStore } from './context';

  // The EFX type as a list of its 40 names ("19 TRIPLE-TAP-DELAY") instead of a number.
  interface Props {
    block: BlockName;
    id: string;
  }
  let { block, id }: Props = $props();
  const store = useStore();
  const options = EFX_TYPES.map(efxTypeLabel);
</script>

<Dropdown
  {options}
  selected={store.get(block, id)}
  label="Type"
  name="EFX type"
  context="Effects"
  width={196}
  onchange={(i) => {
    store.begin('Effects · Type');
    store.set(block, id, i);
    store.end();
  }}
/>
