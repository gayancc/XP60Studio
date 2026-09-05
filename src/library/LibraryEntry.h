#pragma once

#include "library/PatchFingerprint.h"
#include "library/PatchProvenance.h"
#include "roland/RolandTypes.h"
#include "xpmodel/PatchName.h"
#include "xpmodel/Xp60Patch.h"

#include <optional>
#include <string>
#include <vector>

namespace xp60studio::library {

// User-owned metadata about a Patch.
//
// Kept in its own type, and stored separately from the Roland data, so that
// "the user called this a pad and gave it four stars" can never be confused
// with anything the instrument said (AGENTS.md: derived metadata must remain
// separate from Roland hardware data). Editing any of this leaves the Patch,
// its original SysEx and its fingerprint untouched.
struct PatchUserMetadata
{
    static constexpr int kMinRating = 0; // 0 means "not rated"
    static constexpr int kMaxRating = 5;

    bool favourite = false;
    int rating = kMinRating;
    // The user's own category, free text. This is NOT Roland's Patch Category:
    // the XP-60 Parameter Address Map defines no category byte, so inventing
    // one would be inventing a hardware fact.
    std::string category;
    std::vector<std::string> tags;
    std::string notes;

    [[nodiscard]] static bool isValidRating(int value) noexcept
    {
        return value >= kMinRating && value <= kMaxRating;
    }
    [[nodiscard]] bool hasTag(std::string_view tag) const noexcept;
    // Adds `tag` if absent. Returns false for an empty tag or a duplicate;
    // comparison is exact, so tags are never silently case-folded or trimmed
    // into each other.
    bool addTag(std::string tag);
    bool removeTag(std::string_view tag);

    friend bool operator==(const PatchUserMetadata&, const PatchUserMetadata&) noexcept = default;
};

// One Patch in the library.
//
// The entry keeps three kinds of data apart on purpose:
//
//   canonical  the decoded Patch and the original SysEx bytes it arrived in;
//   provenance where it came from, recorded once at import;
//   derived    the fingerprint, and the user's own metadata.
//
// `originalSysEx()` is the exact bytes of the messages that carried this Patch,
// in stream order, complete with their device IDs, chunking and checksums.
// They are preserved so an import is always reversible and any later
// disagreement between the model and the source can be investigated against
// what actually arrived — never re-encoded from the model (ARCHITECTURE.md
// §12: "raw original SysEx preservation").
class LibraryEntry
{
public:
    LibraryEntry(xpmodel::Xp60Patch patch, roland::ByteVector originalSysEx, PatchProvenance provenance);

    [[nodiscard]] const xpmodel::Xp60Patch& patch() const noexcept { return m_patch; }
    [[nodiscard]] const roland::ByteVector& originalSysEx() const noexcept { return m_originalSysEx; }
    [[nodiscard]] const PatchProvenance& provenance() const noexcept { return m_provenance; }
    [[nodiscard]] const PatchFingerprint& fingerprint() const noexcept { return m_fingerprint; }

    [[nodiscard]] const PatchUserMetadata& userMetadata() const noexcept { return m_userMetadata; }
    [[nodiscard]] PatchUserMetadata& userMetadata() noexcept { return m_userMetadata; }

    [[nodiscard]] xpmodel::PatchName name() const { return m_patch.name(); }
    // Trailing padding removed; the bytes themselves are never altered.
    [[nodiscard]] std::string displayName() const { return m_patch.name().displayText(); }

    // Whether two entries hold the same parameters. The fingerprint is only
    // the fast path: a match is confirmed against the encoded blocks, so a
    // hash collision can never report two different patches as duplicates.
    [[nodiscard]] bool hasSameParameters(const LibraryEntry& other) const;

private:
    xpmodel::Xp60Patch m_patch;
    roland::ByteVector m_originalSysEx;
    PatchProvenance m_provenance;
    PatchFingerprint m_fingerprint;
    PatchUserMetadata m_userMetadata;
};

} // namespace xp60studio::library
