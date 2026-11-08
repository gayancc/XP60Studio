# SALE — Parameter-Driven Psychoacoustic Estimator (ELS)

Bank-wide **Normalize bank** for already-loaded User slots (up to 128). Uses SysEx
parameters only — no audio rendering. Levels patches without redesigning their sound.

## Hard rules

1. Never lose a patch’s sounding character or building blocks.
2. Only **active** Tones (`tone_switch` ON) contribute to ELS and may have `tone_level`
   scaled. Muted slots stay byte-equal; switches are never flipped.
3. Editable parameters only: **Patch Common Level** (primary), **Tone Level** (secondary,
   proportional). Never TVF, waves, structure, TVA times/levels, FX, zones, sends.

## Estimated Loudness Score

Per active Tone:

\[
\mathrm{Tone\_ELS} = W_{\mathrm{base}} \times F_{\mathrm{weight}} \times E_{\mathrm{sustain}} \times V_{\mathrm{level}} \times B_{\mathrm{struct}}
\]

- \(W_{\mathrm{base}}\) — wave-name LUT, with **patch-title fallback** for EXP/unnamed waves
  ([`ui/src/model/waveEnergy.ts`](../../../ui/src/model/waveEnergy.ts)). Dist/TB/Flash score
  high; Mariachi/soft winds score low.
- \(F_{\mathrm{weight}}\) — TVF weight (dark LPF penalised; resonance restores presence).
- \(E_{\mathrm{sustain}}\) — TVA L3-weighted integral proxy.
- \(V_{\mathrm{level}}\) — Tone Level / 127.
- \(B_{\mathrm{struct}}\) — structure/booster path factor.

Patch combine uses a **power-sum** of active Tone_ELS (layered tones are not 4× one tone),
then × (patch_level/127).

## Pipeline

1. Score every filled slot.
2. \(\mathrm{absMedian} = \mathrm{median}(\mathrm{Patch\_ELS})\).
3. Per slot:
   - if ELS > absMedian → cut to absMedian (Dist TB-303, stacked strings);
   - else → lift toward \(\min(\mathrm{absMedian},\, 0.92\times\mathrm{capacity})\).
4. Closed-form `patch_level`, then proportional active `tone_level` (mix locked).
5. Preview → Apply in memory. Bank write rebuilds Roland SysEx checksums.

## Runtime

- Core: [`ui/src/model/voiceLeveling.ts`](../../../ui/src/model/voiceLeveling.ts)
- Bank: **Normalize bank (preview)** / Apply / Undo on the Bank Slot tab
