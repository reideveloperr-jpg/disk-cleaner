#include "RecentView.h"

#include "core/SizeFormatter.h"
#include "i18n/I18n.h"

#include <QButtonGroup>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

namespace {

constexpr const char* kSettingsMode    = "recent/mode";
constexpr const char* kSettingsDays    = "recent/sinceDays";
constexpr const char* kSettingsFromIso = "recent/rangeFromIso";
constexpr const char* kSettingsToIso   = "recent/rangeToIso";

// Preset day-thresholds for the "Since" mode. Order matters — the UI
// renders them in a horizontal row in this order.
struct DayPreset { int days; const char* label; };
const QVector<DayPreset>& presets()
{
    static const QVector<DayPreset> kPresets = {
        { 3,   "3 days"   },
        { 7,   "7 days"   },
        { 14,  "14 days"  },
        { 31,  "31 days"  },
        { 90,  "3 months" },
        { 180, "6 months" },
        { 365, "1 year"   },
    };
    return kPresets;
}

QString kindLabel(RecentEntry::Kind kind)
{
    return kind == RecentEntry::Kind::Folder ? tr_("Folder")
                                             : tr_("File");
}

}  // namespace

// ---------------- RecentTableModel ----------------

RecentTableModel::RecentTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void RecentTableModel::setEntries(QVector<RecentEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

void RecentTableModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

int RecentTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

int RecentTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant RecentTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    const RecentEntry& e = m_entries.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColKind:    return kindLabel(e.kind);
            case ColName:    return e.name;
            case ColSize:    return SizeFormatter::humanReadable(e.size);
            case ColCreated: return e.created.isValid()
                                    ? e.created.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                    : QString();
            case ColModified: return e.modified.isValid()
                                    ? e.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                    : QString();
            case ColPath:    return e.path;
        }
    } else if (role == Qt::UserRole) {
        // Sort role — return native types so the proxy compares ints/dates
        // numerically/chronologically, not lexicographically.
        switch (index.column()) {
            case ColKind:    return int(e.kind);
            case ColName:    return e.name.toLower();
            case ColSize:    return e.size;
            case ColCreated: return e.created;
            case ColModified: return e.modified;
            case ColPath:    return e.path.toLower();
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColSize) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
    } else if (role == Qt::ToolTipRole) {
        return e.path;
    }
    return {};
}

QVariant RecentTableModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
        case ColKind:     return tr_("Kind");
        case ColName:     return tr_("Name");
        case ColSize:     return tr_("Size");
        case ColCreated:  return tr_("Created");
        case ColModified: return tr_("Modified");
        case ColPath:     return tr_("Path");
    }
    return {};
}

// ---------------- RecentFilterProxyModel ----------------

RecentFilterProxyModel::RecentFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(Qt::UserRole);
    setDynamicSortFilter(true);
}

void RecentFilterProxyModel::setCreatedRange(const QDateTime& min,
                                             const QDateTime& max)
{
    m_minCreated = min;
    m_maxCreated = max;
    invalidateFilter();
}

void RecentFilterProxyModel::setSearchText(const QString& text)
{
    m_search = text.trimmed().toLower();
    invalidateFilter();
}

bool RecentFilterProxyModel::filterAcceptsRow(int sourceRow,
                                              const QModelIndex& sourceParent) const
{
    auto* src = qobject_cast<RecentTableModel*>(sourceModel());
    if (!src) {
        return false;
    }
    Q_UNUSED(sourceParent);
    if (sourceRow < 0 || sourceRow >= src->rowCount()) {
        return false;
    }
    const RecentEntry& e = src->entryAt(sourceRow);

    if (m_minCreated.isValid()) {
        // Drop rows whose `created` is unknown — we cannot place them in
        // any window, so they are simply never "recent".
        if (!e.created.isValid() || e.created < m_minCreated) {
            return false;
        }
    }
    if (m_maxCreated.isValid()) {
        if (!e.created.isValid() || e.created > m_maxCreated) {
            return false;
        }
    }
    if (!m_search.isEmpty()) {
        if (!e.name.toLower().contains(m_search) &&
            !e.path.toLower().contains(m_search)) {
            return false;
        }
    }
    return true;
}

bool RecentFilterProxyModel::lessThan(const QModelIndex& left,
                                      const QModelIndex& right) const
{
    const QVariant l = sourceModel()->data(left, Qt::UserRole);
    const QVariant r = sourceModel()->data(right, Qt::UserRole);
    if (l.userType() == QMetaType::QDateTime) {
        return l.toDateTime() < r.toDateTime();
    }
    if (l.userType() == QMetaType::LongLong || l.userType() == QMetaType::Int) {
        return l.toLongLong() < r.toLongLong();
    }
    return l.toString() < r.toString();
}

// ---------------- RecentView ----------------

RecentView::RecentView(QWidget* parent) : QWidget(parent)
{
    setObjectName("RecentView");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    // ---- Header ----
    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("ViewTitle");
    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setObjectName("ViewSubtitle");
    m_subtitleLabel->setWordWrap(true);
    root->addWidget(m_titleLabel);
    root->addWidget(m_subtitleLabel);

    // ---- Mode selector (segmented) ----
    auto* modeRow = new QHBoxLayout;
    modeRow->setSpacing(6);
    m_modeSinceBtn = new QPushButton(this);
    m_modeRangeBtn = new QPushButton(this);
    for (auto* b : { m_modeSinceBtn, m_modeRangeBtn }) {
        b->setObjectName("Chip");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFlat(true);
    }
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    m_modeGroup->addButton(m_modeSinceBtn, int(Mode::Since));
    m_modeGroup->addButton(m_modeRangeBtn, int(Mode::Range));
    modeRow->addWidget(m_modeSinceBtn);
    modeRow->addWidget(m_modeRangeBtn);
    modeRow->addStretch();

    m_search = new QLineEdit(this);
    m_search->setObjectName("SearchInput");
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(220);
    modeRow->addWidget(m_search);

    root->addLayout(modeRow);

    // ---- Mode pages ----
    m_modeStack = new QStackedWidget(this);
    auto* sincePage = new QWidget(m_modeStack);
    auto* rangePage = new QWidget(m_modeStack);
    buildSincePage(sincePage);
    buildRangePage(rangePage);
    m_modeStack->addWidget(sincePage);
    m_modeStack->addWidget(rangePage);
    root->addWidget(m_modeStack);

    // ---- Summary line ----
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName("MutedLabel");
    root->addWidget(m_summaryLabel);

    // ---- Table ----
    m_model = new RecentTableModel(this);
    m_proxy = new RecentFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);

    m_table = new QTableView(this);
    m_table->setObjectName("RecentTable");
    m_table->setModel(m_proxy);
    m_table->setSortingEnabled(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->setShowGrid(false);
    m_table->sortByColumn(RecentTableModel::ColCreated, Qt::DescendingOrder);
    root->addWidget(m_table, 1);

    // ---- Wiring ----
    connect(m_modeGroup, &QButtonGroup::idClicked,
            this, [this](int id) { setMode(static_cast<Mode>(id)); });

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& t) {
        m_proxy->setSearchText(t);
        refreshSummary();
    });

    connect(&I18n::instance(), &I18n::languageChanged, this,
            [this] { retranslate(); });

    // Restore persisted state.
    QSettings s;
    const int savedMode = s.value(kSettingsMode, int(Mode::Since)).toInt();
    m_activeDays = s.value(kSettingsDays, 7).toInt();

    retranslate();
    rebuildPillButtons();
    setActiveDays(m_activeDays);

    // Defaults for the date editors: From = today - 7d, To = today.
    const QDate today = QDate::currentDate();
    const QDate fromDate = QDate::fromString(
        s.value(kSettingsFromIso, today.addDays(-7).toString(Qt::ISODate)).toString(),
        Qt::ISODate);
    const QDate toDate = QDate::fromString(
        s.value(kSettingsToIso, today.toString(Qt::ISODate)).toString(),
        Qt::ISODate);
    m_rangeFrom->setDate(fromDate.isValid() ? fromDate : today.addDays(-7));
    m_rangeTo->setDate(toDate.isValid() ? toDate : today);

    setMode(static_cast<Mode>(savedMode));
}

void RecentView::buildSincePage(QWidget* host)
{
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_pillsLabel = new QLabel(host);
    m_pillsLabel->setObjectName("MutedLabel");
    layout->addWidget(m_pillsLabel);

    auto* pillsRow = new QHBoxLayout;
    pillsRow->setSpacing(6);
    m_pillsGroup = new QButtonGroup(host);
    m_pillsGroup->setExclusive(true);
    connect(m_pillsGroup, &QButtonGroup::idClicked, this,
            [this](int days) { setActiveDays(days); });

    // Build the preset pills once. retranslate() will only refresh
    // their text — the button objects themselves are stable.
    for (const auto& p : presets()) {
        auto* btn = new QPushButton(host);
        btn->setObjectName("Chip");
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFlat(true);
        m_pillsGroup->addButton(btn, p.days);
        pillsRow->addWidget(btn);
        m_pillBtns.push_back(btn);
    }
    pillsRow->addStretch();
    layout->addLayout(pillsRow);

    auto* customRow = new QHBoxLayout;
    customRow->setSpacing(6);
    m_customLabel = new QLabel(host);
    m_customLabel->setObjectName("MutedLabel");
    m_customSpin = new QSpinBox(host);
    m_customSpin->setRange(1, 36500);  // up to ~100 years
    m_customSpin->setSingleStep(1);
    m_customSpin->setValue(30);
    m_customApplyBtn = new QPushButton(host);
    m_customApplyBtn->setObjectName("Chip");
    m_customApplyBtn->setCursor(Qt::PointingHandCursor);
    m_customApplyBtn->setFlat(true);
    connect(m_customApplyBtn, &QPushButton::clicked,
            this, &RecentView::applyCustomDays);
    connect(m_customSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { applyCustomDays(); });

    customRow->addWidget(m_customLabel);
    customRow->addWidget(m_customSpin);
    customRow->addWidget(m_customApplyBtn);
    customRow->addStretch();
    layout->addLayout(customRow);
    layout->addStretch();
}

void RecentView::buildRangePage(QWidget* host)
{
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* row = new QHBoxLayout;
    row->setSpacing(6);

    m_rangeFromLabel = new QLabel(host);
    m_rangeFromLabel->setObjectName("MutedLabel");
    m_rangeFrom = new QDateEdit(host);
    m_rangeFrom->setCalendarPopup(true);
    m_rangeFrom->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_rangeFrom->setMinimumDate(QDate(1970, 1, 1));
    m_rangeFrom->setMaximumDate(QDate(9999, 12, 31));

    m_rangeToLabel = new QLabel(host);
    m_rangeToLabel->setObjectName("MutedLabel");
    m_rangeTo = new QDateEdit(host);
    m_rangeTo->setCalendarPopup(true);
    m_rangeTo->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_rangeTo->setMinimumDate(QDate(1970, 1, 1));
    m_rangeTo->setMaximumDate(QDate(9999, 12, 31));

    m_rangeApplyBtn = new QPushButton(host);
    m_rangeApplyBtn->setObjectName("Chip");
    m_rangeApplyBtn->setCursor(Qt::PointingHandCursor);
    m_rangeApplyBtn->setFlat(true);
    connect(m_rangeApplyBtn, &QPushButton::clicked,
            this, &RecentView::applyRangeFromEditors);

    // Re-apply automatically when the user changes either editor — saves
    // the extra click on "Apply" for the common case.
    connect(m_rangeFrom, &QDateEdit::dateChanged,
            this, [this] { applyRangeFromEditors(); });
    connect(m_rangeTo, &QDateEdit::dateChanged,
            this, [this] { applyRangeFromEditors(); });

    row->addWidget(m_rangeFromLabel);
    row->addWidget(m_rangeFrom);
    row->addSpacing(8);
    row->addWidget(m_rangeToLabel);
    row->addWidget(m_rangeTo);
    row->addSpacing(8);
    row->addWidget(m_rangeApplyBtn);
    row->addStretch();
    layout->addLayout(row);

    // Quick presets for the range editors.
    auto* presetsRow = new QHBoxLayout;
    presetsRow->setSpacing(6);
    auto makePreset = [&](QPushButton*& btn) {
        btn = new QPushButton(host);
        btn->setObjectName("Chip");
        btn->setCheckable(false);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFlat(true);
        presetsRow->addWidget(btn);
    };
    makePreset(m_rangeTodayBtn);
    makePreset(m_rangeWeekBtn);
    makePreset(m_rangeMonthBtn);
    presetsRow->addStretch();

    connect(m_rangeTodayBtn, &QPushButton::clicked, this, [this] {
        const QDate today = QDate::currentDate();
        m_rangeFrom->setDate(today);
        m_rangeTo->setDate(today);
    });
    connect(m_rangeWeekBtn, &QPushButton::clicked, this, [this] {
        const QDate today = QDate::currentDate();
        m_rangeFrom->setDate(today.addDays(-6));
        m_rangeTo->setDate(today);
    });
    connect(m_rangeMonthBtn, &QPushButton::clicked, this, [this] {
        const QDate today = QDate::currentDate();
        m_rangeFrom->setDate(today.addMonths(-1).addDays(1));
        m_rangeTo->setDate(today);
    });

    layout->addLayout(presetsRow);
    layout->addStretch();
}

void RecentView::rebuildPillButtons()
{
    // The pill buttons are created once in buildSincePage(); here we
    // only refresh their localized labels. Iteration order matches the
    // presets() table because m_pillBtns was populated in that order.
    const auto& ps = presets();
    for (int i = 0; i < m_pillBtns.size() && i < ps.size(); ++i) {
        m_pillBtns[i]->setText(
            I18n::instance().tr_s(QString::fromUtf8(ps[i].label)));
    }
}

void RecentView::setMode(Mode mode)
{
    m_mode = mode;
    m_modeSinceBtn->setChecked(mode == Mode::Since);
    m_modeRangeBtn->setChecked(mode == Mode::Range);
    m_modeStack->setCurrentIndex(int(mode));

    QSettings s;
    s.setValue(kSettingsMode, int(mode));

    applyFilter();
}

void RecentView::setActiveDays(int days)
{
    if (days <= 0) {
        return;
    }
    m_activeDays = days;
    QSettings s;
    s.setValue(kSettingsDays, days);

    // Sync pill check state — if the value matches a preset, check it;
    // otherwise un-check all (the value lives in the custom spinbox).
    bool isPreset = false;
    for (auto* b : m_pillBtns) {
        const bool match = (m_pillsGroup->id(b) == days);
        b->setChecked(match);
        isPreset = isPreset || match;
    }
    if (!isPreset && m_customSpin) {
        QSignalBlocker blocker(m_customSpin);
        m_customSpin->setValue(days);
    }

    applyFilter();
}

void RecentView::applyCustomDays()
{
    setActiveDays(m_customSpin->value());
}

void RecentView::applyRangeFromEditors()
{
    const QDate from = m_rangeFrom->date();
    const QDate to   = m_rangeTo->date();
    QSettings s;
    s.setValue(kSettingsFromIso, from.toString(Qt::ISODate));
    s.setValue(kSettingsToIso,   to.toString(Qt::ISODate));
    applyFilter();
}

void RecentView::applyFilter()
{
    QDateTime min;
    QDateTime max;
    if (m_mode == Mode::Since) {
        if (m_activeDays > 0) {
            min = QDateTime::currentDateTime().addDays(-m_activeDays);
        }
    } else {
        QDate from = m_rangeFrom ? m_rangeFrom->date() : QDate();
        QDate to   = m_rangeTo   ? m_rangeTo->date()   : QDate();
        if (from.isValid() && to.isValid() && from > to) {
            std::swap(from, to);  // be forgiving if user inverts them
        }
        if (from.isValid()) {
            min = QDateTime(from, QTime(0, 0));
        }
        if (to.isValid()) {
            max = QDateTime(to, QTime(23, 59, 59, 999));
        }
    }
    if (m_proxy) {
        m_proxy->setCreatedRange(min, max);
    }
    refreshSummary();
}

void RecentView::refreshSummary()
{
    if (!m_summaryLabel || !m_proxy) {
        return;
    }
    const int shown = m_proxy->rowCount();
    qint64 totalBytes = 0;
    for (int row = 0; row < shown; ++row) {
        const QModelIndex idx = m_proxy->mapToSource(m_proxy->index(row, 0));
        const RecentEntry& e = m_model->entryAt(idx.row());
        if (e.kind == RecentEntry::Kind::File) {
            totalBytes += e.size;
        }
    }
    m_summaryLabel->setText(
        tr_("Showing %1 of %2 entries · %3")
            .arg(QLocale().toString(shown))
            .arg(QLocale().toString(m_totalRows))
            .arg(SizeFormatter::humanReadable(totalBytes)));
}

void RecentView::collectEntries(const FolderNode& node,
                                QVector<RecentEntry>& out) const
{
    // Folders themselves get a row…
    if (!node.path.isEmpty()) {
        RecentEntry e;
        e.kind = RecentEntry::Kind::Folder;
        e.name = node.name.isEmpty() ? node.path : node.name;
        e.path = node.path;
        e.size = node.totalSize;
        e.modified = node.modified;
        e.created  = node.created;
        e.category = FileCategory::Other;
        out.push_back(std::move(e));
    }
    for (const auto& child : node.children) {
        collectEntries(child, out);
    }
}

void RecentView::setScanResult(const ScanResult& r)
{
    QVector<RecentEntry> rows;
    rows.reserve(r.files.size() + 64);

    // Files first…
    for (const auto& f : r.files) {
        RecentEntry e;
        e.kind = RecentEntry::Kind::File;
        e.name = f.name;
        e.path = f.path;
        e.size = f.size;
        e.modified = f.modified;
        e.created  = f.created;
        e.category = f.category;
        rows.push_back(std::move(e));
    }
    // …then every folder in the tree.
    collectEntries(r.root, rows);

    m_totalRows = rows.size();
    m_model->setEntries(std::move(rows));
    applyFilter();
}

void RecentView::retranslate()
{
    m_titleLabel->setText(tr_("Recent"));
    m_subtitleLabel->setText(
        tr_("Files and folders created within the chosen window."));
    m_modeSinceBtn->setText(tr_("Last N days"));
    m_modeRangeBtn->setText(tr_("Date range"));
    m_search->setPlaceholderText(tr_("Search by name or path"));

    if (m_pillsLabel) {
        m_pillsLabel->setText(tr_("Window:"));
    }
    if (m_customLabel) {
        m_customLabel->setText(tr_("Custom — number of days:"));
    }
    if (m_customApplyBtn) {
        m_customApplyBtn->setText(tr_("Apply"));
    }
    if (m_rangeFromLabel) {
        m_rangeFromLabel->setText(tr_("From:"));
    }
    if (m_rangeToLabel) {
        m_rangeToLabel->setText(tr_("To:"));
    }
    if (m_rangeApplyBtn) {
        m_rangeApplyBtn->setText(tr_("Apply"));
    }
    if (m_rangeTodayBtn) {
        m_rangeTodayBtn->setText(tr_("Today"));
    }
    if (m_rangeWeekBtn) {
        m_rangeWeekBtn->setText(tr_("This week"));
    }
    if (m_rangeMonthBtn) {
        m_rangeMonthBtn->setText(tr_("This month"));
    }

    rebuildPillButtons();
    refreshSummary();
}
