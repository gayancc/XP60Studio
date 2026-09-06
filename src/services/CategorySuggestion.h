#pragma once

#include "library/LibraryDatabase.h"
#include "library/PatchSimilarity.h"
#include "xpmodel/Xp60Patch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace xp60studio::services {

// One already-categorised Patch that a suggestion was drawn from.
struct CategoryEvidence
{
    std::int64_t id = 0;
    std::string name;
    std::string category;
    int percent = 0;  // how alike it is to the Patch being categorised
};

// What the library can say about which category a Patch belongs to.
//
// `category` is empty when nothing can honestly be suggested, and `reason` then
// says why in words a user can act on. "Unknown" is a first-class answer here:
// a wrong category quietly applied to a library is worse than no category, and
// the user is the only authority on what their own labels mean.
struct CategorySuggestion
{
    std::string category;       // empty when unavailable
    int confidencePercent = 0;  // share of the voting neighbours that agreed
    int agreeingNeighbours = 0;
    int consideredNeighbours = 0;
    // The neighbours the vote was taken over, most similar first, including the
    // ones that disagreed. A suggestion that cannot be inspected is a guess
    // wearing a number, so the dissent is reported alongside the winner.
    std::vector<CategoryEvidence> evidence;
    std::string reason;  // why unavailable, or how the winner was reached

    [[nodiscard]] bool available() const noexcept { return !category.empty(); }
    [[nodiscard]] std::string summary() const;
};

struct CategorySuggestionOptions
{
    // How many of the most similar categorised Patches vote.
    int neighbourCount = 5;
    // A Patch less alike than this is not evidence of anything. The default is
    // deliberately high: this is "your library already contains something very
    // like this", not a general classifier.
    int minimumPercent = 85;
    // The winner must carry at least this share of the votes cast, or the
    // neighbours are judged to disagree and nothing is suggested.
    int minimumAgreementPercent = 60;
    // Bound on the categorised entries scanned, for the same reason the
    // duplicate sweep has one.
    int maximumEntries = 5000;
};

// Suggests a category for a Patch from the categories the user has already
// applied to Patches like it.
//
// This is the only form of automatic categorization this project can honestly
// offer. The XP-60 Parameter Address Map defines no category byte, so there is
// no hardware fact to read. A Patch's name is not evidence either — "Strings 3"
// is a label somebody typed, and reading categories out of names would be
// inventing data from text, which is exactly what this codebase refuses
// elsewhere. Nor can structure alone decide it: a two-Tone layer is as easily a
// pad as a bass.
//
// What is left is genuine and useful: the user's own labels, transferred along
// measured similarity. If four of the five Patches most like this one are
// filed under "Pad", that is a fact about their library, and it is reported as
// such — with the neighbours it came from, the dissenters included, so the
// suggestion can be checked rather than trusted.
//
// Nothing is written. The suggestion is returned; applying it is the user's
// act, and `LibraryDatabase::updateUserMetadata` is where that happens.
class CategorySuggester
{
public:
    // For a Patch already in the library. The entry itself never votes, and
    // neither does any Patch sharing its id.
    [[nodiscard]] static CategorySuggestion suggestFor(const library::LibraryDatabase& database,
                                                       std::int64_t id,
                                                       const CategorySuggestionOptions& options = {});

    // For a Patch that is not in the library yet — an import being reviewed, or
    // the editor's working copy.
    [[nodiscard]] static CategorySuggestion suggestFor(const library::LibraryDatabase& database,
                                                       const xpmodel::Xp60Patch& patch,
                                                       const CategorySuggestionOptions& options = {});

private:
    [[nodiscard]] static CategorySuggestion suggest(const library::LibraryDatabase& database,
                                                    const library::PatchSignature& signature,
                                                    std::int64_t excludedId,
                                                    const CategorySuggestionOptions& options);
};

} // namespace xp60studio::services
