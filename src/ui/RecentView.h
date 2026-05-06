#pragma once

#include "core/FileType.h"
#include "core/ScanResult.h"

#include <QAbstractTableModel>
#include <QDate>
#include <QDateTime>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVector>
#include <QWidget>

class QButtonGroup;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableView;
class QVBoxLayout;

// One row in the Recent view: either a file or a folder, captured with
// the metadata needed to display + filter by created/modified date.
struct RecentEntry {
    enum class Kind { File, Folder };
    Kind kind = Kind::File;
    QString name;
    QString path;
    qint64 size = 0;          // for folders: aggregated total size
    QDateTime modified;
    QDateTime created;
    FileCategory category = FileCategory::Other;  // Other for folders
};

// Flat table model over a vector of RecentEntry rows.
class RecentTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColKind = 0,
        ColName,
        ColSize,
        ColCreated,
        ColModified,
        ColPath,
        ColumnCount,
    };

    explicit RecentTableModel(QObject* parent = nullptr);

    void setEntries(QVector<RecentEntry> entries);
    void clear();
    const RecentEntry& entryAt(int row) const { return m_entries.at(row); }

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;

private:
    QVector<RecentEntry> m_entries;
};

// Filters by a `created` date range and by free-text name/path search.
// The range is open on either end: an invalid `min` means "no lower
// bound", an invalid `max` means "no upper bound".  This lets the same
// proxy power both modes — Since (only min) and Range (min + max).
class RecentFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit RecentFilterProxyModel(QObject* parent = nullptr);

    void setCreatedRange(const QDateTime& min, const QDateTime& max);
    void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left,
                  const QModelIndex& right) const override;

private:
    QDateTime m_minCreated;
    QDateTime m_maxCreated;
    QString m_search;
};

// "Recent" navigation page: lists every file AND folder from the most
// recent scan whose creation date falls inside the chosen window.
//
// Two modes, switchable via a segmented control at the top:
//   * Since:  "files created in the last N days" — pills 3/7/14/31 days
//             plus 3/6 months, 1 year, and a custom spinbox.
//   * Range:  "files created between two dates" — two QDateEdit fields
//             with quick presets (Today / This week / This month).
class RecentView : public QWidget {
    Q_OBJECT
public:
    explicit RecentView(QWidget* parent = nullptr);

    void setScanResult(const ScanResult& r);

private:
    enum class Mode { Since = 0, Range = 1 };

    void retranslate();
    void buildSincePage(QWidget* host);
    void buildRangePage(QWidget* host);
    void rebuildPillButtons();
    void setMode(Mode mode);
    void setActiveDays(int days);
    void applyCustomDays();
    void applyRangeFromEditors();
    void applyFilter();
    void refreshSummary();

    void collectEntries(const FolderNode& node,
                        QVector<RecentEntry>& out) const;

    RecentTableModel* m_model = nullptr;
    RecentFilterProxyModel* m_proxy = nullptr;

    QTableView* m_table = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLineEdit* m_search = nullptr;

    // Mode selector.
    QButtonGroup* m_modeGroup = nullptr;
    QPushButton* m_modeSinceBtn = nullptr;
    QPushButton* m_modeRangeBtn = nullptr;
    QStackedWidget* m_modeStack = nullptr;

    // "Since" page widgets.
    QButtonGroup* m_pillsGroup = nullptr;
    QVector<QPushButton*> m_pillBtns;
    QSpinBox* m_customSpin = nullptr;
    QPushButton* m_customApplyBtn = nullptr;
    QLabel* m_customLabel = nullptr;
    QLabel* m_pillsLabel = nullptr;

    // "Range" page widgets.
    QLabel* m_rangeFromLabel = nullptr;
    QLabel* m_rangeToLabel = nullptr;
    QDateEdit* m_rangeFrom = nullptr;
    QDateEdit* m_rangeTo = nullptr;
    QPushButton* m_rangeApplyBtn = nullptr;
    QPushButton* m_rangeTodayBtn = nullptr;
    QPushButton* m_rangeWeekBtn = nullptr;
    QPushButton* m_rangeMonthBtn = nullptr;

    Mode m_mode = Mode::Since;
    int m_activeDays = 7;
    int m_totalRows = 0;
};
