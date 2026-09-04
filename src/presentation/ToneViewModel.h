#pragma once

#include "xpmodel/Xp60Patch.h"

#include <QObject>
#include <QString>
#include <QVariantList>

namespace xp60studio::presentation {

class PatchEditorViewModel;

// One Tone of the Patch under edit, as the four-Tone mixer needs it.
//
// Holds no state of its own beyond audition (solo/mute): every value is read
// from and written to the editor's working Patch, so the card and the model
// can never drift apart.
class ToneViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int toneNumber READ toneNumber CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(QString waveText READ waveText NOTIFY changed)
    Q_PROPERTY(QString waveSourceText READ waveSourceText NOTIFY changed)
    Q_PROPERTY(int level READ level WRITE setLevel NOTIFY changed)
    Q_PROPERTY(QString levelText READ levelText NOTIFY changed)
    Q_PROPERTY(int pan READ pan WRITE setPan NOTIFY changed)
    Q_PROPERTY(QString panText READ panText NOTIFY changed)
    Q_PROPERTY(int octave READ octave NOTIFY changed)
    Q_PROPERTY(QString octaveText READ octaveText NOTIFY changed)
    Q_PROPERTY(int coarseTune READ coarseTune NOTIFY changed)
    Q_PROPERTY(bool solo READ solo WRITE setSolo NOTIFY auditionChanged)
    Q_PROPERTY(bool mute READ mute WRITE setMute NOTIFY auditionChanged)
    // False when another Tone is soloed or this one is muted. Audition state
    // only: the Patch data is never altered by it.
    Q_PROPERTY(bool audible READ audible NOTIFY auditionChanged)
    Q_PROPERTY(QVariantList miniEnvelope READ miniEnvelope NOTIFY changed)

public:
    ToneViewModel(PatchEditorViewModel& editor, xpmodel::ToneIndex tone, QObject* parent = nullptr);

    [[nodiscard]] int toneNumber() const noexcept { return m_tone.number(); }
    [[nodiscard]] xpmodel::ToneIndex toneIndex() const noexcept { return m_tone; }

    [[nodiscard]] bool enabled() const;
    void setEnabled(bool enabled);
    [[nodiscard]] QString waveText() const;
    [[nodiscard]] QString waveSourceText() const;
    [[nodiscard]] int level() const;
    void setLevel(int level);
    [[nodiscard]] QString levelText() const;
    [[nodiscard]] int pan() const;
    void setPan(int pan);
    [[nodiscard]] QString panText() const;
    [[nodiscard]] int octave() const;
    [[nodiscard]] QString octaveText() const;
    [[nodiscard]] int coarseTune() const;
    [[nodiscard]] bool solo() const noexcept { return m_solo; }
    void setSolo(bool solo);
    [[nodiscard]] bool mute() const noexcept { return m_mute; }
    void setMute(bool mute);
    [[nodiscard]] bool audible() const;
    [[nodiscard]] QVariantList miniEnvelope() const;

    // Steps the Octave control, which moves Coarse Tune by twelve semitones.
    Q_INVOKABLE void nudgeOctave(int delta);

    void notifyChanged();
    void notifyAuditionChanged() { emit auditionChanged(); }

signals:
    void changed();
    void auditionChanged();

private:
    [[nodiscard]] bool hasPatch() const;

    PatchEditorViewModel& m_editor;
    xpmodel::ToneIndex m_tone;
    bool m_solo = false;
    bool m_mute = false;
};

} // namespace xp60studio::presentation
