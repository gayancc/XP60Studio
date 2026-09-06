#include "xpmodel/Xp60Performance.h"

#include <utility>

namespace xp60studio::xpmodel {

namespace {

constexpr std::size_t idx(PerformanceCommonParameter p) noexcept
{
    return static_cast<std::size_t>(p);
}
constexpr std::size_t idx(PerformancePartParameter p) noexcept
{
    return static_cast<std::size_t>(p);
}

// Performance Common carries one Voice Reserve per Part, in Part order.
constexpr PerformanceCommonParameter voiceReserveFor(PartIndex part) noexcept
{
    return static_cast<PerformanceCommonParameter>(idx(PerformanceCommonParameter::VoiceReserve1) + part.index());
}

} // namespace

Xp60Performance::Xp60Performance(BlockValues common, std::array<BlockValues, PartIndex::kCount> parts)
    : m_common(std::move(common))
    , m_parts(std::move(parts))
{
}

int Xp60Performance::raw(PerformanceCommonParameter parameter) const noexcept
{
    return m_common.rawAt(idx(parameter));
}

int Xp60Performance::raw(PartIndex part, PerformancePartParameter parameter) const noexcept
{
    return m_parts[part.index()].rawAt(idx(parameter));
}

int Xp60Performance::display(PerformanceCommonParameter parameter) const noexcept
{
    return xp60performance::descriptor(parameter).toDisplay(raw(parameter));
}

int Xp60Performance::display(PartIndex part, PerformancePartParameter parameter) const noexcept
{
    return xp60performance::descriptor(parameter).toDisplay(raw(part, parameter));
}

std::string Xp60Performance::displayText(PerformanceCommonParameter parameter) const
{
    return xp60performance::descriptor(parameter).formatDisplay(raw(parameter));
}

std::string Xp60Performance::displayText(PartIndex part, PerformancePartParameter parameter) const
{
    return xp60performance::descriptor(parameter).formatDisplay(raw(part, parameter));
}

bool Xp60Performance::setRaw(PerformanceCommonParameter parameter, int value) noexcept
{
    return m_common.setRawAt(idx(parameter), value);
}

bool Xp60Performance::setRaw(PartIndex part, PerformancePartParameter parameter, int value) noexcept
{
    return m_parts[part.index()].setRawAt(idx(parameter), value);
}

PatchName Xp60Performance::name() const
{
    PatchName::Bytes bytes{};
    for (std::size_t i = 0; i < PatchName::kLength; ++i) {
        bytes[i] = static_cast<roland::Byte>(
            m_common.rawAt(idx(PerformanceCommonParameter::PerformanceName1) + i) & 0x7F);
    }
    // Bytes came through the table's 32..127 range check, so this cannot fail;
    // fall back to a blank name defensively rather than asserting.
    return PatchName::fromBytes(bytes).value_or(PatchName());
}

bool Xp60Performance::setName(const PatchName& value) noexcept
{
    const auto& bytes = value.bytes();
    for (std::size_t i = 0; i < PatchName::kLength; ++i) {
        if (!m_common.setRawAt(idx(PerformanceCommonParameter::PerformanceName1) + i, bytes[i])) {
            return false;
        }
    }
    return true;
}

Xp60Performance::PartAssignment Xp60Performance::assignment(PartIndex part) const
{
    PartAssignment out;
    out.groupTypeRaw = raw(part, PerformancePartParameter::PatchGroupType);
    out.groupTypeLabel = xp60performance::descriptor(PerformancePartParameter::PatchGroupType)
                             .label(out.groupTypeRaw)
                             .value_or(std::string_view{});
    out.groupId = raw(part, PerformancePartParameter::PatchGroupId);
    out.numberRaw = raw(part, PerformancePartParameter::PatchNumber);
    out.numberDisplay = display(part, PerformancePartParameter::PatchNumber);
    return out;
}

Xp60Performance::PartMix Xp60Performance::mix(PartIndex part) const
{
    PartMix out;
    out.receives = raw(part, PerformancePartParameter::ReceiveSwitch) != 0;
    out.midiChannel = display(part, PerformancePartParameter::MidiChannel);
    out.level = raw(part, PerformancePartParameter::PartLevel);
    out.pan = raw(part, PerformancePartParameter::PartPan);
    out.panText = displayText(part, PerformancePartParameter::PartPan);
    out.outputAssignRaw = raw(part, PerformancePartParameter::OutputAssign);
    out.outputAssignLabel = xp60performance::descriptor(PerformancePartParameter::OutputAssign)
                                .label(out.outputAssignRaw)
                                .value_or(std::string_view{});
    out.chorusSend = raw(part, PerformancePartParameter::ChorusSendLevel);
    out.reverbSend = raw(part, PerformancePartParameter::ReverbSendLevel);
    // Voice Reserve lives in Performance Common, one per Part, but it belongs
    // to the mixer strip as far as a musician is concerned.
    out.voiceReserve = raw(voiceReserveFor(part));
    return out;
}

Xp60Performance::PartRange Xp60Performance::range(PartIndex part) const
{
    PartRange out;
    out.lowerRaw = raw(part, PerformancePartParameter::KeyboardRangeLower);
    out.upperRaw = raw(part, PerformancePartParameter::KeyboardRangeUpper);
    out.lowerNote = displayText(part, PerformancePartParameter::KeyboardRangeLower);
    out.upperNote = displayText(part, PerformancePartParameter::KeyboardRangeUpper);
    out.octaveShift = display(part, PerformancePartParameter::OctaveShift);
    out.coarseTune = display(part, PerformancePartParameter::PartCoarseTune);
    out.fineTune = display(part, PerformancePartParameter::PartFineTune);
    return out;
}

int Xp60Performance::activePartCount() const noexcept
{
    int count = 0;
    for (const auto part : PartIndex::all()) {
        // A Part that receives nothing cannot sound, and one reserving no
        // voices can be starved by the others — but the second is a warning,
        // not a fact about this Performance, so only the switch counts here.
        if (raw(part, PerformancePartParameter::ReceiveSwitch) != 0) {
            ++count;
        }
    }
    return count;
}

std::string Xp60Performance::summary() const
{
    std::string text = name().displayText();
    text += " · " + std::to_string(activePartCount()) + " of "
        + std::to_string(PartIndex::kCount) + " parts receiving";
    text += " · tempo " + displayText(PerformanceCommonParameter::PerformanceTempo);
    text += " · "
        + std::string(xp60performance::descriptor(PerformanceCommonParameter::KeyboardMode)
                          .label(raw(PerformanceCommonParameter::KeyboardMode))
                          .value_or(std::string_view{}));
    return text;
}

bool operator==(const Xp60Performance& lhs, const Xp60Performance& rhs) noexcept
{
    return lhs.m_common == rhs.m_common && lhs.m_parts == rhs.m_parts;
}

} // namespace xp60studio::xpmodel
