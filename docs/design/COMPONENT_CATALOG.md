# XP60Studio UI Component Catalog

This catalog defines the reusable interactive UI building blocks Codex should create instead of styling each screen independently.

Visual reference: [`xp60studio-ui-master-mockup.jpg`](xp60studio-ui-master-mockup.jpg)

Implementation architecture: [`UI_IMPLEMENTATION_ARCHITECTURE.md`](UI_IMPLEMENTATION_ARCHITECTURE.md)

The names below are conceptual. Codex may refine names while preserving responsibility and reuse.

---

# 1. Foundation controls

Build branded wrappers/styles around Qt Quick Controls rather than using default platform visuals directly.

Expected foundation components:

- `XpButton` — primary/secondary/ghost/danger variants
- `XpIconButton`
- `XpToolButton`
- `XpToggle`
- `XpSwitch`
- `XpCheckBox`
- `XpRadioButton`
- `XpComboBox`
- `XpSearchField`
- `XpTextField`
- `XpNumericField`
- `XpSegmentedControl`
- `XpTabBar`
- `XpChip` / `FilterChip`
- `XpBadge` / `StatusPill`
- `XpTooltip`
- `XpMenu`
- `XpContextMenu`
- `XpDialog`
- `XpDrawer`
- `XpToast` / transient notification
- `XpProgressBar`
- `XpBusyIndicator`
- `XpScrollArea`
- `XpCard`
- `XpPanelHeader`
- `XpEmptyState`
- `XpErrorState`

All must consume shared Theme/Typography/Metrics tokens.

---

# 2. Global shell components

These establish the persistent product experience seen in the master mockup.

- `AppNavigationRail`
- `AppHeader`
- `ConnectionStatusIndicator`
- `CurrentDeviceSelector`
- `GlobalActivityIndicator`
- `GlobalOperationTray`
- `BreadcrumbBar`
- `ScreenHeader`
- `InspectorDrawer`

`ConnectionStatusIndicator` is authoritative globally. Screens should not invent different connection semantics.

---

# 3. Patch and Tone components

Central to the approved Patch Editor design:

- `PatchHeaderCard`
- `PatchIdentityTags`
- `ToneCard`
- `ToneColorMarker`
- `ToneEnableControl`
- `ToneSoloButton`
- `ToneMuteButton`
- `ToneLevelControl`
- `TonePanControl`
- `ToneOctaveControl`
- `ToneWaveSelector`
- `ToneMiniEnvelope`
- `ToneContributionMeter`
- `FourToneMixer`
- `FourToneOverview`
- `PatchDirtyStateBadge`
- `PatchHardwareStateBadge`
- `OriginalCurrentToggle` for A/B

Tone identity colors must be semantic tokens shared across all screens.

---

# 4. Synth editing controls

These should feel like premium instrument controls, not generic form fields.

- `XpKnob`
- `XpBipolarKnob`
- `XpFader`
- `XpBipolarFader`
- `ParameterValueEditor`
- `ParameterLabel`
- `ParameterResetAction`
- `ParameterFineAdjustMode`
- `EnvelopeEditor`
- `EnvelopePointHandle`
- `EnvelopeStageReadout`
- `KeyRangeSelector`
- `VelocityRangeSelector`
- `KeyboardStrip`
- `StructureSelector`
- `EffectBlock`
- `EffectRoutingView`
- `SignalFlowNode`
- `SignalFlowConnector`
- `LfoShapeSelector`
- `LfoPreview`

Every continuous visual control must also support exact numeric entry.

---

# 5. Wave Browser components

Reference: lower-left screen in the master mockup.

- `WaveSearchBar`
- `WaveSourceFilter`
- `WaveCategoryFilter`
- `WaveResultList`
- `WaveResultRow`
- `WaveAvailabilityBadge`
- `ExpansionRequirementBadge`
- `WavePreviewPanel`
- `WaveMetadataPanel`
- `WaveCompatibilityPanel`
- `WaveSourceBadge`

Availability states must distinguish at least:

- available
- missing expansion
- unknown mapping
- incompatible/unsupported if verified

Never show unavailable waves as normal selectable results without warning.

---

# 6. Library components

- `LibrarySearchBar`
- `PatchResultList`
- `PatchResultCard`
- `PatchTagEditor`
- `PatchRatingControl`
- `FavouriteButton`
- `ImportDropZone`
- `ImportProgressPanel`
- `ImportAnalysisSummary`
- `SourceProvenancePanel`
- `LibraryFilterDrawer`
- `LibrarySortControl`
- `CompatibilityFilter`
- `DuplicateStatusBadge`
- `PatchDnaSummary`

Large lists must use virtualized Qt model/view patterns.

---

# 7. Bank Builder components

Reference: lower-right screen in the master mockup.

- `BankSelector`
- `BankOccupancyIndicator`
- `BankCategoryRail`
- `BankGrid`
- `BankSlot`
- `BankSlotDragGhost`
- `BankDropIndicator`
- `BankSectionEditor`
- `BankSelectionToolbar`
- `BankAnalysisSummary`
- `BankWarningPanel`
- `BankPatchInspector`
- `SmartBankProposalPanel`
- `BankTransferAction`

`BankSlot` must visually represent states such as:

- normal
- selected
- empty
- local-only
- modified
- duplicate
- near-duplicate
- missing expansion
- transfer mismatch
- verified on hardware

Drag/drop must have explicit non-drag alternatives.

---

# 8. Compare components

- `PatchCompareHeader`
- `PatchCompareColumn`
- `ToneDifferenceBar`
- `ParameterDifferenceRow`
- `DifferenceGroup`
- `OnlyDifferencesToggle`
- `ComponentCopyAction`
- `SimilarityBadge`
- `ComparisonSummary`

Unchanged parameters should be collapsed/suppressed by default rather than flooding the user.

---

# 9. Expansion components

- `ExpansionSlotCard`
- `ExpansionBoardPicker`
- `ExpansionProfileSummary`
- `PatchCompatibilityCard`
- `ToneCompatibilityRow`
- `MissingWaveResolutionPanel`

The UI must never imply an expansion is physically installed merely because a patch references it.

---

# 10. Device, transfer, and diagnostics components

- `MidiEndpointPicker`
- `DeviceConnectionCard`
- `DeviceHealthCard`
- `SysExHealthIndicator`
- `RoundTripTestAction`
- `TransferProgressPanel`
- `TransferItemRow`
- `VerificationStatus`
- `TransferMismatchCard`
- `RetryAction`
- `CommunicationTimeline`
- `ProtocolLogView`
- `RawSysExInspector`
- `ConnectionDiagnosticsPanel`

Transfer progress should show semantic states such as Sending, Waiting, Read Back, Comparing, Verified, Failed, Cancelled.

---

# 11. Performance components

- `PerformanceHeader`
- `PartMixer`
- `PerformancePartStrip`
- `PartPatchSelector`
- `PartChannelSelector`
- `PartLevelMeter`
- `PartPanControl`
- `PartKeyRange`
- `PartVelocityRange`
- `PartEffectsRouting`
- `PartMuteSoloControls`

The visual model should resemble a musical mixer/workstation, not a property grid.

---

# 12. Rhythm components

- `RhythmKeyboardMap`
- `RhythmKey`
- `RhythmToneInspector`
- `RhythmWaveSelector`
- `RhythmLevelControl`
- `RhythmPanControl`
- `RhythmPitchControl`
- `RhythmEffectsControls`

Selection of a keyboard key should drive the inspector rather than opening repeated modal dialogs.

---

# 13. Snapshot / restore components

- `SnapshotCard`
- `SnapshotCoverageSummary`
- `SnapshotProgress`
- `RestorePlan`
- `SafetySnapshotPrompt`
- `RestoreDifferenceSummary`
- `RestoreVerificationSummary`

Always distinguish what can be restored with verified support from data merely captured for archival purposes.

---

# 14. Sound intelligence components

Later phases:

- `PatchDnaRadarOrSummary` only if visualization genuinely helps
- `SimilarityClusterView`
- `VariationAmountControl`
- `VariationLocks`
- `VariationCandidateCard`
- `MutationTree`
- `MorphControl`
- `TransformationChip`
- `VersionHistoryTimeline`

Do not make these visually impressive at the expense of explainability.

---

# 15. Live Mode components

- `SetlistNavigator`
- `SongCard`
- `CurrentSongStageCard`
- `CurrentSoundStageCard`
- `NextSoundPreview`
- `SectionNavigator`
- `LiveConnectionStatus`

Live Mode should intentionally hide destructive editing actions.

---

# 16. Component acceptance rule

A reusable component is not complete merely because it resembles the mockup.

For each interactive component verify:

- default state
- hover state
- keyboard focus
- pressed state
- selected state if applicable
- disabled state
- error/warning state if applicable
- high-DPI rendering
- keyboard interaction
- accessible name/value where possible
- light/dark assumptions (XP60Studio is dark-first; still centralize tokens)
- data binding without embedded protocol logic

The component library should make later screens faster to build while preserving one coherent visual language.
