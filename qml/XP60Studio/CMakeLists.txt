# XP60Studio QML module: design tokens, reusable controls, shell and screens.
# QML here is presentation only; every domain decision lives in C++.

set(XP60STUDIO_QML_FILES
  Main.qml
  Theme/Theme.qml
  Theme/Typography.qml
  Theme/Metrics.qml
  Theme/Motion.qml
  Controls/XpButton.qml
  Controls/XpIcon.qml
  Controls/XpCard.qml
  Controls/XpComboBox.qml
  Controls/XpDivider.qml
  Controls/XpEmptyState.qml
  Controls/XpSection.qml
  Controls/XpSwitch.qml
  Controls/XpFieldRow.qml
  Controls/XpValueReadout.qml
  Controls/XpByteView.qml
  Controls/XpLabel.qml
  Controls/XpMetricTile.qml
  Controls/XpPanelHeader.qml
  Controls/XpScrollBar.qml
  Controls/XpSpinField.qml
  Controls/XpTextField.qml
  Controls/StatusPill.qml
  Controls/XpKnob.qml
  Controls/XpFader.qml
  Controls/XpSegmentedControl.qml
  Controls/XpRangeBar.qml
  Controls/ParameterValueEditor.qml
  Controls/EditorParameterPanel.qml
  Controls/ToneCard.qml
  Controls/ToneMiniEnvelope.qml
  Controls/EnvelopeEditor.qml
  Controls/KeyboardStrip.qml
  Controls/DashboardSummaryCard.qml
  Controls/DashboardToneCard.qml
  Controls/EffectsCanvas.qml
  Controls/EffectProcessorNode.qml
  Controls/EffectRouteChip.qml
  Controls/EffectSignalRail.qml
  Controls/EffectMicroViz.qml
  Controls/EffectsWorkbench.qml
  Controls/EffectParameterControl.qml
  Controls/EffectChoiceControl.qml
  Controls/EffectRouteSelector.qml
  Controls/EffectParameterGraph.qml
  Controls/FourToneMixer.qml
  Controls/LfoShapeSelector.qml
  Controls/LfoPreview.qml
  Controls/DeviceConnectionBar.qml
  Controls/DeviceHealthCard.qml
  Controls/SafeReadFlow.qml
  Controls/OperationProgressRow.qml
  Controls/LibraryTransferCard.qml
  Controls/ProtocolActivityPanel.qml
  Controls/TransferMismatchCard.qml
  Controls/WaveSearchBar.qml
  Controls/WaveResultRow.qml
  Controls/WaveAvailabilityBadge.qml
  Controls/LibraryResultRow.qml
  Controls/LibraryDetailsPanel.qml
  Controls/XpHardwareButton.qml
  Controls/BankPanelDisplay.qml
  Controls/BankDestinationTile.qml
  Controls/BankOverviewMap.qml
  Controls/BankSourcePanel.qml
  Shell/AppNavigationRail.qml
  Shell/AppHeader.qml
  Shell/ConnectionStatusIndicator.qml
  Shell/ScreenHeader.qml
  Screens/DashboardScreen.qml
  Screens/DevicesScreen.qml
  Screens/EditorScreen.qml
  Screens/WaveBrowserScreen.qml
  Screens/LibraryScreen.qml
  Screens/BankBuilderScreen.qml
  Screens/UnavailableScreen.qml
)

set_source_files_properties(
  Theme/Theme.qml
  Theme/Typography.qml
  Theme/Metrics.qml
  Theme/Motion.qml
  PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

# The module contains QML files only, so no plugin is generated (NO_PLUGIN):
# applications and tests link the STATIC backing library, which carries the QML
# sources and qmldir as resources under qrc:/qt/qml/XP60Studio. STATIC keeps
# this independent of shared-library load order and linker --as-needed rules.
qt_add_qml_module(xp60studio_ui
  STATIC
  NO_PLUGIN
  URI XP60Studio
  VERSION 1.0
  RESOURCE_PREFIX /qt/qml
  OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/XP60Studio
  QML_FILES ${XP60STUDIO_QML_FILES}
)
target_link_libraries(xp60studio_ui PRIVATE Qt6::Quick Qt6::QuickControls2)
