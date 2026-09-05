#include "library/LibraryEntry.h"

#include <algorithm>
#include <utility>

namespace xp60studio::library {

bool PatchUserMetadata::hasTag(std::string_view tag) const noexcept
{
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

bool PatchUserMetadata::addTag(std::string tag)
{
    if (tag.empty() || hasTag(tag)) {
        return false;
    }
    tags.push_back(std::move(tag));
    return true;
}

bool PatchUserMetadata::removeTag(std::string_view tag)
{
    const auto it = std::find(tags.begin(), tags.end(), tag);
    if (it == tags.end()) {
        return false;
    }
    tags.erase(it);
    return true;
}

LibraryEntry::LibraryEntry(xpmodel::Xp60Patch patch, roland::ByteVector originalSysEx, PatchProvenance provenance)
    : m_patch(std::move(patch))
    , m_originalSysEx(std::move(originalSysEx))
    , m_provenance(std::move(provenance))
    , m_fingerprint(PatchFingerprint::of(m_patch))
{
}

bool LibraryEntry::hasSameParameters(const LibraryEntry& other) const
{
    // The fingerprint only decides when to bother comparing. The answer comes
    // from the parameters themselves, so a collision cannot create a duplicate.
    return m_fingerprint == other.m_fingerprint && m_patch == other.m_patch;
}

} // namespace xp60studio::library
