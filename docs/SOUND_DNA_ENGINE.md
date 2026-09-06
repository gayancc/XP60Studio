# Sound DNA evidence and implementation contract

Sound DNA is derived application metadata. It is neither a Roland parameter
set nor a collection of hand-authored macros. The runtime intentionally ships
with no visible DNA dimensions until a reviewed knowledge model passes every
gate below.

## Current evidence

The repository contains one known-good 128-Patch User bank and a complete
documentation-derived Patch parameter model. That fixture proves codec and
round-trip behaviour, not perceptual meaning. The internal wave catalog has
448 official names but no verified spectral, transient, loop, or instrument
taxonomy. It must not be used to fabricate those properties.

The required v1 research corpus is a private capture of all 512 onboard XP-60
presets, standardized whole-Patch and isolated-Tone audio, and isolated renders
of the 448 internal waves. Expansion boards are separate, versioned cohorts.
Raw factory SysEx and audio are not committed; reviewed aggregate artifacts,
content hashes and this methodology are.

## Offline workflow

1. Fetch presets through the existing XP-60 read/codec path. Do not write User
   memory. Verify names and bank/slot identity against the official Patch list.
2. Export decoded features with `xp60studio_sounddna_export INPUT.syx OUTPUT.jsonl`.
   Schema `xp60-patch-features/2` retains every documented raw value, whether a
   Tone is active, and an atomic categorical waveform identity. Disabled-Tone
   storage remains available for provenance but is neutral during analysis.
3. Capture a fixed MIDI performance at several velocities/registers, release
   tails, an interval and a chord. Generate the versioned note-only sequence
   with `python tools/generate_sounddna_stimulus.py stimulus.mid --manifest
   stimulus.json`. Preserve raw audio and create a level-matched derivative for
   timbre judgments. The manifest hash makes stimulus drift detectable.
4. Collect blinded category-matched pairwise judgments from at least five
   experienced raters per dimension/cohort. Names and bank identity remain
   hidden. Repeated pairs measure Krippendorff nominal alpha; a majority-vote
   fraction is not accepted as inter-rater reliability.
5. Run `tools/sounddna_analysis.py audit-corpus`, `fit-ratings`,
   `analyze-relations`, and `gate-evidence`. The relation report ranks
   category-conditioned monotonic associations and pair interactions, but
   labels them exploratory. Train and test interpretable models on
   source/near-duplicate-cluster splits, never random Patch rows.
6. Put only passing models in `docs/data/sounddna_model.json`, review its model
   card, then run `tools/generate_sounddna_model.py`. Application startup never
   analyzes the corpus.

The automated hardware capture stage may select presets and issue RQ1 reads
only after the exact XP-60 preset Bank Select mapping has official or captured
evidence. The project currently does not encode that mapping, so a tool that
guessed it would be false automation. Audio-device capture also remains a
development dependency, never an application/runtime dependency.

The generated stimulus intentionally contains no Bank Select, Program Change,
SysEx or controller events. This makes it safe and reproducible now without
quietly turning an unverified preset map into corpus metadata.

## Publication gates

A category cohort needs at least 30 independent Patches and five raters. Each
dimension needs Krippendorff alpha >= 0.70, held-out rank correlation >= 0.65, held-out pairwise
accuracy >= 0.70, calibrated 90% interval width <= 20 points, and no unresolved
correlation above `|0.80|` with another published dimension. An editable
dimension additionally needs blinded transformation-direction accuracy >= 75%
and median identity preservation >= 4/5.

Factory correlations are not causal transformation evidence. Editing requires
controlled temporary-Patch perturbations, recorded audio, and blinded before /
after judgments. A dimension can therefore be published as analysis-only.

## Runtime behaviour

`PatchFeatureExtractor` retains canonical parameter and relational features.
`SoundDnaAnalyzer` evaluates only passing category-conditioned models and emits
cohort percentiles, calibrated intervals, confidence and four-Tone attribution.
`SoundDnaTransformationEngine` performs deterministic constrained search over
validated continuous parameters and penalizes target error, collateral DNA
movement, and edit distance.

V1 transformations preserve waveform identity, Tone switches, structures,
key/velocity architecture, controller destinations and unknown EFX slots.
Analysis terms are read-only unless controlled listening evidence separately
marks them transformable. Each transformable term has an edit-risk cost and a
maximum normalized movement, while each request has a maximum changed-parameter
count and a bounded real-time evaluation budget (1,024 candidates by default).
Target-contributing parameters are searched before optional compensation terms.
Scores use an empirical latent-to-percentile calibration curve rather than a
linear parameter scale.

Inactive Tone bytes cannot affect a score or Tone attribution, and a model that
expects an inactive Tone loses in-distribution support instead of reporting
false high confidence. Interactions are
centered on cohort reference values, and attribution separates Tone-local from
patch-wide contribution. Missing/categorical model features suppress the
dimension rather than silently contributing zero. Feature support and category
certainty widen the interval and lower confidence for out-of-distribution
Patches. A category classifier must reach probability 0.60 with a 0.15 margin;
otherwise the runtime uses a validated global model or reports unavailable.

Runtime model loading rejects constant/malformed curves, duplicate identities,
invalid evidence ranges, interactions without their source terms, invalid
support bounds, and non-finite coefficients. The generated model validator
applies the same contract before compilation.
Unreachable requests return the nearest safe result or a refusal. Explanations
are generated from the actual parameter diff and recalculated profile.

The Play-mode panel consumes only the C++ evidence-gated profile. A complete
DNA gesture creates one editor undo entry and reaches the hardware solely via
the existing coalesced temporary-Patch live-audition path.

## Remaining evidence gaps

- The repository still has no lawful, verified 512-preset parameter capture.
- No standardized XP-60 audio corpus or isolated-Tone/wave renders are present.
- The 448 internal wave names have no verified spectral, transient or loop
  annotations. Names are not used to invent them.
- No five-rater blinded comparison set exists, so no perceptual dimension can
  pass publication.
- `analyze-relations` reports centered interactions, collinearity and dimension
  overlap, but intentionally does not promote exploratory correlation into a
  causal edit rule. Artifact publication remains a reviewed offline step.
- Physical capture automation remains blocked on verified preset Bank Select
  mapping and an explicitly selected development audio interface.
