#pragma once

#include "roland/RolandModelId.h"
#include "roland/RolandParseError.h"
#include "roland/RolandSysExMessage.h"
#include "roland/RolandTypes.h"

#include <optional>
#include <span>

namespace xp60studio::roland {

struct RolandDecodeResult
{
    std::optional<RolandSysExMessage> message;
    std::optional<RolandParseFailure> failure;

    [[nodiscard]] bool ok() const noexcept { return message.has_value(); }
};

// Quick structural checks that do not require model knowledge.
[[nodiscard]] bool isSysEx(ByteSpan bytes) noexcept;          // starts with F0
[[nodiscard]] bool isRolandSysEx(ByteSpan bytes) noexcept;    // F0 41 ...

// Decodes a complete SysEx message (F0 .. F7) into a validated RQ1/DT1.
//
// `knownModelIds` must list the model IDs the caller is prepared to accept.
// Because Roland model IDs vary in length, the command byte cannot be located
// without this knowledge; a message whose model bytes match none of them is
// reported as UnsupportedModel.
[[nodiscard]] RolandDecodeResult decodeRolandSysEx(ByteSpan bytes, std::span<const RolandModelId> knownModelIds);

// Convenience overload for a single expected model.
[[nodiscard]] RolandDecodeResult decodeRolandSysEx(ByteSpan bytes, const RolandModelId& knownModelId);

} // namespace xp60studio::roland
