#include "xpmodel/Xp60Patch.h"

namespace xp60studio::xpmodel {

namespace {

constexpr std::size_t idx(CommonParameter p) noexcept
{
    return static_cast<std::size_t>(p);
}
constexpr std::size_t idx(ToneParameter p) noexcept
{
    return static_cast<std::size_t>(p);
}

} // namespace

Xp60Patch::Xp60Patch(BlockValues common, std::array<BlockValues, ToneIndex::kCount> tones)
    : m_common(std::move(common))
    , m_tones(std::move(tones))
{
}

int Xp60Patch::raw(CommonParameter parameter) const noexcept
{
    return m_common.rawAt(idx(parameter));
}

int Xp60Patch::raw(ToneIndex tone, ToneParameter parameter) const noexcept
{
    return m_tones[tone.index()].rawAt(idx(parameter));
}

int Xp60Patch::display(CommonParameter parameter) const noexcept
{
    return xp60tables::descriptor(parameter).toDisplay(raw(parameter));
}

int Xp60Patch::display(ToneIndex tone, ToneParameter parameter) const noexcept
{
    return xp60tables::descriptor(parameter).toDisplay(raw(tone, parameter));
}

std::string Xp60Patch::displayText(CommonParameter parameter) const
{
    return xp60tables::descriptor(parameter).formatDisplay(raw(parameter));
}

std::string Xp60Patch::displayText(ToneIndex tone, ToneParameter parameter) const
{
    return xp60tables::descriptor(parameter).formatDisplay(raw(tone, parameter));
}

bool Xp60Patch::setRaw(CommonParameter parameter, int value) noexcept
{
    return m_common.setRawAt(idx(parameter), value);
}

bool Xp60Patch::setRaw(ToneIndex tone, ToneParameter parameter, int value) noexcept
{
    return m_tones[tone.index()].setRawAt(idx(parameter), value);
}

PatchName Xp60Patch::name() const
{
    PatchName::Bytes bytes{};
    for (std::size_t i = 0; i < PatchName::kLength; ++i) {
        bytes[i] = static_cast<roland::Byte>(m_common.rawAt(idx(CommonParameter::PatchName1) + i) & 0x7F);
    }
    // Bytes came through the table's 32..127 range check, so this cannot fail;
    // fall back to a blank name defensively rather than asserting.
    return PatchName::fromBytes(bytes).value_or(PatchName());
}

bool Xp60Patch::setName(const PatchName& value) noexcept
{
    const auto& bytes = value.bytes();
    for (std::size_t i = 0; i < PatchName::kLength; ++i) {
        if (!m_common.setRawAt(idx(CommonParameter::PatchName1) + i, bytes[i])) {
            return false;
        }
    }
    return true;
}

bool Xp60Patch::toneEnabled(ToneIndex tone) const noexcept
{
    return raw(tone, ToneParameter::ToneSwitch) == 1;
}

int Xp60Patch::enabledToneCount() const noexcept
{
    int count = 0;
    for (const auto tone : ToneIndex::all()) {
        if (toneEnabled(tone)) {
            ++count;
        }
    }
    return count;
}

Xp60Patch::WaveReference Xp60Patch::wave(ToneIndex tone) const
{
    WaveReference ref;
    ref.groupTypeRaw = raw(tone, ToneParameter::WaveGroupType);
    ref.groupTypeLabel = xp60tables::descriptor(ToneParameter::WaveGroupType).label(ref.groupTypeRaw).value_or("?");
    ref.groupId = raw(tone, ToneParameter::WaveGroupId);
    ref.numberRaw = raw(tone, ToneParameter::WaveNumber);
    ref.numberDisplay = display(tone, ToneParameter::WaveNumber);
    ref.gainRaw = raw(tone, ToneParameter::WaveGain);
    ref.gainLabel = xp60tables::descriptor(ToneParameter::WaveGain).label(ref.gainRaw).value_or("?");
    return ref;
}

namespace {

Xp60Patch::Envelope makeEnvelope(const Xp60Patch& patch, ToneIndex tone, std::string_view name,
                                 std::array<ToneParameter, 4> times, std::array<ToneParameter, 4> levels, int levelCount)
{
    Xp60Patch::Envelope env;
    env.name = name;
    env.levelCount = levelCount;
    for (std::size_t i = 0; i < 4; ++i) {
        env.timeRaw[i] = patch.raw(tone, times[i]);
    }
    for (int i = 0; i < levelCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        env.levelRaw[index] = patch.raw(tone, levels[index]);
        env.levelDisplay[index] = patch.display(tone, levels[index]);
    }
    return env;
}

} // namespace

Xp60Patch::Envelope Xp60Patch::pitchEnvelope(ToneIndex tone) const
{
    return makeEnvelope(*this, tone, "Pitch Envelope",
                        {ToneParameter::PitchEnvelopeTime1, ToneParameter::PitchEnvelopeTime2, ToneParameter::PitchEnvelopeTime3,
                         ToneParameter::PitchEnvelopeTime4},
                        {ToneParameter::PitchEnvelopeLevel1, ToneParameter::PitchEnvelopeLevel2, ToneParameter::PitchEnvelopeLevel3,
                         ToneParameter::PitchEnvelopeLevel4},
                        4);
}

Xp60Patch::Envelope Xp60Patch::filterEnvelope(ToneIndex tone) const
{
    return makeEnvelope(*this, tone, "Filter Envelope",
                        {ToneParameter::FilterEnvelopeTime1, ToneParameter::FilterEnvelopeTime2, ToneParameter::FilterEnvelopeTime3,
                         ToneParameter::FilterEnvelopeTime4},
                        {ToneParameter::FilterEnvelopeLevel1, ToneParameter::FilterEnvelopeLevel2, ToneParameter::FilterEnvelopeLevel3,
                         ToneParameter::FilterEnvelopeLevel4},
                        4);
}

Xp60Patch::Envelope Xp60Patch::levelEnvelope(ToneIndex tone) const
{
    return makeEnvelope(*this, tone, "Level Envelope",
                        {ToneParameter::LevelEnvelopeTime1, ToneParameter::LevelEnvelopeTime2, ToneParameter::LevelEnvelopeTime3,
                         ToneParameter::LevelEnvelopeTime4},
                        {ToneParameter::LevelEnvelopeLevel1, ToneParameter::LevelEnvelopeLevel2, ToneParameter::LevelEnvelopeLevel3,
                         ToneParameter::LevelEnvelopeLevel3},
                        3);
}

std::string Xp60Patch::summary() const
{
    std::string out = "'" + name().displayText() + "'";
    out += " · tones";
    bool any = false;
    for (const auto tone : ToneIndex::all()) {
        if (toneEnabled(tone)) {
            out += any ? "," : " ";
            out += std::to_string(tone.number());
            any = true;
        }
    }
    if (!any) {
        out += " none";
    }
    out += " · structure " + displayText(CommonParameter::StructureType12) + "/" + displayText(CommonParameter::StructureType34);
    out += " · EFX " + displayText(CommonParameter::EfxType);
    out += " · reverb " + displayText(CommonParameter::ReverbType);
    return out;
}

bool operator==(const Xp60Patch& lhs, const Xp60Patch& rhs) noexcept
{
    return lhs.m_common == rhs.m_common && lhs.m_tones == rhs.m_tones;
}

} // namespace xp60studio::xpmodel
