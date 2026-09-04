#pragma once

#include "roland/RolandModelId.h"
#include "roland/RolandParseError.h"
#include "roland/RolandSysExMessage.h"
#include "roland/RolandTypes.h"
#include "xpmodel/MemoryImage.h"

#include <optional>
#include <span>
#include <vector>

namespace xp60studio::xpmodel {

// One message found in a byte stream (typically the contents of a .syx file).
struct SysExStreamItem
{
    std::size_t offset = 0;      // byte offset of the message in the stream
    roland::ByteVector raw;      // the complete message
    bool isSysEx = false;        // false for channel / system messages
    std::optional<roland::RolandSysExMessage> roland; // decoded RQ1/DT1
    std::optional<roland::RolandParseFailure> failure; // why a SysEx was not accepted
};

struct SysExStreamResult
{
    std::vector<SysExStreamItem> items;
    std::size_t strayBytes = 0;   // data bytes outside any message
    std::size_t abortedSysEx = 0; // unterminated SysEx interrupted by a status byte
    std::size_t dataSetCount = 0;
    std::size_t requestCount = 0;
    std::size_t nonRolandSysExCount = 0;
    std::size_t rejectedRolandCount = 0; // Roland SysEx that failed validation
    std::size_t otherMidiCount = 0;      // channel / system messages

    [[nodiscard]] bool isClean() const noexcept
    {
        return strayBytes == 0 && abortedSysEx == 0 && rejectedRolandCount == 0;
    }
};

// Splits a raw byte stream into messages and decodes the Roland ones.
// Nothing is dropped silently: every message and every anomaly is reported.
[[nodiscard]] SysExStreamResult parseSysExStream(roland::ByteSpan bytes, std::span<const roland::RolandModelId> knownModelIds);

// Lays every decoded DT1 of a parsed stream into a MemoryImage.
[[nodiscard]] MemoryImage imageFromStream(const SysExStreamResult& stream);

} // namespace xp60studio::xpmodel
