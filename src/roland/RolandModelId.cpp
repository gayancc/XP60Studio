#include "roland/RolandModelId.h"

#include "roland/HexFormat.h"

#include <algorithm>
#include <stdexcept>

namespace xp60studio::roland {

RolandModelId::RolandModelId(std::initializer_list<Byte> bytes)
{
    const auto result = fromBytes(ByteSpan(bytes.begin(), bytes.size()));
    if (!result) {
        throw std::invalid_argument("invalid Roland model ID bytes");
    }
    *this = *result;
}

std::optional<RolandModelId> RolandModelId::fromBytes(ByteSpan bytes) noexcept
{
    if (bytes.empty() || bytes.size() > kMaxBytes) {
        return std::nullopt;
    }
    if (!std::all_of(bytes.begin(), bytes.end(), [](Byte b) { return isDataByte(b); })) {
        return std::nullopt;
    }
    RolandModelId id;
    std::copy(bytes.begin(), bytes.end(), id.m_bytes.begin());
    id.m_length = bytes.size();
    return id;
}

std::string RolandModelId::toHexString() const
{
    return toHex(bytes());
}

bool operator==(const RolandModelId& lhs, const RolandModelId& rhs) noexcept
{
    return lhs.m_length == rhs.m_length
        && std::equal(lhs.m_bytes.begin(), lhs.m_bytes.begin() + static_cast<std::ptrdiff_t>(lhs.m_length),
                      rhs.m_bytes.begin());
}

} // namespace xp60studio::roland
