#include "xpmodel/BlockCodec.h"

#include "roland/HexFormat.h"

#include <algorithm>

namespace xp60studio::xpmodel {

// ---------------------------------------------------------------------------
// BlockValues
// ---------------------------------------------------------------------------

BlockValues::BlockValues(const ParameterTable& table, roland::ByteVector originalBytes, std::vector<int> raw)
    : m_table(&table)
    , m_originalBytes(std::move(originalBytes))
    , m_raw(std::move(raw))
{
}

std::optional<int> BlockValues::raw(std::string_view id) const noexcept
{
    const auto index = m_table->indexOf(id);
    return index ? std::optional<int>(m_raw[*index]) : std::nullopt;
}

std::optional<int> BlockValues::display(std::string_view id) const noexcept
{
    const auto index = m_table->indexOf(id);
    if (!index) {
        return std::nullopt;
    }
    return m_table->parameters()[*index].toDisplay(m_raw[*index]);
}

std::optional<std::string_view> BlockValues::label(std::string_view id) const noexcept
{
    const auto index = m_table->indexOf(id);
    if (!index) {
        return std::nullopt;
    }
    return m_table->parameters()[*index].label(m_raw[*index]);
}

bool BlockValues::setRawAt(std::size_t index, int raw) noexcept
{
    if (index >= m_raw.size()) {
        return false;
    }
    const auto& parameter = m_table->parameters()[index];
    if (!parameter.isRawInRange(raw)) {
        return false;
    }
    m_raw[index] = raw;
    return true;
}

bool BlockValues::setRaw(std::string_view id, int raw) noexcept
{
    const auto index = m_table->indexOf(id);
    return index ? setRawAt(*index, raw) : false;
}

bool BlockValues::setDisplay(std::string_view id, int display) noexcept
{
    const auto index = m_table->indexOf(id);
    if (!index) {
        return false;
    }
    return setRawAt(*index, m_table->parameters()[*index].fromDisplay(display));
}

std::string BlockValues::text(std::string_view idPrefix) const
{
    std::string out;
    const auto parameters = m_table->parameters();
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        const auto& parameter = parameters[i];
        if (parameter.isText() && parameter.id.substr(0, idPrefix.size()) == idPrefix) {
            out.push_back(static_cast<char>(m_raw[i]));
        }
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

bool operator==(const BlockValues& lhs, const BlockValues& rhs) noexcept
{
    if (lhs.m_table != rhs.m_table || lhs.m_raw != rhs.m_raw) {
        return false;
    }
    return BlockCodec::encode(lhs) == BlockCodec::encode(rhs);
}

// ---------------------------------------------------------------------------
// Issues
// ---------------------------------------------------------------------------

std::string_view blockIssueKindName(BlockIssueKind kind) noexcept
{
    switch (kind) {
    case BlockIssueKind::SizeMismatch:
        return "SizeMismatch";
    case BlockIssueKind::DataByteBit7:
        return "DataByteBit7";
    case BlockIssueKind::InvalidNibbleByte:
        return "InvalidNibbleByte";
    case BlockIssueKind::OutOfRange:
        return "OutOfRange";
    case BlockIssueKind::TableInvalid:
        return "TableInvalid";
    }
    return "Unknown";
}

bool isBlockIssueError(BlockIssueKind kind) noexcept
{
    return kind != BlockIssueKind::OutOfRange;
}

bool BlockDecodeResult::hasWarnings() const noexcept
{
    return std::any_of(issues.begin(), issues.end(), [](const BlockIssue& i) { return !isBlockIssueError(i.kind); });
}

std::size_t BlockDecodeResult::errorCount() const noexcept
{
    return static_cast<std::size_t>(
        std::count_if(issues.begin(), issues.end(), [](const BlockIssue& i) { return isBlockIssueError(i.kind); }));
}

// ---------------------------------------------------------------------------
// Codec
// ---------------------------------------------------------------------------

int BlockCodec::readRaw(const ParameterDescriptor& parameter, roland::ByteSpan block) noexcept
{
    switch (parameter.encoding) {
    case ParameterEncoding::SevenBit:
    case ParameterEncoding::Ascii:
        return block[parameter.offset] & 0x7F;
    case ParameterEncoding::Nibble: {
        int value = 0;
        for (std::uint8_t i = 0; i < parameter.byteCount; ++i) {
            value = (value << 4) | (block[parameter.offset + i] & 0x0F);
        }
        return value;
    }
    }
    return 0;
}

void BlockCodec::writeRaw(const ParameterDescriptor& parameter, int raw, roland::ByteVector& block) noexcept
{
    switch (parameter.encoding) {
    case ParameterEncoding::SevenBit:
    case ParameterEncoding::Ascii:
        block[parameter.offset] = static_cast<roland::Byte>(raw & 0x7F);
        return;
    case ParameterEncoding::Nibble:
        for (std::uint8_t i = 0; i < parameter.byteCount; ++i) {
            const int shift = 4 * (parameter.byteCount - 1 - i);
            block[parameter.offset + i] = static_cast<roland::Byte>((raw >> shift) & 0x0F);
        }
        return;
    }
}

BlockDecodeResult BlockCodec::decode(const ParameterTable& table, roland::ByteSpan bytes)
{
    BlockDecodeResult result;

    const auto tableIssues = table.validate();
    if (!tableIssues.empty()) {
        for (const auto& issue : tableIssues) {
            result.issues.push_back({BlockIssueKind::TableInvalid, {}, 0, 0,
                                     std::string(tableIssueKindName(issue.kind)) + ": " + issue.detail});
        }
        return result;
    }

    if (bytes.size() != table.blockSize()) {
        result.issues.push_back({BlockIssueKind::SizeMismatch, {}, 0, static_cast<int>(bytes.size()),
                                 std::string(table.blockName()) + " expects " + std::to_string(table.blockSize())
                                     + " bytes, got " + std::to_string(bytes.size())});
        return result;
    }

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (!roland::isDataByte(bytes[i])) {
            const auto* owner = table.atOffset(static_cast<std::uint32_t>(i));
            result.issues.push_back({BlockIssueKind::DataByteBit7, owner ? std::string(owner->id) : std::string(),
                                     static_cast<std::uint32_t>(i), bytes[i],
                                     "byte " + std::to_string(i) + " is " + roland::toHex(bytes[i])});
        }
    }
    if (result.errorCount() != 0) {
        return result;
    }

    std::vector<int> raw;
    raw.reserve(table.size());
    for (const auto& parameter : table.parameters()) {
        if (parameter.encoding == ParameterEncoding::Nibble) {
            for (std::uint8_t i = 0; i < parameter.byteCount; ++i) {
                const roland::Byte b = bytes[parameter.offset + i];
                if ((b & 0x70) != 0) {
                    result.issues.push_back({BlockIssueKind::InvalidNibbleByte, std::string(parameter.id),
                                             parameter.offset + i, b,
                                             std::string(parameter.id) + ": nibble byte " + roland::toHex(b)
                                                 + " has bits 4-6 set"});
                }
            }
        }
        const int value = readRaw(parameter, bytes);
        if (!parameter.isRawInRange(value)) {
            result.issues.push_back({BlockIssueKind::OutOfRange, std::string(parameter.id), parameter.offset, value,
                                     std::string(parameter.id) + " = " + std::to_string(value) + " outside "
                                         + std::to_string(parameter.rawMin) + ".." + std::to_string(parameter.rawMax)
                                         + " (kept verbatim)"});
        }
        raw.push_back(value);
    }
    if (result.errorCount() != 0) {
        return result;
    }

    result.values.emplace(table, roland::ByteVector(bytes.begin(), bytes.end()), std::move(raw));
    return result;
}

roland::ByteVector BlockCodec::encode(const BlockValues& values)
{
    roland::ByteVector out = values.originalBytes();
    const auto parameters = values.table().parameters();
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        writeRaw(parameters[i], values.rawAt(i), out);
    }
    return out;
}

} // namespace xp60studio::xpmodel
