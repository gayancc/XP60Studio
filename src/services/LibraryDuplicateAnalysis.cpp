#include "services/LibraryDuplicateAnalysis.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <unordered_map>

namespace xp60studio::services {

using library::PatchSignature;

namespace {

std::string joinNames(const std::vector<std::string>& names)
{
    std::string out;
    for (const auto& name : names) {
        if (!out.empty()) {
            out += ", ";
        }
        out += name;
    }
    return out;
}

// Union-find over row indices: the groups are the connected components of the
// "at least this alike" relation.
class DisjointSets
{
public:
    explicit DisjointSets(std::size_t count) : m_parent(count)
    {
        std::iota(m_parent.begin(), m_parent.end(), std::size_t(0));
    }

    std::size_t find(std::size_t i)
    {
        while (m_parent[i] != i) {
            m_parent[i] = m_parent[m_parent[i]];
            i = m_parent[i];
        }
        return i;
    }

    void unite(std::size_t a, std::size_t b) { m_parent[find(a)] = find(b); }

private:
    std::vector<std::size_t> m_parent;
};

} // namespace

std::string SimilarPair::summary() const
{
    if (identicalSound && nameEqual) {
        return leftName + " and " + rightName + " are identical.";
    }
    if (identicalSound) {
        return leftName + " and " + rightName + " are the same sound under different names.";
    }
    return leftName + " and " + rightName + " are " + std::to_string(percent) + "% alike \xE2\x80\x94 "
        + std::to_string(comparedParameters - equalParameters) + " parameters differ.";
}

std::string SimilarityCluster::summary() const
{
    const std::string members = std::to_string(ids.size()) + " Patches";
    if (allIdenticalSound) {
        return members + ", all the same sound: " + joinNames(names);
    }
    return members + ", from " + std::to_string(minimumPercent) + "% alike: " + joinNames(names);
}

std::string LibraryDuplicateReport::summary() const
{
    if (entriesAnalysed == 0) {
        return "Nothing to analyse.";
    }
    std::string out = "Compared " + std::to_string(entriesAnalysed) + " Patches: "
        + std::to_string(identicalGroups.size())
        + (identicalGroups.size() == 1 ? " group of identical sounds, " : " groups of identical sounds, ")
        + std::to_string(nearDuplicateGroups.size())
        + (nearDuplicateGroups.size() == 1 ? " group of near-duplicates." : " groups of near-duplicates.");
    if (entriesUndecodable > 0) {
        out += " " + std::to_string(entriesUndecodable)
            + (entriesUndecodable == 1 ? " entry could not be decoded and was left out."
                                       : " entries could not be decoded and were left out.");
    }
    if (truncated) {
        out += " The sweep was incomplete.";
    }
    return out;
}

LibraryDuplicateReport LibraryDuplicateAnalysis::analyse(const library::LibraryDatabase& database,
                                                         const LibraryDuplicateOptions& options)
{
    LibraryDuplicateReport report;
    if (!database.isOpen()) {
        report.truncated = true;
        report.truncationReason = QStringLiteral("The library is not open.");
        return report;
    }

    auto records = database.search(options.query);
    report.entriesConsidered = static_cast<int>(records.size());
    if (options.maximumEntries > 0 && report.entriesConsidered > options.maximumEntries) {
        // Truncate the input rather than the findings: a report over the first
        // N entries is a true statement about those entries, whereas a report
        // that stopped comparing partway through would look complete and not
        // be.
        records.resize(static_cast<std::size_t>(options.maximumEntries));
        report.truncated = true;
        report.truncationReason
            = QStringLiteral("Analysed the first %1 of %2 entries; a sweep grows with the "
                             "square of the library.")
                  .arg(options.maximumEntries)
                  .arg(report.entriesConsidered);
    }

    struct Row
    {
        std::int64_t id = 0;
        std::string name;
        PatchSignature signature;
    };
    std::vector<Row> rows;
    rows.reserve(records.size());
    for (const auto& record : records) {
        const auto entry = database.loadEntry(record.id);
        if (!entry) {
            ++report.entriesUndecodable;
            continue;
        }
        rows.push_back(Row{record.id, entry->displayName(), PatchSignature::of(entry->patch())});
    }
    report.entriesAnalysed = static_cast<int>(rows.size());
    if (rows.size() < 2) {
        return report;
    }

    const int compared = rows.front().signature.parameterCount();
    // Ceil, so a threshold is never met by a Patch that falls just short of it.
    const int minimumEqual = compared == 0
        ? 0
        : static_cast<int>((static_cast<long long>(options.minimumPercent) * compared + 99) / 100);

    DisjointSets sets(rows.size());
    std::vector<bool> chained(rows.size(), false);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        for (std::size_t j = i + 1; j < rows.size(); ++j) {
            ++report.pairsCompared;
            if (!PatchSignature::atLeast(rows[i].signature, rows[j].signature, minimumEqual)) {
                continue;
            }
            const int equal = PatchSignature::equalCount(rows[i].signature, rows[j].signature);

            SimilarPair pair;
            pair.leftId = rows[i].id;
            pair.rightId = rows[j].id;
            pair.leftName = rows[i].name;
            pair.rightName = rows[j].name;
            pair.equalParameters = equal;
            pair.comparedParameters = compared;
            pair.identicalSound = equal == compared;
            pair.nameEqual = rows[i].name == rows[j].name;
            // Same rule as PatchSimilarity::percent(): 100 means identical.
            const int rounded = compared == 0
                ? 100
                : static_cast<int>((static_cast<long long>(equal) * 200 + compared) / (2LL * compared));
            pair.percent = pair.identicalSound ? 100 : std::min(rounded, 99);
            report.pairs.push_back(std::move(pair));

            sets.unite(i, j);
            chained[i] = true;
            chained[j] = true;
        }
    }

    std::stable_sort(report.pairs.begin(), report.pairs.end(),
                     [](const SimilarPair& a, const SimilarPair& b) {
                         return a.equalParameters > b.equalParameters;
                     });

    // Build the groups from the components, and record the least similar pair
    // actually compared inside each so chaining stays visible.
    std::map<std::size_t, SimilarityCluster> clusters;
    std::map<std::size_t, bool> allIdentical;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!chained[i]) {
            continue;
        }
        const auto root = sets.find(i);
        auto& cluster = clusters[root];
        cluster.ids.push_back(rows[i].id);
        cluster.names.push_back(rows[i].name);
        if (!allIdentical.contains(root)) {
            allIdentical[root] = true;
        }
    }
    std::unordered_map<std::int64_t, std::size_t> indexOfId;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        indexOfId[rows[i].id] = i;
    }
    for (const auto& pair : report.pairs) {
        // Both endpoints are in the same component by construction.
        const auto root = sets.find(indexOfId[pair.leftId]);
        auto& cluster = clusters[root];
        cluster.minimumPercent = std::min(cluster.minimumPercent, pair.percent);
        if (!pair.identicalSound) {
            allIdentical[root] = false;
        }
    }

    for (auto& [root, cluster] : clusters) {
        cluster.allIdenticalSound = allIdentical[root];
        if (cluster.allIdenticalSound) {
            report.identicalGroups.push_back(std::move(cluster));
        } else {
            report.nearDuplicateGroups.push_back(std::move(cluster));
        }
    }
    const auto biggestFirst = [](const SimilarityCluster& a, const SimilarityCluster& b) {
        return a.ids.size() > b.ids.size();
    };
    std::stable_sort(report.identicalGroups.begin(), report.identicalGroups.end(), biggestFirst);
    std::stable_sort(report.nearDuplicateGroups.begin(), report.nearDuplicateGroups.end(), biggestFirst);
    return report;
}

} // namespace xp60studio::services
