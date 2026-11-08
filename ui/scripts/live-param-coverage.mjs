// Diff patch catalog parameters against live UI evidence. Pure JSON — no TS imports.
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..', '..');
const raw = JSON.parse(readFileSync(join(root, 'data/xp60-parameters.json'), 'utf8'));
const common = raw.blocks.find((b) => b.id === 'patch_common');
const tone = raw.blocks.find((b) => b.id === 'patch_tone');

const evidenceFiles = [
  'live-params-all-ui.txt',
  'live-nonsiders-ui.txt',
  'live-params-ui.txt',
].map((n) => join(root, 'artifact/docs/xppatchstudio/hardware-evidence', n));
const evidenceText = evidenceFiles
  .map((f) => {
    try {
      return readFileSync(f, 'utf8');
    } catch {
      return '';
    }
  })
  .join('\n')
  .toLowerCase();

function titleCaseId(id) {
  return id
    .replace(/^efx_/, 'EFX ')
    .replace(/_/g, ' ')
    .replace(/\b\w/g, (c) => c.toUpperCase());
}

function covered(id, name) {
  const needles = [
    name,
    id,
    id.replace(/_/g, ' '),
    titleCaseId(id),
    name.replace(/\s*\([^)]*\)/g, ''),
  ]
    .filter(Boolean)
    .map((s) => s.toLowerCase());
  return needles.some((s) => s.length >= 3 && evidenceText.includes(s));
}

function isName(p) {
  return /_name_\d+$/.test(p.id) || p.display === 'character' || (typeof p.display === 'object' && p.display?.kind === 'character');
}

const commonParams = common.params.filter((p) => !isName(p));
const toneParams = tone.params.filter((p) => !isName(p));

const uncoveredCommon = commonParams.filter((p) => !covered(p.id, p.name)).map((p) => ({ id: p.id, name: p.name }));
const uncoveredTone = toneParams.filter((p) => !covered(p.id, p.name)).map((p) => ({ id: p.id, name: p.name }));
const coveredCommon = commonParams.length - uncoveredCommon.length;
const coveredTone = toneParams.length - uncoveredTone.length;

const report = {
  commonTotal: commonParams.length,
  commonCoveredApprox: coveredCommon,
  toneParamIds: toneParams.length,
  toneCoveredApprox: coveredTone,
  toneInstancesIfAll4: toneParams.length * 4,
  note: 'Coverage is approximate string match against UI evidence labels; false negatives possible for short/ambiguous names.',
  uncoveredCommon,
  uncoveredTone,
};

console.log(
  JSON.stringify(
    {
      common: `${coveredCommon}/${commonParams.length}`,
      toneIds: `${coveredTone}/${toneParams.length}`,
      uncoveredCommon: uncoveredCommon.map((p) => p.id),
      uncoveredTone: uncoveredTone.map((p) => p.id),
    },
    null,
    2,
  ),
);
writeFileSync(join(root, 'artifact/docs/xppatchstudio/hardware-evidence/live-param-coverage.json'), JSON.stringify(report, null, 2));
