#pragma once

#include "library/BankDraft.h"
#include "library/ExpansionProfile.h"
#include "library/LibraryEntry.h"
#include "library/PatchFingerprint.h"
#include "library/PatchProvenance.h"

#include <QString>

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace xp60studio::library {

// A row of the library, without the Patch itself.
//
// This is what a search returns and what a list model renders: a name, the
// user's own metadata, and enough provenance to say where the sound came from.
// The Patch and its bytes are fetched separately, so scrolling a library of
// thousands never decodes a Patch it is not going to show
// (ARCHITECTURE.md §11: large collections travel through Qt item models).
struct LibraryRecord
{
    std::int64_t id = 0;
    std::string name;          // display text, trailing padding removed
    PatchFingerprint fingerprint;
    PatchProvenance provenance;
    PatchUserMetadata userMetadata;
    // Bytes of the preserved original SysEx, without loading it.
    std::int64_t originalSysExSize = 0;

    // Every Wave Expansion group this Patch's Tones refer to, ascending. Read
    // from the derived tables rather than by decoding, so a row can say "needs
    // a board" without the library ever paying for a decode while scrolling.
    //
    // `expansionScanned` separates "uses no expansion wave" from "not analysed
    // yet"; an empty set means the first only when it is true. Nothing but a
    // library opened read-only or an entry inserted by an older build should
    // ever be unscanned.
    std::set<int> expansionGroups;
    bool expansionScanned = false;
};

// What to look for.
//
// Every filter is a narrowing: an unset field constrains nothing. Filters
// combine with AND, because "favourite pads rated four or more" is the
// question a librarian actually asks.
struct LibraryQuery
{
    enum class Order {
        NameAscending,
        NameDescending,
        ImportedNewestFirst,
        ImportedOldestFirst,
        RatingDescending,
        // The User bank slot the Patch came from; entries without one sort last.
        SourceSlotAscending,
    };

    // Whitespace-separated terms, all of which must appear in the Patch name.
    // Matching is case-insensitive and substring-based; the stored name bytes
    // are never altered.
    std::string text;
    // The entry must carry every one of these tags (exact match).
    std::vector<std::string> tags;
    std::string category;         // exact match; empty means "any"
    std::string sourceDigest;     // everything imported from one file
    std::optional<bool> favourite;
    std::optional<int> minimumRating;
    // Only entries whose parameters hash to this. Used to find exact
    // duplicates; the caller still confirms against the parameters.
    std::optional<PatchFingerprint> fingerprint;

    // What the Patch needs from a Wave Expansion Board.
    //
    // Deliberately not spelled "playable" / "unplayable". This layer knows
    // which groups a Patch refers to and which groups the caller says it has;
    // it does not know whether the caller's picture of the instrument is
    // complete. Turning `NeedsGroupOutsideProfile` into "will not play" rather
    // than "may not play" needs that, and it belongs where the profile lives.
    enum class Expansion {
        Any,
        // No Tone refers to an expansion wave, so no board can be missing.
        InternalOnly,
        // At least one Tone does, whoever provides it.
        UsesExpansion,
        // At least one Tone refers to a group not in `providedGroups`.
        NeedsGroupOutsideProfile,
        // Every group the Patch refers to, if any, is in `providedGroups` — the
        // exact complement of the above. Internal-only Patches qualify, because
        // needing no board is a way of having every board you need.
        PlaysWithProfile,
    };

    Expansion expansion = Expansion::Any;
    // The groups the caller's boards answer for. Only read for
    // `NeedsGroupOutsideProfile`; an empty set there means every expansion
    // reference is outside the profile, which is exactly right for an
    // instrument with no boards in it.
    std::set<int> providedGroups;

    Order order = Order::NameAscending;
    int limit = 0;  // 0 = no limit
    int offset = 0;
};

// One import source, as the Bank Builder's source picker needs it.
//
// A "source bank" is simply everything that arrived from one file or one
// device read: patches are grouped by the digest their provenance recorded, so
// the grouping is a fact about where the data came from rather than a category
// anybody had to maintain.
struct LibrarySourceSummary
{
    std::string digest;   // empty for entries with no file source
    std::string name;     // the source name recorded at import
    int patchCount = 0;
    std::chrono::system_clock::time_point importedAt{};
};

// A User bank the user arranged and saved.
struct SavedBankRecord
{
    std::int64_t id = 0;
    std::string name;
    int occupiedCount = 0;
    // Destinations whose Patch has since been deleted from the library. The
    // arrangement keeps them, and says so, rather than pretending they are
    // free.
    int missingCount = 0;
    std::chrono::system_clock::time_point createdAt{};
    std::chrono::system_clock::time_point updatedAt{};
};

struct SavedBank
{
    SavedBankRecord record;
    // Always BankDraft::kSlotCount entries, in slot order.
    std::vector<BankSlotContent> destinations;
    // The musician's own grouping of those destinations. XP60Studio's
    // organisation, saved with the bank and carried nowhere near the
    // instrument.
    std::vector<BankSection> sections;
};

// The persistent local library.
//
// `.syx` files are an import/export format, not the database
// (ARCHITECTURE.md §12). What is stored is normalised searchable metadata, the
// user's own metadata, provenance, the fingerprint, and the original SysEx
// bytes exactly as they arrived.
//
// The decoded Patch is deliberately NOT stored. `loadEntry` rebuilds it from
// the preserved bytes through the same codec an import uses, so the stored
// SysEx stays the single source of truth and can never drift from a
// second, decoded copy of the same data.
//
// The database never deduplicates on its own. Two identical Patches from
// different sources are two entries with two provenances; `findDuplicatesOf`
// reports them and the user decides. Silently discarding an import is exactly
// the data loss AGENTS.md forbids.
//
// All calls are synchronous and belong on a worker thread for large imports;
// the class owns no timers, signals or event-loop dependency.
class LibraryDatabase
{
public:
    // Bumped whenever the schema changes; `open` migrates forward and refuses
    // to open a file written by a newer build.
    //
    // 2 added the `banks` / `bank_slots` tables. The migration is additive —
    // no existing row is touched — so opening a version 1 library simply
    // creates the two new tables and stamps the new version.
    //
    // 5 added `patch_expansion_scan` / `patch_expansion_groups`. Those hold
    // derived data — the Wave Group IDs already present in each Patch's stored
    // SysEx — so an older library opens with them empty and `open` backfills
    // them by decoding the bytes it already has. Nothing is asked of the user
    // and nothing stored is altered.
    static constexpr int kSchemaVersion = 5;
    // Passed as the path to keep the whole library in memory (tests).
    static constexpr const char* kInMemoryPath = ":memory:";

    LibraryDatabase();
    ~LibraryDatabase();
    LibraryDatabase(const LibraryDatabase&) = delete;
    LibraryDatabase& operator=(const LibraryDatabase&) = delete;

    // Opens or creates the library at `path`, creating the schema if needed.
    // False on failure; `lastError()` says why.
    [[nodiscard]] bool open(const QString& path);
    void close();
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] const QString& lastError() const noexcept { return m_lastError; }
    [[nodiscard]] std::optional<int> schemaVersion() const;

    // Adds one entry. Returns its new id, or nullopt on failure.
    [[nodiscard]] std::optional<std::int64_t> insert(const LibraryEntry& entry);
    // Adds many in one transaction: either every entry lands or none does, so
    // a cancelled or failed bank import cannot leave a half-imported library.
    // Returns the new ids in order, or nullopt on failure.
    [[nodiscard]] std::optional<std::vector<std::int64_t>> insertAll(const std::vector<LibraryEntry>& entries);

    [[nodiscard]] bool updateUserMetadata(std::int64_t id, const PatchUserMetadata& metadata);
    [[nodiscard]] bool remove(std::int64_t id);

    [[nodiscard]] std::optional<LibraryRecord> record(std::int64_t id) const;
    // Rebuilds the full entry, decoding the preserved original SysEx.
    [[nodiscard]] std::optional<LibraryEntry> loadEntry(std::int64_t id) const;
    // The preserved bytes, exactly as they were imported.
    [[nodiscard]] std::optional<roland::ByteVector> originalSysEx(std::int64_t id) const;

    [[nodiscard]] std::vector<LibraryRecord> search(const LibraryQuery& query) const;
    // How many records the query matches, ignoring limit/offset. This is what
    // a virtualized model needs for its row count.
    [[nodiscard]] std::optional<int> count(const LibraryQuery& query) const;
    [[nodiscard]] std::optional<int> totalCount() const;

    // Other entries whose parameters hash the same as `id`'s. A fingerprint
    // match is a candidate, not a verdict: confirm with
    // `LibraryEntry::hasSameParameters` before calling anything a duplicate.
    [[nodiscard]] std::vector<LibraryRecord> findDuplicatesOf(std::int64_t id) const;

    // Distinct values in use, for filter chips. Sorted, no empties.
    [[nodiscard]] std::vector<std::string> categoriesInUse() const;
    [[nodiscard]] std::vector<std::string> tagsInUse() const;


    // Bank engineering ------------------------------------------------------
    //
    // A saved bank is an *arrangement*: 128 destinations, each either empty or
    // a reference to a Patch that stays exactly where it is in the library.
    // Saving a bank never copies, moves or rewrites a Patch, and deleting a
    // bank never deletes a Patch.
    //
    // A destination keeps the Patch name it was given even after that Patch is
    // deleted from the library. The reference becomes null, the slot is
    // reported as missing, and the user is told — which is the opposite of
    // silently shrinking their bank.

    // Inserts a new bank, or replaces the arrangement of `existingId`.
    // `destinations` shorter than 128 is padded with empty destinations; longer is
    // refused. Returns the bank's id.
    [[nodiscard]] std::optional<std::int64_t> saveBank(const std::string& name,
        const std::vector<BankSlotContent>& destinations, std::optional<std::int64_t> existingId = std::nullopt,
        const std::vector<BankSection>& sections = {});
    [[nodiscard]] std::vector<SavedBankRecord> banks() const;
    [[nodiscard]] std::optional<SavedBank> loadBank(std::int64_t id) const;
    [[nodiscard]] bool removeBank(std::int64_t id);

    // The musician's Wave Expansion configuration ---------------------------
    // One row per occupied slot. Stored in the library because it is the same
    // kind of thing as everything else here: XP60Studio's knowledge about the
    // user's setup, not Roland data.
    [[nodiscard]] bool saveExpansionProfile(const ExpansionProfile& profile);
    [[nodiscard]] std::optional<ExpansionProfile> loadExpansionProfile() const;

    // Where the library's Patches came from, one entry per distinct source.
    [[nodiscard]] std::vector<LibrarySourceSummary> sourcesInUse() const;

    // The expansion wave groups each of `ids` needs, for entries that have been
    // scanned. An id absent from the result was never scanned or is not in the
    // library; an id present with an empty set uses internal waves only. One
    // query whatever the count, so asking about a whole 128-destination bank is
    // as cheap as asking about one Patch — and decodes nothing.
    [[nodiscard]] std::map<std::int64_t, std::set<int>> expansionGroupsOf(const std::vector<std::int64_t>& ids) const;

private:
    // Fills the derived expansion tables for entries that have no scan row —
    // every entry in a library written before schema 5. Best-effort: an entry
    // whose stored bytes no longer decode stays unscanned, and the library
    // still opens.
    void backfillExpansionGroups();

    class Private;
    std::unique_ptr<Private> m_d;
    mutable QString m_lastError;
};

} // namespace xp60studio::library
