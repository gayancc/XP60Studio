#pragma once

#include "roland/RolandTypes.h"
#include "xpmodel/Xp60Patch.h"

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace xp60studio::library {

// Exact parameter fingerprint of a Patch.
//
// This is DERIVED metadata, never Roland hardware data (ARCHITECTURE.md §13).
// It exists so a library holding thousands of patches can find exact
// duplicates without comparing every pair byte by byte; it is an index, not an
// answer. Two entries whose fingerprints match must still be confirmed by
// comparing their bytes before anything is called a duplicate.
//
// What is hashed is the Patch's five encoded blocks — Patch Common and Tone
// 1..4 as `Xp60PatchCodec::blockBytes` produces them — and nothing else. That
// is exactly the parameter content the Parameter Address Map describes, so two
// patches fingerprint alike when their parameters are identical, regardless of
// the device ID, message chunking, checksums or address the SysEx that
// delivered them happened to use. Nothing is normalised: bytes the tables do
// not describe and out-of-range values are hashed exactly as they are stored.
//
// The payload is domain-separated and versioned (`kVersion`), so a future
// change to what is hashed produces different digests instead of silently
// colliding with fingerprints computed by an earlier build.
class PatchFingerprint
{
public:
    static constexpr std::size_t kByteCount = 32; // SHA-256
    static constexpr std::string_view kVersion = "XP60STUDIO-PATCH-FINGERPRINT-1";
    using Bytes = std::array<roland::Byte, kByteCount>;

    PatchFingerprint() noexcept = default; // all zero: "not computed"

    [[nodiscard]] static PatchFingerprint of(const xpmodel::Xp60Patch& patch);
    // Round-trips `toHexString()`; nullopt when the text is not 64 hex digits.
    [[nodiscard]] static std::optional<PatchFingerprint> fromHexString(std::string_view text) noexcept;

    [[nodiscard]] const Bytes& bytes() const noexcept { return m_bytes; }
    [[nodiscard]] std::string toHexString() const;
    // First 8 hex digits, for logs and diagnostics. Never use as an identity.
    [[nodiscard]] std::string toShortString() const;
    [[nodiscard]] bool isNull() const noexcept;

    friend auto operator<=>(const PatchFingerprint&, const PatchFingerprint&) noexcept = default;

private:
    Bytes m_bytes{};
};

} // namespace xp60studio::library
