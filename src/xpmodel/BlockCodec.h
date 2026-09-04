#pragma once

#include "roland/RolandTypes.h"
#include "xpmodel/ParameterTable.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xp60studio::xpmodel {

// Decoded values of one block plus the original bytes.
//
// The original bytes are kept so that reserved / not-yet-described regions
// survive a decode -> encode round trip unchanged. `encode(decode(b)) == b`
// holds for every block whose parameter bytes are structurally valid.
class BlockValues
{
public:
    BlockValues(const ParameterTable& table, roland::ByteVector originalBytes, std::vector<int> raw);

    [[nodiscard]] const ParameterTable& table() const noexcept { return *m_table; }
    [[nodiscard]] const roland::ByteVector& originalBytes() const noexcept { return m_originalBytes; }

    [[nodiscard]] int rawAt(std::size_t index) const { return m_raw.at(index); }
    [[nodiscard]] std::optional<int> raw(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<int> display(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<std::string_view> label(std::string_view id) const noexcept;

    // Sets a raw value; false when the id is unknown or the value is outside
    // the documented range. Values are never clamped silently.
    bool setRaw(std::string_view id, int raw) noexcept;
    bool setDisplay(std::string_view id, int display) noexcept;
    bool setRawAt(std::size_t index, int raw) noexcept;

    // Text spanning consecutive Ascii parameters with the given id prefix
    // (e.g. "common.name."), trimmed of trailing spaces.
    [[nodiscard]] std::string text(std::string_view idPrefix) const;

    [[nodiscard]] std::span<const int> rawValues() const noexcept { return m_raw; }

    // Two BlockValues are equal when they describe the same table and would
    // encode to identical bytes (parameter values and reserved bytes alike).
    friend bool operator==(const BlockValues& lhs, const BlockValues& rhs) noexcept;

private:
    const ParameterTable* m_table;
    roland::ByteVector m_originalBytes;
    std::vector<int> m_raw;
};

enum class BlockIssueKind {
    SizeMismatch,      // error: bytes.size() != table.blockSize()
    DataByteBit7,      // error: a byte has bit 7 set
    InvalidNibbleByte, // error: a nibble byte has bits 4-6 set (cannot round-trip)
    OutOfRange,        // warning: raw value outside the documented range (kept verbatim)
    TableInvalid,      // error: the table itself failed validation
};

[[nodiscard]] std::string_view blockIssueKindName(BlockIssueKind kind) noexcept;
[[nodiscard]] bool isBlockIssueError(BlockIssueKind kind) noexcept;

struct BlockIssue
{
    BlockIssueKind kind;
    std::string parameterId; // empty for whole-block issues
    std::uint32_t offset = 0;
    int value = 0;
    std::string detail;
};

struct BlockDecodeResult
{
    std::optional<BlockValues> values; // present unless an error occurred
    std::vector<BlockIssue> issues;

    [[nodiscard]] bool ok() const noexcept { return values.has_value(); }
    [[nodiscard]] bool hasWarnings() const noexcept;
    [[nodiscard]] std::size_t errorCount() const noexcept;
};

struct BlockCodec
{
    [[nodiscard]] static BlockDecodeResult decode(const ParameterTable& table, roland::ByteSpan bytes);
    // Writes every parameter over a copy of the original bytes; reserved
    // ranges come through untouched.
    [[nodiscard]] static roland::ByteVector encode(const BlockValues& values);

    // Raw helpers exposed for tests and for tools that inspect single fields.
    [[nodiscard]] static int readRaw(const ParameterDescriptor& parameter, roland::ByteSpan block) noexcept;
    static void writeRaw(const ParameterDescriptor& parameter, int raw, roland::ByteVector& block) noexcept;
};

} // namespace xp60studio::xpmodel
