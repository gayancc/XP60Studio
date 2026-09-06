#pragma once

#include "library/ExpansionProfile.h"

#include <QAbstractListModel>
#include <QVariantMap>
#include <vector>

namespace xp60studio::presentation {

// The waveform catalogue, as a browsable list.
//
// Two sources, kept distinct because they are known to different degrees:
//
//  * **Internal** waves come from the transcribed XP-60 waveform list and are
//    always there. Their bank/number to SysEx mapping is hardware-verified
//    (DEVICE_ACCEPTANCE.md area 9).
//  * **Expansion** waves come from Roland's per-board Waveform Lists, and only
//    for boards the musician has declared *and* this project holds a list for
//    (`library::srJv80WaveName`). A board nobody declared contributes nothing:
//    the browser offers what this instrument can actually play, so a wave
//    picked here is one the Patch will sound.
//
// Catalog browsing cannot mutate a Patch or access a MIDI service.
class WaveBrowserModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY filterChanged)
    Q_PROPERTY(int sourceFilter READ sourceFilter WRITE setSourceFilter NOTIFY filterChanged)
    Q_PROPERTY(int count READ count NOTIFY filterChanged)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY selectionChanged)
    Q_PROPERTY(int selectedRow READ selectedRow NOTIFY selectionChanged)
    // How many expansion waves the declared boards contribute. Zero means the
    // Expansion tab has nothing to show, and the screen says why.
    Q_PROPERTY(int expansionCount READ expansionCount NOTIFY filterChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        BankRole,
        NumberRole,
        KeyRole,
        PageRole,
        AvailabilityRole,
        // True for a wave on a Wave Expansion Board.
        ExpansionRole,
        // The Wave Group ID an expansion row would write, or -1 for internal.
        WaveGroupRole,
        // "SR-JV80-01 Pop" for an expansion row, empty for internal.
        BoardRole,
    };
    explicit WaveBrowserModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString query() const { return m_query; }
    void setQuery(const QString& query);
    int sourceFilter() const { return m_source; }
    void setSourceFilter(int source);
    int count() const { return rowCount(); }
    int expansionCount() const;
    QVariantMap selected() const;
    int selectedRow() const;
    Q_INVOKABLE void selectRow(int row);

    // The instrument whose boards may be browsed. Optional: without it the
    // catalogue is internal waves only, which is what the screenshot harness
    // and any catalog-only context get.
    void setExpansionProfile(const library::ExpansionProfile* profile);
    // Call when the profile behind that pointer changes; a newly declared board
    // adds its waves and a cleared one takes them away.
    Q_INVOKABLE void expansionProfileChanged();

signals:
    void filterChanged();
    void selectionChanged();

private:
    // One browsable wave. Internal rows index the generated catalogue;
    // expansion rows carry the board and Roland's printed number, which is what
    // an assignment needs.
    struct Entry
    {
        bool expansion = false;
        int catalogIndex = -1;  // internal only
        int board = 0;          // expansion only: the SR-JV80 number = Wave Group ID
        int displayNumber = 0;  // expansion only, 1-based
    };

    void rebuild();
    void rebuildAll();
    [[nodiscard]] QString nameOf(const Entry& entry) const;
    [[nodiscard]] QString bankOf(const Entry& entry) const;
    [[nodiscard]] int numberOf(const Entry& entry) const;
    [[nodiscard]] QString keyOf(const Entry& entry) const;

    QString m_query;
    int m_source = 0;
    // Index into m_all of the selected entry, or -1.
    int m_selected = -1;
    const library::ExpansionProfile* m_profile = nullptr;
    // Every wave that could be shown, internal then expansion.
    std::vector<Entry> m_all;
    // Indices into m_all that pass the current filters.
    std::vector<int> m_rows;
};
} // namespace xp60studio::presentation
