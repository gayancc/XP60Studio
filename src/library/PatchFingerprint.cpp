#include "library/PatchFingerprint.h"

#include "xpmodel/Xp60PatchCodec.h"

#include <QByteArray>
#include <QCryptographicHash>

#include <algorithm>
#include <cctype>

namespace xp60studio::library {
namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

void appendLittleEndian(QByteArray& out, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        out.append(static_cast<char>((value >> shift) & 0xFF));
    }
}

std::optional<int> hexValue(char c) noexcept
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower >= 'a' && lower <= 'f') {
        return 10 + (lower - 'a');
    }
    return std::nullopt;
}

} // namespace

PatchFingerprint PatchFingerprint::of(const xpmodel::Xp60Patch& patch)
{
    const auto blocks = xpmodel::Xp60PatchCodec::blockBytes(patch);

    // Domain separation first, then each block's length before its bytes, so
    // no two different block layouts can produce the same byte sequence.
    QByteArray payload;
    payload.append(kVersion.data(), static_cast<qsizetype>(kVersion.size()));
    payload.append('\0');
    appendLittleEndian(payload, static_cast<std::uint32_t>(blocks.size()));
    for (const auto& block : blocks) {
        appendLittleEndian(payload, static_cast<std::uint32_t>(block.size()));
        payload.append(reinterpret_cast<const char*>(block.data()), static_cast<qsizetype>(block.size()));
    }

    const QByteArray digest = QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
    PatchFingerprint fingerprint;
    Q_ASSERT(static_cast<std::size_t>(digest.size()) == kByteCount);
    std::copy_n(reinterpret_cast<const roland::Byte*>(digest.constData()), kByteCount, fingerprint.m_bytes.begin());
    return fingerprint;
}

std::optional<PatchFingerprint> PatchFingerprint::fromHexString(std::string_view text) noexcept
{
    if (text.size() != kByteCount * 2) {
        return std::nullopt;
    }
    PatchFingerprint fingerprint;
    for (std::size_t i = 0; i < kByteCount; ++i) {
        const auto high = hexValue(text[i * 2]);
        const auto low = hexValue(text[i * 2 + 1]);
        if (!high || !low) {
            return std::nullopt;
        }
        fingerprint.m_bytes[i] = static_cast<roland::Byte>((*high << 4) | *low);
    }
    return fingerprint;
}

std::string PatchFingerprint::toHexString() const
{
    std::string out;
    out.reserve(kByteCount * 2);
    for (const auto byte : m_bytes) {
        out.push_back(kHexDigits[byte >> 4]);
        out.push_back(kHexDigits[byte & 0x0F]);
    }
    return out;
}

std::string PatchFingerprint::toShortString() const
{
    return toHexString().substr(0, 8);
}

bool PatchFingerprint::isNull() const noexcept
{
    return std::all_of(m_bytes.begin(), m_bytes.end(), [](roland::Byte b) { return b == 0; });
}

} // namespace xp60studio::library
