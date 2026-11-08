# Gates: status-bar message queue

OWNS: ui/src/model/messages.svelte.ts, ui/src/model/messages.test.ts, ui/src/model/messageDiscipline.test.ts, ui/src/kit/HintBar.svelte, ui/src/kit/HintBar.test.ts, ui/src/kit/index.ts, ui/src/editor/Workspace.svelte, ui/e2e/**, ui/scripts/verify-status-bar-messages.mjs, GATES.md, CHANGELOG.md

Scope: Replace floating MessageStack overlays with a single bottom hint-bar status line that queues every app message and shows them one by one with display time scaled by queue size.

- [x] G1: No floating message stack overlays the main view
  CHECK: node ui/scripts/verify-status-bar-messages.mjs
  EXPECT: status-bar message queue verification passed
  EVIDENCE: automatic-evidence=v1; definition-sha256=2a56e749e8b169b7665da86bfb6b53e2368f8cd9eac10bb7265757d9edb0bce3; exit=0; EXPECT=matched; output-sha256=e7ac03b2d085eef34e125589da145304d78a0d849e0959658ab82889df2fbff0; output-bytes=45; shell=C:\WINDOWS\system32\cmd.exe; cwd=D:\Codes\XP60Studio; path=50ab7b923ef2/70 entries

- [x] G2: Message centre queues every kind and times each slot by queue size
  CHECK: npm test -- --run src/model/messages.test.ts
  EXPECT: Test Files  1 passed
  CWD: ui
  EVIDENCE: automatic-evidence=v1; definition-sha256=f5c7019f9b29b9a5c78e295be2dbfdc439ffa02a8d7f7cbbf460677f8c31dfc9; exit=0; EXPECT=matched; output-sha256=f646a12bd03eb2a6a7c0d47d5507795a2974b82d25b6d1a0e79d814a7cb8d988; output-bytes=280; shell=C:\WINDOWS\system32\cmd.exe; cwd=D:\Codes\XP60Studio\ui; path=50ab7b923ef2/70 entries

- [x] G3: Hint bar renders the active queued message for every kind
  CHECK: npm test -- --run src/kit/HintBar.test.ts src/model/messageDiscipline.test.ts
  EXPECT: Test Files  2 passed
  CWD: ui
  EVIDENCE: automatic-evidence=v1; definition-sha256=fe9d669ff79fdf1c1e707f1f6c192e7c7da64f8c52c5eeffa185d3d62cbc7c65; exit=0; EXPECT=matched; output-sha256=9bca02bbf3d9d9b7d4725e84a7f764b5291b065d591aedfd1ae37319995788fc; output-bytes=309; shell=C:\WINDOWS\system32\cmd.exe; cwd=D:\Codes\XP60Studio\ui; path=50ab7b923ef2/70 entries

- [x] G4: Full UI regression suite stays green
  CHECK: npm --prefix ui test
  EXPECT: Test Files
  EVIDENCE: automatic-evidence=v1; definition-sha256=dc8d3e1d59b156e60da5567b98580652b78778e0255298fc2fd39b7fd6baa16a; exit=0; EXPECT=matched; output-sha256=4ee314a61db50750130b5080e4e129e24388663ae727097a221ddf972b397607; output-bytes=542; shell=C:\WINDOWS\system32\cmd.exe; cwd=D:\Codes\XP60Studio; path=50ab7b923ef2/70 entries
