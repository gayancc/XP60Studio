#include "presentation/PatchEditorViewModel.h"
#include "xpmodel/Xp60Effects.h"
#include "xpmodel/Xp60PatchRouting.h"

namespace xp60studio::presentation {
namespace {
QString text(std::string_view s) { return QString::fromUtf8(s.data(), qsizetype(s.size())); }
bool supported(const xpmodel::ParameterDescriptor& p)
{
    return (p.category == "EFX" && !p.id.starts_with("common.efx_parameter_"))
        || p.category == "Chorus" || p.category == "Reverb" || p.id == "tone.output_assign";
}
}

void PatchEditorViewModel::setEffectPage(int page)
{
    if (page < 0 || page > 4 || page == m_effectPage) return;
    endEffectGesture();
    m_effectPage = page;
    emit effectPageChanged();
}

// A knob drag is one thing the user did, so it is one step back — not one per
// pixel. The workspace owns that grouping now, which means it applies equally to
// an effect knob here and to an envelope point being pulled elsewhere.
void PatchEditorViewModel::beginEffectGesture()
{
    if (!hasPatch() || comparing() || m_effectGesture || editGestureActive()) return;
    m_effectGesture = true;
    m_effectGestureHasUndo = false;
    m_workspace.beginGesture();
}

void PatchEditorViewModel::endEffectGesture()
{
    // Only end the workspace gesture this function started. `commitEdit` calls
    // here on every commit to break a stale effect drag, and an unguarded
    // `endGesture()` there tore down whichever *other* gesture was running —
    // a Sound DNA drag was ended by its own first commit.
    const bool mine = m_effectGesture;
    m_effectGesture = false;
    m_effectGestureHasUndo = false;
    m_applyingEffectGesture = false;
    if (mine) {
        m_workspace.endGesture();
    }
}

QVariantMap PatchEditorViewModel::effectValues() const
{
    if (!hasPatch()) return {};
    const auto route = xpmodel::patchRouting(patch(), selectedToneIndex());
    QVariantMap result;
    for (const bool common : {true, false}) {
        const auto parameters = (common ? xpmodel::xp60tables::patchCommonTable()
                                       : xpmodel::xp60tables::patchToneTable()).parameters();
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            const auto& p = parameters[i];
            if (!supported(p) || (!common && !route.outputTone)) continue;
            const auto& block = common ? patch().common()
                : patch().tone(xpmodel::ToneIndex::all()[std::size_t(route.outputTone - 1)]);
            const int value = block.rawAt(i);
            // The A side comes from the workspace, so "modified" here means the
            // same thing it means everywhere else showing this Patch.
            const auto& baseline = m_workspace.baseline();
            const int original = !baseline ? value : common ? baseline->common().rawAt(i)
                : baseline->tone(xpmodel::ToneIndex::all()[std::size_t(route.outputTone - 1)]).rawAt(i);
            QStringList choices;
            for (auto label : p.enumLabels) choices.append(text(label));
            // Unknown OUTPUT-2 is preserved on import, never offered as a Design destination.
            if (p.id == "common.efx_output_assign" || p.id == "tone.output_assign") choices.removeLast();
            QString label = text(p.name);
            if (p.id == "tone.mix_efx_send_level") label = tr("Tone → Mix / EFX");
            else if (p.id == "tone.chorus_send_level") label = tr("Tone → Chorus");
            else if (p.id == "tone.reverb_send_level") label = tr("Tone → Reverb");
            else if (p.id == "common.efx_mix_out_send_level") label = tr("EFX output");
            else if (p.id == "common.efx_chorus_send_level") label = tr("EFX → Chorus");
            else if (p.id == "common.efx_reverb_send_level") label = tr("EFX → Reverb");
            else if (p.id == "common.reverb_time" && patch().raw(xpmodel::CommonParameter::ReverbType) >= 6) label = tr("Delay time");
            const QString unit = !common ? "tone" : p.category == "Chorus" ? "chorus" : p.category == "Reverb" ? "reverb" : "efx";
            result.insert(text(p.id), QVariantMap{{"value", value}, {"original", original},
                {"minimum", p.rawMin}, {"maximum", p.rawMax}, {"name", text(p.name)},
                {"label", label}, {"unit", unit},
                {"display", QString::fromStdString(p.formatDisplay(value))}, {"choices", choices},
                {"modified", value != original}, {"bipolar", p.displayOffset < 0}});
        }
    }
    return result;
}

void PatchEditorViewModel::editEffect(const QString& id, int value)
{
    if (!hasPatch() || comparing()) return;
    const bool common = id.startsWith(QStringLiteral("common."));
    const auto parameters = (common ? xpmodel::xp60tables::patchCommonTable()
                                   : xpmodel::xp60tables::patchToneTable()).parameters();
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        const auto& p = parameters[i];
        if (text(p.id) != id || !supported(p) || !p.isRawInRange(value)) continue;
        if ((p.id == "common.efx_output_assign" && value > 1)
            || (p.id == "tone.output_assign" && value > 2)) return;
        m_applyingEffectGesture = m_effectGesture;
        if (common) setCommonRaw(static_cast<xpmodel::CommonParameter>(i), value);
        else {
            const auto route = xpmodel::patchRouting(patch(), selectedToneIndex());
            if (route.outputTone) setToneRaw(xpmodel::ToneIndex::all()[std::size_t(route.outputTone - 1)],
                                            static_cast<xpmodel::ToneParameter>(i), value);
        }
        m_applyingEffectGesture = false;
        return;
    }
}

QVariantList PatchEditorViewModel::effectAlgorithms() const
{
    // UI terminology only, transcribed from Owner's Manual pp.199-203.
    // Deliberately no slot indices, conversion formulas, or pretend current values.
    static const QStringList controls{
        "Low Freq|Low Gain|High Freq|High Gain|P1 Freq|P1 Q|P1 Gain|P2 Freq|P2 Q|P2 Gain|Level",
        "Drive|Amp Type|Low Gain|High Gain|Pan|Level",
        "Drive|Amp Type|Low Gain|High Gain|Pan|Level",
        "Manual|Rate|Depth|Resonance|Mix|Pan|Level",
        "Band 1|Band 2|Band 3|Band 4|Band 5|Band 6|Band 7|Band 8|Band Width|Pan|Level",
        "Sensitivity|Mix|Low Gain|High Gain|Level",
        "Filter Type|Sensitivity|Manual|Peak|Rate|Depth|Level",
        "Low Slow Rate|Low Fast Rate|Low Acceleration|Low Level|High Slow Rate|High Fast Rate|High Acceleration|High Level|Separation|Speed|Level",
        "Attack|Sustain|Post Gain|Low Gain|High Gain|Pan|Level",
        "Threshold|Ratio|Release|Post Gain|Low Gain|High Gain|Pan|Level",
        "Pre Delay|Rate|Depth|Pre Delay Deviation|Depth Deviation|Pan Deviation|Balance|Level",
        "Pre Delay|Chorus Rate|Chorus Depth|Chorus Phase|Tremolo Rate|Tremolo Separation|Balance|Level",
        "Pre Delay|Rate|Depth|Phase|Low Gain|High Gain|Balance|Level",
        "Pre Delay|Rate|Depth|Phase|Filter Type|Cutoff Freq|Low Gain|High Gain|Balance|Level",
        "Pre Delay|Rate|Depth|Feedback|Phase|Filter Type|Cutoff Freq|Low Gain|High Gain|Balance|Level",
        "Pre Delay|Rate|Depth|Feedback|Phase|Step Rate|Low Gain|High Gain|Balance|Level",
        "Delay Left|Delay Right|Feedback|Feedback Mode|Phase Left|Phase Right|HF Damp|Low Gain|High Gain|Balance|Level",
        "Delay Left|Delay Right|Feedback|Feedback Mode|Rate|Depth|Phase|HF Damp|Low Gain|High Gain|Balance|Level",
        "Delay Center|Delay Left|Delay Right|Feedback|Center Level|Left Level|Right Level|HF Damp|Low Gain|High Gain|Balance|Level",
        "Delay 1|Delay 2|Delay 3|Delay 4|Level 1|Level 2|Level 3|Level 4|Feedback|HF Damp|Balance|Level",
        "Delay|Acceleration|Feedback|HF Damp|Pan|Low Gain|High Gain|Balance|Level",
        "Coarse A|Fine A|Pan A|Pre Delay A|Coarse B|Fine B|Pan B|Pre Delay B|Mode|Level Balance|Balance|Level",
        "Coarse|Fine|Pan|Pre Delay|Mode|Feedback|Low Gain|High Gain|Balance|Level",
        "Type|Pre Delay|Time|HF Damp|Low Gain|High Gain|Balance|Level",
        "Type|Pre Delay|Gate Time|Low Gain|High Gain|Balance|Level"
    };
    static const QStringList families{
        "eq", "drive", "drive", "filter", "spectrum", "enhancer", "filter", "rotary", "compressor", "limiter",
        "modulation", "modulation", "modulation", "modulation", "modulation", "modulation",
        "delay", "delay", "delay", "delay", "delay", "pitch", "pitch", "reverb", "gate"
    };
    // '#' annotations in the algorithm descriptions, printed pp.74-88.
    // Eligibility is known; this list does not assert byte-slot or CTRL-lane mapping.
    static const QStringList eligible{
        "Level", "Drive|Pan", "Drive|Pan", "Manual|Rate", "Pan|Level", "Sensitivity|Mix", "Manual|Rate", "Speed|Level",
        "Pan|Level", "Pan|Level", "Rate|Balance", "Tremolo Rate|Balance", "Rate|Balance", "Rate|Balance", "Rate|Feedback",
        "Feedback|Step Rate", "Feedback|Balance", "Rate|Balance", "Feedback|Balance", "Feedback|Balance", "Delay|Feedback",
        "Coarse A|Coarse B", "Coarse|Feedback", "Time|Balance", "Balance|Level",
        "A · Pan|B · Balance", "A · Pan|B · Balance", "A · Pan|B · Balance",
        "A · Pan|B · Balance", "A · Pan|B · Balance", "A · Pan|B · Balance",
        "A · Sensitivity|B · Balance", "A · Sensitivity|B · Balance", "A · Sensitivity|B · Balance",
        "A · Balance|B · Balance", "A · Balance|B · Balance", "A · Balance|B · Balance",
        "A · Balance|B · Balance", "A · Balance|B · Balance", "A · Balance|B · Balance"
    };
    QVariantList result;
    for (int i = 0; i < 40; ++i) {
        const QString name = text(xpmodel::kEfxTypeNames[std::size_t(i)]);
        const bool combined = i >= 25;
        const bool parallel = i >= 37;
        const QStringList stages = combined ? name.split(parallel ? QStringLiteral("/") : QStringLiteral(" -> ")) : QStringList{name};
        QVariantList stageModels;
        for (int s = 0; s < stages.size(); ++s) {
            const auto stage = stages[s];
            QString family;
            QStringList params;
            if (!combined) { family = families[i]; params = controls[i].split('|'); }
            else if (stage == "OVERDRIVE" || stage == "DISTORTION") { family = "drive"; params = {"Drive", "Pan"}; }
            else if (stage == "ENHANCER") { family = "enhancer"; params = {"Sensitivity", "Mix"}; }
            else if (stage == "DELAY") { family = "delay"; params = {"Time", "Feedback", "HF Damp", "Balance"}; }
            else { family = "modulation"; params = {"Pre Delay", "Rate", "Depth"};
                if (stage == "FLANGER") params.append("Feedback");
                params.append("Balance"); }
            if (combined && s == stages.size() - 1) params.append("Level");
            stageModels.append(QVariantMap{{"name", stage}, {"family", family}, {"controls", params}, {"bound", false}});
        }
        result.append(QVariantMap{{"value", i}, {"number", QStringLiteral("%1").arg(i + 1, 2, 10, QLatin1Char('0'))},
            {"name", name}, {"topology", combined ? parallel ? "PARALLEL" : "SERIES" : "SINGLE"},
            {"stages", stageModels}, {"eligibleTargets", eligible[i].split('|')}, {"bound", false}});
    }
    return result;
}
} // namespace xp60studio::presentation
