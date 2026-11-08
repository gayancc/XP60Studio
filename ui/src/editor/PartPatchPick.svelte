<script lang="ts">
  import { Dropdown } from '../kit';
  import type { BlockName } from '../model/catalog';
  import { boardName, PATCH_GROUPS, presetPatchName } from '../model/factory';
  import { expansions } from '../model/expansions.svelte';
  import { useStore } from './context';

  // Choosing a Part's Patch where the Part is: the group (USER, the Preset banks, the installed expansion boards) and then the Patch, named from
  // Roland's list for the fixed banks. User memory is the owner's own, so it shows the number only; an expansion board's patches are chosen by number.
  interface Props {
    block: BlockName;
    partName: string;
  }
  let { block, partName }: Props = $props();
  const store = useStore();

  interface Option {
    key: string;
    label: string;
    type: number;
    id: number;
  }

  const type = $derived(store.get(block, 'patch_group_type'));
  const gid = $derived(store.get(block, 'patch_group_id'));
  const raw = $derived(store.get(block, 'patch_number'));

  const options = $derived.by((): Option[] => {
    const list: Option[] = PATCH_GROUPS.map((g) => ({ key: g.key, label: g.label, type: g.type, id: g.id }));
    for (const n of expansions.installed) list.push({ key: `E${n}`, label: boardName(n) ?? `Board ${n}`, type: 2, id: n });
    // The Part may already point somewhere the lists above do not offer (a board not marked installed, a PCM card): keep it visible.
    if (!list.some((o) => o.type === type && o.id === gid)) {
      list.push({ key: 'cur', label: type === 2 ? `${boardName(gid) ?? `Board ${gid}`} (not marked installed)` : type === 0 && gid === 0 ? 'Not set' : `Group ${type}/${gid}`, type, id: gid });
    }
    return list;
  });
  const selected = $derived(Math.max(0, options.findIndex((o) => o.type === type && o.id === gid)));

  /** Patch numbers 1 to 128 in User and the Preset banks, 1 to 255 on an expansion board. */
  const count = $derived(type === 2 ? 255 : 128);
  const numbers = $derived(
    Array.from({ length: Math.max(count, raw + 1) }, (_, i) => {
      const n = String(i + 1).padStart(3, '0');
      const name = presetPatchName(type, gid, i);
      return name ? `${n} ${name}` : n;
    }),
  );

  function chooseGroup(i: number): void {
    const o = options[i];
    if (!o) return;
    // Type and id together: one undo step, one live update. Sending type alone can point the Part at a missing bank.
    const changes = [
      { block, id: 'patch_group_type', value: o.type },
      { block, id: 'patch_group_id', value: o.id },
    ];
    // User memory and the Preset banks hold 128 Patches; keep the number inside the new group.
    if (o.type === 0 && raw > 127) changes.push({ block, id: 'patch_number', value: 127 });
    store.applyMany(changes, `${partName} · Patch group`);
  }

  const chooseNumber = (i: number): void => {
    store.begin(`${partName} · Patch`);
    store.set(block, 'patch_number', i);
    store.end();
  };

</script>

<span class="pp">
  <Dropdown options={options.map((o) => o.label)} {selected} name="{partName} patch group" context="{partName} > Patch" width={92} onchange={chooseGroup} />
  <Dropdown options={numbers} selected={raw} name="{partName} patch" context="{partName} > Patch" width={150} onchange={chooseNumber} />
</span>

<style>
  .pp {
    display: flex;
    gap: 4px;
    min-width: 0;
  }
</style>
