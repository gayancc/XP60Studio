#pragma once

#include "library/LibraryDatabase.h"
#include "library/PatchSimilarity.h"

#include <QString>

#include <cstdint>
#include <string>
#include <vector>

namespace xp60studio::services {

// One pair of library entries that are alike.
struct SimilarPair
{
    std::int64_t leftId = 0;
    std::int64_t rightId = 0;
    std::string leftName;
    std::string rightName;
    int equalParameters = 0;
    int comparedParameters = 0;
    // The same 0..100 number PatchSimilarity::percent() reports, including its
    // refusal to round up to 100 for Patches that differ.
    int percent = 0;
    bool identicalSound = false;
    bool nameEqual = false;

    [[nodiscard]] std::string summary() const;
};

// A group of entries connected by similarity.
//
// Connected, not "all alike": membership is transitive, so A~B and B~C put A
// and C in one group even if A and C are further apart than the threshold.
// That is single-linkage clustering and it chains — three Patches each one
// tweak from the last form a group whose extremes are three tweaks apart.
//
// Chaining is not hidden behind an average. `minimumPercent` is the least
// similar pair actually compared inside the group, so a group that chained says
// so in the one number that would reveal it, and the user decides whether the
// grouping is useful. Nothing is merged or deleted on the strength of it.
struct SimilarityCluster
{
    std::vector<std::int64_t> ids;   // ascending
    std::vector<std::string> names;  // parallel to ids
    int minimumPercent = 100;        // least similar compared pair in the group
    // Every entry in the group has the same sound; only names may differ.
    bool allIdenticalSound = false;

    [[nodiscard]] std::size_t size() const noexcept { return ids.size(); }
    [[nodiscard]] std::string summary() const;
};

// What a sweep looked at and what it found.
struct LibraryDuplicateReport
{
    int entriesConsidered = 0;   // rows the query returned
    int entriesAnalysed = 0;     // rows whose Patch actually decoded
    int entriesUndecodable = 0;  // rows whose stored bytes would not decode
    std::int64_t pairsCompared = 0;

    // Groups whose members are byte-identical in every sound parameter. These
    // are the exact duplicates; the fingerprint finds most of them, but a group
    // here can also hold Patches that differ only in name, which it cannot.
    std::vector<SimilarityCluster> identicalGroups;
    // Groups above the threshold that are not identical: the near-duplicates.
    std::vector<SimilarityCluster> nearDuplicateGroups;
    // Every pair at or above the threshold, most similar first. The groups are
    // built from these, and this is what a "why is this a duplicate?" view
    // needs.
    std::vector<SimilarPair> pairs;

    // True when the sweep stopped early against its budget, in which case the
    // findings are real but incomplete. Never silently true: the caller is
    // expected to say so.
    bool truncated = false;
    QString truncationReason;

    [[nodiscard]] std::string summary() const;
};

struct LibraryDuplicateOptions
{
    // Report a pair at or above this percentage. 100 finds only Patches that
    // are the same sound; the default looks for the near-misses a decade of
    // resaving leaves behind.
    int minimumPercent = 95;
    // Which entries to sweep. The default is the whole library.
    library::LibraryQuery query{};
    // Refuse rather than run for hours. A sweep is quadratic in the number of
    // entries, so a limit on entries is the honest control; exceeding it
    // truncates the *input* and says so, rather than truncating the findings
    // and looking complete.
    int maximumEntries = 5000;
};

// Finds the duplicates and near-duplicates in a library.
//
// `LibraryDatabase::findDuplicatesOf` answers "is this exact Patch already
// here?" from the fingerprint alone, without decoding anything. That is the
// right tool at import time and it cannot see a near-duplicate, nor a Patch
// saved twice under two names — the fingerprint covers the name bytes, so a
// rename makes two rows that are the same sound look unrelated.
//
// This sweeps instead: every entry is decoded once into a `PatchSignature`, and
// every pair is compared as an integer scan with an early exit. The comparison
// is `PatchSimilarity`'s, unweighted and per parameter, so a percentage here
// and a percentage on the Compare screen are the same number.
//
// Nothing is merged, deleted, or rewritten. The report is derived metadata
// about the library, handed to the user to act on.
//
// Synchronous and free of timers and signals; a large sweep belongs on a worker
// thread, like the rest of the library services.
class LibraryDuplicateAnalysis
{
public:
    [[nodiscard]] static LibraryDuplicateReport analyse(const library::LibraryDatabase& database,
                                                        const LibraryDuplicateOptions& options);
};

} // namespace xp60studio::services
