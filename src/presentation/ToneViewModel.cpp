#include "presentation/ToneViewModel.h"

#include "presentation/PatchEditorViewModel.h"

#include <QVariantMap>

namespace xp60studio::presentation {

using xpmodel::ToneParameter;

ToneViewModel::ToneViewModel(PatchEditorViewModel& editor, xpmodel::ToneIndex tone, QObject* parent)
    : QObject(parent)
    , m_editor(editor)
    , m_tone(tone)
{
}

bool ToneViewModel::hasPatch() const
{
    return m_editor.hasPatch();
}

bool ToneViewModel::enabled() const
{
    return hasPatch() && m_editor.patch().toneEnabled(m_tone);
}

void ToneViewModel::setEnabled(bool enabled)
{
    m_editor.setToneRaw(m_tone, ToneParameter::ToneSwitch, enabled ? 1 : 0);
}

QString ToneViewModel::waveText() const
{
    if (!hasPatch()) {
        return {};
    }
    // The XP-60 waveform name list is not transcribed yet, so the wave is
    // identified by its number. See docs/PHASE_4_PATCH_EDITOR.md.
    const auto wave = m_editor.patch().wave(m_tone);
    return QStringLiteral("%1 %2")
        .arg(QString::fromUtf8(wave.groupTypeLabel.data(), qsizetype(wave.groupTypeLabel.size())))
        .arg(wave.numberDisplay, 3, 10, QLatin1Char('0'));
}

QString ToneViewModel::waveSourceText() const
{
    if (!hasPatch()) {
        return {};
    }
    const auto wave = m_editor.patch().wave(m_tone);
    return QStringLiteral("group %1 · gain %2")
        .arg(wave.groupId)
        .arg(QString::fromUtf8(wave.gainLabel.data(), qsizetype(wave.gainLabel.size())));
}

int ToneViewModel::level() const
{
    return hasPatch() ? m_editor.patch().raw(m_tone, ToneParameter::ToneLevel) : 0;
}

void ToneViewModel::setLevel(int level)
{
    m_editor.setToneRaw(m_tone, ToneParameter::ToneLevel, level);
}

QString ToneViewModel::levelText() const
{
    return hasPatch() ? QString::number(level()) : QString();
}

int ToneViewModel::pan() const
{
    return hasPatch() ? m_editor.patch().raw(m_tone, ToneParameter::TonePan) : 64;
}

void ToneViewModel::setPan(int pan)
{
    m_editor.setToneRaw(m_tone, ToneParameter::TonePan, pan);
}

QString ToneViewModel::panText() const
{
    if (!hasPatch()) {
        return {};
    }
    const auto text = m_editor.patch().displayText(m_tone, ToneParameter::TonePan);
    return text == "0" ? QStringLiteral("C") : QString::fromStdString(text);
}

int ToneViewModel::coarseTune() const
{
    return hasPatch() ? m_editor.patch().display(m_tone, ToneParameter::CoarseTune) : 0;
}

int ToneViewModel::octave() const
{
    const int semitones = coarseTune();
    // Round toward zero so a +13 semitone tuning still reads as +1 octave.
    return semitones / 12;
}

QString ToneViewModel::octaveText() const
{
    if (!hasPatch()) {
        return {};
    }
    const int semitones = coarseTune();
    const int octaves = octave();
    const int remainder = semitones - octaves * 12;
    QString text = octaves > 0 ? QStringLiteral("+%1").arg(octaves) : QString::number(octaves);
    if (remainder != 0) {
        // Coarse Tune is per-semitone on the XP-60; show what the octave
        // control cannot express rather than hiding it.
        text += remainder > 0 ? QStringLiteral(" +%1").arg(remainder) : QStringLiteral(" %1").arg(remainder);
    }
    return text;
}

void ToneViewModel::nudgeOctave(int delta)
{
    if (!hasPatch() || delta == 0) {
        return;
    }
    const auto& descriptor = xpmodel::xp60tables::descriptor(ToneParameter::CoarseTune);
    const int target = m_editor.patch().raw(m_tone, ToneParameter::CoarseTune) + delta * 12;
    if (target >= descriptor.rawMin && target <= descriptor.rawMax) {
        m_editor.setToneRaw(m_tone, ToneParameter::CoarseTune, target);
    }
}

void ToneViewModel::setSolo(bool solo)
{
    if (m_solo == solo) {
        return;
    }
    m_solo = solo;
    m_editor.notifyAuditionChanged();
}

void ToneViewModel::setMute(bool mute)
{
    if (m_mute == mute) {
        return;
    }
    m_mute = mute;
    m_editor.notifyAuditionChanged();
}

bool ToneViewModel::audible() const
{
    if (!enabled() || m_mute) {
        return false;
    }
    return m_editor.anyToneSoloed() ? m_solo : true;
}

QVariantList ToneViewModel::miniEnvelope() const
{
    QVariantList points;
    if (!hasPatch()) {
        return points;
    }
    // The card preview always shows the amplitude (Level) envelope, which is
    // the one that describes how the Tone is heard.
    const auto envelope = m_editor.patch().levelEnvelope(m_tone);
    int totalTime = 0;
    for (const int t : envelope.timeRaw) {
        totalTime += t;
    }
    const double span = totalTime > 0 ? totalTime : 1.0;
    double elapsed = 0.0;
    QVariantMap start;
    start.insert(QStringLiteral("x"), 0.0);
    start.insert(QStringLiteral("y"), 0.0);
    points.append(start);
    for (int i = 0; i < 4; ++i) {
        elapsed += envelope.timeRaw[static_cast<std::size_t>(i)];
        const int levelIndex = std::min(i, envelope.levelCount - 1);
        const double level = i >= envelope.levelCount
            ? 0.0 // the final stage falls to silence
            : envelope.levelRaw[static_cast<std::size_t>(levelIndex)] / 127.0;
        QVariantMap point;
        point.insert(QStringLiteral("x"), elapsed / span);
        point.insert(QStringLiteral("y"), level);
        points.append(point);
    }
    return points;
}

void ToneViewModel::notifyChanged()
{
    emit changed();
}

} // namespace xp60studio::presentation
