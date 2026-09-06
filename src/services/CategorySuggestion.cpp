#include "services/CategorySuggestion.h"

#include <algorithm>
#include <map>

namespace xp60studio::services {

using library::LibraryQuery;
using library::PatchSignature;

std::string CategorySuggestion::summary() const
{
    if (!available()) {
        return reason.empty() ? std::string("No category can be suggested.") : reason;
    }
    return "\"" + category + "\" \xE2\x80\x94 " + std::to_string(agreeingNeighbours) + " of "
        + std::to_string(consideredNeighbours) + " similar Patches in your library are filed under it.";
}

CategorySuggestion CategorySuggester::suggestFor(const library::LibraryDatabase& database,
                                                 std::int64_t id,
                                                 const CategorySuggestionOptions& options)
{
    CategorySuggestion suggestion;
    const auto entry = database.loadEntry(id);
    if (!entry) {
        suggestion.reason = "That Patch could not be read from the library.";
        return suggestion;
    }
    return suggest(database, PatchSignature::of(entry->patch()), id, options);
}

CategorySuggestion CategorySuggester::suggestFor(const library::LibraryDatabase& database,
                                                 const xpmodel::Xp60Patch& patch,
                                                 const CategorySuggestionOptions& options)
{
    return suggest(database, PatchSignature::of(patch), 0, options);
}

CategorySuggestion CategorySuggester::suggest(const library::LibraryDatabase& database,
                                              const PatchSignature& signature,
                                              std::int64_t excludedId,
                                              const CategorySuggestionOptions& options)
{
    CategorySuggestion suggestion;
    if (!database.isOpen()) {
        suggestion.reason = "The library is not open.";
        return suggestion;
    }

    // Only Patches the user has actually filed can teach anything. Everything
    // else in the library is as uncategorised as the Patch being asked about.
    const auto categories = database.categoriesInUse();
    if (categories.empty()) {
        suggestion.reason = "Nothing in your library is categorised yet, so there is nothing to "
                            "suggest from.";
        return suggestion;
    }

    struct Neighbour
    {
        std::int64_t id = 0;
        std::string name;
        std::string category;
        int equal = 0;
        int percent = 0;
    };
    std::vector<Neighbour> neighbours;
    int scanned = 0;

    const int compared = signature.parameterCount();
    const int minimumEqual = compared == 0
        ? 0
        : static_cast<int>((static_cast<long long>(options.minimumPercent) * compared + 99) / 100);

    bool budgetSpent = false;
    for (const auto& category : categories) {
        if (category.empty() || budgetSpent) {
            continue;
        }
        LibraryQuery query;
        query.category = category;
        for (const auto& record : database.search(query)) {
            if (record.id == excludedId) {
                continue;
            }
            if (options.maximumEntries > 0 && scanned >= options.maximumEntries) {
                budgetSpent = true;
                break;
            }
            ++scanned;
            const auto entry = database.loadEntry(record.id);
            if (!entry) {
                continue;
            }
            const auto other = PatchSignature::of(entry->patch());
            if (!PatchSignature::atLeast(signature, other, minimumEqual)) {
                continue;
            }
            const int equal = PatchSignature::equalCount(signature, other);
            const int rounded = compared == 0
                ? 100
                : static_cast<int>((static_cast<long long>(equal) * 200 + compared) / (2LL * compared));
            neighbours.push_back(Neighbour{record.id, record.name, category, equal,
                                           equal == compared ? 100 : std::min(rounded, 99)});
        }
    }

    if (neighbours.empty()) {
        suggestion.reason = "No categorised Patch in your library is at least "
            + std::to_string(options.minimumPercent) + "% like this one.";
        return suggestion;
    }

    std::stable_sort(neighbours.begin(), neighbours.end(),
                     [](const Neighbour& a, const Neighbour& b) { return a.equal > b.equal; });
    if (options.neighbourCount > 0
        && neighbours.size() > static_cast<std::size_t>(options.neighbourCount)) {
        neighbours.resize(static_cast<std::size_t>(options.neighbourCount));
    }

    // One Patch, one vote. Weighting a vote by how similar the neighbour is
    // would be a claim about how much closer counts, which nothing here can
    // measure; equal votes are a fact about the library instead.
    std::map<std::string, int> votes;
    for (const auto& neighbour : neighbours) {
        ++votes[neighbour.category];
    }
    const auto winner = std::max_element(
        votes.begin(), votes.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    const int cast = static_cast<int>(neighbours.size());
    const int agreed = winner->second;
    const int share = cast == 0 ? 0 : (agreed * 100 + cast / 2) / cast;

    suggestion.consideredNeighbours = cast;
    suggestion.evidence.reserve(neighbours.size());
    for (const auto& neighbour : neighbours) {
        suggestion.evidence.push_back(
            CategoryEvidence{neighbour.id, neighbour.name, neighbour.category, neighbour.percent});
    }

    // A tie is a disagreement, not a coin toss.
    const bool tied = std::count_if(votes.begin(), votes.end(), [agreed](const auto& entry) {
                          return entry.second == agreed;
                      }) > 1;
    if (tied) {
        suggestion.reason = "The Patches most like this one are filed under more than one category, "
                            "with no clear winner.";
        return suggestion;
    }
    if (share < options.minimumAgreementPercent) {
        suggestion.reason = "Only " + std::to_string(agreed) + " of " + std::to_string(cast)
            + " similar Patches share a category, which is too little agreement to suggest one.";
        return suggestion;
    }

    suggestion.category = winner->first;
    suggestion.agreeingNeighbours = agreed;
    suggestion.confidencePercent = share;
    suggestion.reason = suggestion.summary();
    return suggestion;
}

} // namespace xp60studio::services
