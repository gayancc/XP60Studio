<script lang="ts">
  // Test support: the Bank workspace's Library tab on its own.
  import type { BankService } from '../model/bank.svelte';
  import type { LibraryService } from '../model/library.svelte';
  import BulkLibraryPanel from './BulkLibraryPanel.svelte';

  interface Props {
    bank: BankService;
    lib: LibraryService;
    onput?: (slot: number) => void;
  }
  let { bank, lib, onput }: Props = $props();
</script>

<BulkLibraryPanel {bank} {lib} deviceId={16} {onput} />
