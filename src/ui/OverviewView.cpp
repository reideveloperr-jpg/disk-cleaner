#include "OverviewView.h"

#include "DriveCard.h"
#include "UsageBar.h"
#include "core/DriveInfo.h"
#include "core/SizeFormatter.h"
#include "i18n/I18n.h"

#include <QDateTime>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>

namespace {

QWidget* makeMetric(const QString& caption, const QString& value, QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);
    auto* cap = new QLabel(caption, w);
    cap->setObjectName("MetricCaption");
    auto* val = new QLabel(value, w);
    val->setObjectName("MetricValue");
    lay->addWidget(cap);
    lay->addWidget(val);
    return w;
}

void clearGrid(QGridLayout* grid)
{
    QLayoutItem* item;
    while ((item = grid->takeAt(0)) != nullptr) {
        if (auto* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
}

// Convenience accessor for the per-block visibility toggles. The defaults
// match the v0.2 behaviour so users who never visit Settings keep seeing
// every block.
bool overviewFlag(const QString& key, bool defaultValue = true)
{
    QSettings s;
    return s.value(QStringLiteral("overview/") + key, defaultValue).toBool();
}

}  // namespace

OverviewView::OverviewView(QWidget* parent) : QWidget(parent)
{
    setObjectName("OverviewView");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    auto* head = new QHBoxLayout;
    auto* col = new QVBoxLayout;

    m_titleLabel = new QLabel(tr_("Drives"), this);
    m_titleLabel->setObjectName("PageTitle");
    m_subtitleLabel = new QLabel(
        tr_("Pick a drive to inspect its contents."),
        this);
    m_subtitleLabel->setObjectName("PageSubtitle");

    col->addWidget(m_titleLabel);
    col->addWidget(m_subtitleLabel);
    head->addLayout(col, 1);

    m_configureBtn = new QPushButton(tr_("⚙ Configure"), this);
    m_configureBtn->setObjectName("SecondaryButton");
    m_configureBtn->setCursor(Qt::PointingHandCursor);
    m_configureBtn->setToolTip(
        tr_("Pick which statistics to show on the Overview page"));
    connect(m_configureBtn, &QPushButton::clicked,
            this, &OverviewView::configureRequested);
    head->addWidget(m_configureBtn, 0, Qt::AlignTop);

    m_refreshBtn = new QPushButton(tr_("Refresh"), this);
    m_refreshBtn->setObjectName("SecondaryButton");
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    connect(m_refreshBtn, &QPushButton::clicked, this, &OverviewView::refresh);
    head->addWidget(m_refreshBtn, 0, Qt::AlignTop);

    root->addLayout(head);

    connect(&I18n::instance(), &I18n::languageChanged, this, [this] {
        m_titleLabel->setText(tr_("Drives"));
        m_subtitleLabel->setText(tr_("Pick a drive to inspect its contents."));
        m_refreshBtn->setText(tr_("Refresh"));
        m_configureBtn->setText(tr_("⚙ Configure"));
        m_configureBtn->setToolTip(
            tr_("Pick which statistics to show on the Overview page"));
        refresh();
    });

    // Outer scroll area wrapping the entire stats stack so wide content
    // (e.g. long file paths in "Largest files") never gets clipped on
    // narrow windows.  We disable horizontal scrolling and rely on the
    // content widget shrinking to the viewport width.
    auto* outerScroll = new QScrollArea(this);
    outerScroll->setObjectName("OverviewScroll");
    outerScroll->setWidgetResizable(true);
    outerScroll->setFrameShape(QFrame::NoFrame);
    outerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* outerContent = new QWidget(outerScroll);
    outerContent->setObjectName("OverviewScrollContent");
    auto* contentCol = new QVBoxLayout(outerContent);
    contentCol->setContentsMargins(0, 0, 0, 0);
    contentCol->setSpacing(14);

    outerScroll->setWidget(outerContent);
    root->addWidget(outerScroll, 1);

    // ---------- Storage at a glance ----------
    m_statsCard = new QFrame(outerContent);
    m_statsCard->setObjectName("StatsCard");
    m_statsCard->setFrameShape(QFrame::StyledPanel);
    m_statsCard->setSizePolicy(QSizePolicy::MinimumExpanding,
                               QSizePolicy::Maximum);
    {
        auto* v = new QVBoxLayout(m_statsCard);
        v->setContentsMargins(18, 14, 18, 14);
        v->setSpacing(10);
        m_statsTitle = new QLabel(tr_("Storage at a glance"), m_statsCard);
        m_statsTitle->setObjectName("CardTitle");
        v->addWidget(m_statsTitle);
        m_statsGrid = new QGridLayout;
        m_statsGrid->setContentsMargins(0, 0, 0, 0);
        m_statsGrid->setHorizontalSpacing(28);
        m_statsGrid->setVerticalSpacing(8);
        v->addLayout(m_statsGrid);

        m_drivesBarsTitle = new QLabel(tr_("Per-drive usage"), m_statsCard);
        m_drivesBarsTitle->setObjectName("CardSubtitle");
        v->addSpacing(4);
        v->addWidget(m_drivesBarsTitle);
        m_drivesBarsGrid = new QGridLayout;
        m_drivesBarsGrid->setContentsMargins(0, 0, 0, 0);
        m_drivesBarsGrid->setHorizontalSpacing(14);
        m_drivesBarsGrid->setVerticalSpacing(6);
        m_drivesBarsGrid->setColumnStretch(1, 1);
        v->addLayout(m_drivesBarsGrid);
    }
    contentCol->addWidget(m_statsCard);

    // ---------- Last scan ----------
    m_lastScanCard = new QFrame(outerContent);
    m_lastScanCard->setObjectName("StatsCard");
    m_lastScanCard->setFrameShape(QFrame::StyledPanel);
    m_lastScanCard->setSizePolicy(QSizePolicy::MinimumExpanding,
                                  QSizePolicy::Maximum);
    {
        auto* v = new QVBoxLayout(m_lastScanCard);
        v->setContentsMargins(18, 14, 18, 14);
        v->setSpacing(8);
        m_lastScanTitle = new QLabel(tr_("Last scan"), m_lastScanCard);
        m_lastScanTitle->setObjectName("CardTitle");
        m_lastScanBody = new QLabel(m_lastScanCard);
        m_lastScanBody->setObjectName("CardBody");
        m_lastScanBody->setWordWrap(true);
        v->addWidget(m_lastScanTitle);
        v->addWidget(m_lastScanBody);

        // Extra metrics row: avg size, median size, smallest, etc.
        m_lastScanExtraTitle = new QLabel(tr_("File size summary"),
                                          m_lastScanCard);
        m_lastScanExtraTitle->setObjectName("CardSubtitle");
        v->addSpacing(2);
        v->addWidget(m_lastScanExtraTitle);
        m_lastScanExtraGrid = new QGridLayout;
        m_lastScanExtraGrid->setContentsMargins(0, 0, 0, 0);
        m_lastScanExtraGrid->setHorizontalSpacing(28);
        m_lastScanExtraGrid->setVerticalSpacing(8);
        v->addLayout(m_lastScanExtraGrid);

        // Top file extensions by aggregate size.
        m_topExtsTitle = new QLabel(tr_("Top file types by size"),
                                    m_lastScanCard);
        m_topExtsTitle->setObjectName("CardSubtitle");
        v->addSpacing(2);
        v->addWidget(m_topExtsTitle);
        m_topExtsGrid = new QGridLayout;
        m_topExtsGrid->setContentsMargins(0, 0, 0, 0);
        m_topExtsGrid->setHorizontalSpacing(14);
        m_topExtsGrid->setVerticalSpacing(8);
        m_topExtsGrid->setColumnStretch(1, 1);
        v->addLayout(m_topExtsGrid);

        m_lastScanCatsTitle = new QLabel(tr_("By type"), m_lastScanCard);
        m_lastScanCatsTitle->setObjectName("CardSubtitle");
        v->addSpacing(2);
        v->addWidget(m_lastScanCatsTitle);
        m_lastScanCatsGrid = new QGridLayout;
        m_lastScanCatsGrid->setContentsMargins(0, 0, 0, 0);
        m_lastScanCatsGrid->setHorizontalSpacing(14);
        m_lastScanCatsGrid->setVerticalSpacing(8);
        m_lastScanCatsGrid->setColumnStretch(1, 1);
        v->addLayout(m_lastScanCatsGrid);

        m_lastScanTopFilesTitle = new QLabel(tr_("Largest files"), m_lastScanCard);
        m_lastScanTopFilesTitle->setObjectName("CardSubtitle");
        v->addSpacing(2);
        v->addWidget(m_lastScanTopFilesTitle);
        m_lastScanTopFilesGrid = new QGridLayout;
        m_lastScanTopFilesGrid->setContentsMargins(0, 0, 0, 0);
        m_lastScanTopFilesGrid->setHorizontalSpacing(14);
        m_lastScanTopFilesGrid->setVerticalSpacing(8);
        m_lastScanTopFilesGrid->setColumnStretch(0, 1);
        v->addLayout(m_lastScanTopFilesGrid);

        m_ageHistTitle = new QLabel(tr_("File age"), m_lastScanCard);
        m_ageHistTitle->setObjectName("CardSubtitle");
        v->addSpacing(2);
        v->addWidget(m_ageHistTitle);
        m_ageHistGrid = new QGridLayout;
        m_ageHistGrid->setContentsMargins(0, 0, 0, 0);
        m_ageHistGrid->setHorizontalSpacing(14);
        m_ageHistGrid->setVerticalSpacing(8);
        m_ageHistGrid->setColumnStretch(1, 1);
        v->addLayout(m_ageHistGrid);

        m_sizeBucketsTitle = new QLabel(tr_("File size"), m_lastScanCard);
        m_sizeBucketsTitle->setObjectName("CardSubtitle");
        v->addSpacing(2);
        v->addWidget(m_sizeBucketsTitle);
        m_sizeBucketsGrid = new QGridLayout;
        m_sizeBucketsGrid->setContentsMargins(0, 0, 0, 0);
        m_sizeBucketsGrid->setHorizontalSpacing(14);
        m_sizeBucketsGrid->setVerticalSpacing(8);
        m_sizeBucketsGrid->setColumnStretch(1, 1);
        v->addLayout(m_sizeBucketsGrid);
    }
    contentCol->addWidget(m_lastScanCard);
    m_lastScanCard->hide();

    // ---------- Drive cards ----------
    m_gridContainer = new QWidget(outerContent);
    m_grid = new QGridLayout(m_gridContainer);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(16);
    contentCol->addWidget(m_gridContainer);
    contentCol->addStretch(1);
}

void OverviewView::rebuildStatsCard()
{
    clearGrid(m_statsGrid);
    clearGrid(m_drivesBarsGrid);

    const auto drives = Drives::enumerate();
    if (drives.isEmpty()) {
        auto* msg = new QLabel(tr_("No drives detected."), m_statsCard);
        msg->setObjectName("CardBody");
        m_statsGrid->addWidget(msg, 0, 0);
        m_drivesBarsTitle->hide();
        return;
    }
    m_drivesBarsTitle->show();

    qint64 totalAll = 0;
    qint64 freeAll  = 0;
    QHash<QString, int> fsCount;
    QString biggestPath;
    qint64 biggestUsed = 0;
    QString fullestPath;
    double  fullestPct = -1.0;

    for (const DriveInfo& d : drives) {
        totalAll += d.totalBytes;
        freeAll  += d.freeBytes;
        const QString fs = d.fileSystem.isEmpty()
                               ? tr_("Unknown")
                               : d.fileSystem;
        fsCount[fs]++;
        if (d.usedBytes() > biggestUsed) {
            biggestUsed = d.usedBytes();
            biggestPath = d.rootPath;
        }
        const double p = d.totalBytes > 0
            ? double(d.usedBytes()) / double(d.totalBytes) : 0.0;
        if (p > fullestPct) {
            fullestPct = p;
            fullestPath = d.rootPath;
        }
    }
    const qint64 usedAll = totalAll - freeAll;
    const double usedPct = totalAll > 0
        ? 100.0 * double(usedAll) / double(totalAll) : 0.0;

    QStringList fsList;
    for (auto it = fsCount.constBegin(); it != fsCount.constEnd(); ++it) {
        fsList << QString::fromUtf8("%1 × %2")
                      .arg(it.key(), QLocale().toString(it.value()));
    }

    int col = 0;
    m_statsGrid->addWidget(
        makeMetric(tr_("Drives"),
                   QLocale().toString(qlonglong(drives.size())),
                   m_statsCard),
        0, col++);
    m_statsGrid->addWidget(
        makeMetric(tr_("Total"),
                   SizeFormatter::humanReadable(totalAll),
                   m_statsCard),
        0, col++);
    m_statsGrid->addWidget(
        makeMetric(tr_("Used"),
                   QString::fromUtf8("%1  ·  %2 %")
                       .arg(SizeFormatter::humanReadable(usedAll),
                            QLocale().toString(usedPct, 'f', 1)),
                   m_statsCard),
        0, col++);
    m_statsGrid->addWidget(
        makeMetric(tr_("Free"),
                   SizeFormatter::humanReadable(freeAll),
                   m_statsCard),
        0, col++);

    int row = 1;
    if (overviewFlag(QStringLiteral("showCombinedBar"))) {
        auto* combined = new QWidget(m_statsCard);
        auto* cv = new QVBoxLayout(combined);
        cv->setContentsMargins(0, 0, 0, 0);
        cv->setSpacing(2);
        auto* combinedCap = new QLabel(tr_("Combined usage"), combined);
        combinedCap->setObjectName("MetricCaption");
        auto* combinedBar = new UsageBar(combined);
        combinedBar->setValue(usedAll, totalAll);
        cv->addWidget(combinedCap);
        cv->addWidget(combinedBar);
        m_statsGrid->addWidget(combined, row++, 0, 1, col);
    }

    if (overviewFlag(QStringLiteral("showFilesystems"))) {
        m_statsGrid->addWidget(
            makeMetric(tr_("Filesystems"),
                       fsList.join(QStringLiteral(", ")),
                       m_statsCard),
            row++, 0, 1, col);
    }

    if (overviewFlag(QStringLiteral("showMostUsedDrive")) && !biggestPath.isEmpty()) {
        m_statsGrid->addWidget(
            makeMetric(tr_("Most used drive"),
                       QString::fromUtf8("%1  ·  %2 used")
                           .arg(biggestPath,
                                SizeFormatter::humanReadable(biggestUsed)),
                       m_statsCard),
            row++, 0, 1, col);
    }
    if (overviewFlag(QStringLiteral("showFullestDrive")) && !fullestPath.isEmpty() && fullestPct >= 0) {
        m_statsGrid->addWidget(
            makeMetric(tr_("Fullest drive"),
                       QString::fromUtf8("%1  ·  %2 %")
                           .arg(fullestPath,
                                QLocale().toString(fullestPct * 100.0, 'f', 1)),
                       m_statsCard),
            row++, 0, 1, col);
    }

    // Free-space-low warning: a quick "you should clean up" cue when any
    // drive is below 10 % free.  Hidden by default-off toggle.
    if (overviewFlag(QStringLiteral("showLowSpaceAlert"))) {
        QStringList lowDrives;
        for (const DriveInfo& d : drives) {
            if (d.totalBytes <= 0) continue;
            const double freePct =
                100.0 * double(d.freeBytes) / double(d.totalBytes);
            if (freePct < 10.0) {
                lowDrives << QString::fromUtf8("%1 (%2 %)")
                                  .arg(d.rootPath,
                                       QLocale().toString(freePct, 'f', 1));
            }
        }
        if (!lowDrives.isEmpty()) {
            m_statsGrid->addWidget(
                makeMetric(tr_("Low space"),
                           lowDrives.join(QStringLiteral(", ")),
                           m_statsCard),
                row++, 0, 1, col);
        }
    }

    // Per-drive bars: name | bar | numeric label.
    if (overviewFlag(QStringLiteral("showPerDriveBars"))) {
        m_drivesBarsTitle->show();
        int r = 0;
        for (const DriveInfo& d : drives) {
            const QString name = d.label.isEmpty()
                                     ? d.rootPath
                                     : QString::fromUtf8("%1  ·  %2")
                                           .arg(d.rootPath, d.label);
            auto* nameLbl = new QLabel(name, m_statsCard);
            nameLbl->setObjectName("CardBody");
            auto* bar = new UsageBar(m_statsCard);
            bar->setValue(d.usedBytes(), d.totalBytes);
            const double pct = d.totalBytes > 0
                ? 100.0 * double(d.usedBytes()) / double(d.totalBytes) : 0.0;
            const QString numeric =
                QString::fromUtf8("%1 / %2  ·  %3 %")
                    .arg(SizeFormatter::humanReadable(d.usedBytes()),
                         SizeFormatter::humanReadable(d.totalBytes),
                         QLocale().toString(pct, 'f', 1));
            auto* numLbl = new QLabel(numeric, m_statsCard);
            numLbl->setObjectName("CardBody");
            numLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            numLbl->setMinimumWidth(180);

            m_drivesBarsGrid->addWidget(nameLbl, r, 0);
            m_drivesBarsGrid->addWidget(bar, r, 1);
            m_drivesBarsGrid->addWidget(numLbl, r, 2);
            ++r;
        }
    } else {
        m_drivesBarsTitle->hide();
    }
}

void OverviewView::rebuildLastScanCard()
{
    clearGrid(m_lastScanCatsGrid);
    clearGrid(m_lastScanTopFilesGrid);
    clearGrid(m_ageHistGrid);
    clearGrid(m_sizeBucketsGrid);
    clearGrid(m_lastScanExtraGrid);
    clearGrid(m_topExtsGrid);

    if (!overviewFlag(QStringLiteral("showLastScan"))) {
        m_lastScanCard->hide();
        return;
    }

    QSettings s;
    const QString path = s.value(QStringLiteral("lastScan/path")).toString();
    if (path.isEmpty()) {
        m_lastScanCard->hide();
        return;
    }
    const qlonglong files = s.value(QStringLiteral("lastScan/files")).toLongLong();
    const qlonglong size  = s.value(QStringLiteral("lastScan/size")).toLongLong();
    const qlonglong dirs  = s.value(QStringLiteral("lastScan/dirs")).toLongLong();
    const QDateTime when  = s.value(QStringLiteral("lastScan/when")).toDateTime();

    m_lastScanTitle->setText(tr_("Last scan"));
    QString body =
        tr_("%1  ·  %2 files  ·  %3  ·  %4 folders")
            .arg(path,
                 QLocale().toString(files),
                 SizeFormatter::humanReadable(size),
                 QLocale().toString(dirs));
    if (when.isValid()) {
        body += QString::fromUtf8("\n");
        body += tr_("Finished: %1")
                    .arg(when.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    }
    m_lastScanBody->setText(body);

    // -------- Extra metrics row (avg / median / smallest / largest /
    // unique extensions / scan-root free space). All values are saved
    // by MainWindow::onScanFinished, no on-the-fly recomputation.
    if (overviewFlag(QStringLiteral("showFileSizeSummary"), true) &&
        files > 0) {
        m_lastScanExtraTitle->show();
        const qlonglong avgSize    = s.value(QStringLiteral("lastScan/avgSize")).toLongLong();
        const qlonglong medianSize = s.value(QStringLiteral("lastScan/medianSize")).toLongLong();
        const qlonglong minSize    = s.value(QStringLiteral("lastScan/minSize")).toLongLong();
        const qlonglong maxSize    = s.value(QStringLiteral("lastScan/maxSize")).toLongLong();
        const qlonglong uniqExts   = s.value(QStringLiteral("lastScan/uniqExts")).toLongLong();
        const qlonglong rootFree   = s.value(QStringLiteral("lastScan/rootFree"), -1LL).toLongLong();
        const qlonglong rootTotal  = s.value(QStringLiteral("lastScan/rootTotal"), -1LL).toLongLong();
        const qlonglong avgPerDir  = dirs > 0 ? (files / dirs) : 0;

        int col = 0;
        m_lastScanExtraGrid->addWidget(
            makeMetric(tr_("Avg size"),
                       SizeFormatter::humanReadable(avgSize),
                       m_lastScanCard),
            0, col++);
        m_lastScanExtraGrid->addWidget(
            makeMetric(tr_("Median size"),
                       SizeFormatter::humanReadable(medianSize),
                       m_lastScanCard),
            0, col++);
        m_lastScanExtraGrid->addWidget(
            makeMetric(tr_("Smallest"),
                       SizeFormatter::humanReadable(minSize),
                       m_lastScanCard),
            0, col++);
        m_lastScanExtraGrid->addWidget(
            makeMetric(tr_("Largest"),
                       SizeFormatter::humanReadable(maxSize),
                       m_lastScanCard),
            0, col++);
        m_lastScanExtraGrid->addWidget(
            makeMetric(tr_("Files / folder"),
                       QLocale().toString(qlonglong(avgPerDir)),
                       m_lastScanCard),
            0, col++);
        m_lastScanExtraGrid->addWidget(
            makeMetric(tr_("Unique types"),
                       QLocale().toString(qlonglong(uniqExts)),
                       m_lastScanCard),
            0, col++);
        if (rootFree >= 0 && rootTotal > 0) {
            const double freePct = 100.0 * double(rootFree) / double(rootTotal);
            m_lastScanExtraGrid->addWidget(
                makeMetric(
                    tr_("Free on volume"),
                    QString::fromUtf8("%1  ·  %2 %")
                        .arg(SizeFormatter::humanReadable(rootFree),
                             QLocale().toString(freePct, 'f', 1)),
                    m_lastScanCard),
                0, col++);
        }
    } else {
        m_lastScanExtraTitle->hide();
    }

    // -------- Top file extensions by aggregate size --------
    const QStringList extNames = s.value(QStringLiteral("lastScan/topExtNames")).toStringList();
    const QStringList extSizes = s.value(QStringLiteral("lastScan/topExtSizes")).toStringList();
    const QStringList extCounts = s.value(QStringLiteral("lastScan/topExtCounts")).toStringList();
    if (overviewFlag(QStringLiteral("showTopExtensions"), true)
        && !extNames.isEmpty() && size > 0) {
        m_topExtsTitle->show();
        const int n = std::min({ extNames.size(), extSizes.size(),
                                 extCounts.size() });
        for (int i = 0; i < n; ++i) {
            const qlonglong sz  = extSizes[i].toLongLong();
            const qlonglong cnt = extCounts[i].toLongLong();
            auto* nameLbl = new QLabel(extNames[i].isEmpty()
                                           ? tr_("(no extension)")
                                           : extNames[i],
                                       m_lastScanCard);
            nameLbl->setObjectName("CardBody");
            auto* bar = new UsageBar(m_lastScanCard);
            bar->setValue(sz, size);
            auto* numLbl = new QLabel(
                QString::fromUtf8("%1  ·  %2")
                    .arg(QLocale().toString(qlonglong(cnt)),
                         SizeFormatter::humanReadable(sz)),
                m_lastScanCard);
            numLbl->setObjectName("CardBody");
            numLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            // Reserve enough space on the right so the numeric column
            // does not get clipped on narrow viewports.
            numLbl->setMinimumWidth(180);
            m_topExtsGrid->addWidget(nameLbl, i, 0);
            m_topExtsGrid->addWidget(bar, i, 1);
            m_topExtsGrid->addWidget(numLbl, i, 2);
        }
    } else {
        m_topExtsTitle->hide();
    }

    // Top categories (saved as parallel lists in QSettings).
    const QStringList catNames = s.value(QStringLiteral("lastScan/topCatNames")).toStringList();
    const QStringList catSizesStr = s.value(QStringLiteral("lastScan/topCatSizes")).toStringList();
    if (overviewFlag(QStringLiteral("showTopCategories"))
        && !catNames.isEmpty() && size > 0) {
        m_lastScanCatsTitle->show();
        const int n = std::min(catNames.size(), catSizesStr.size());
        for (int i = 0; i < n; ++i) {
            const qlonglong sz = catSizesStr[i].toLongLong();
            const double pct = 100.0 * double(sz) / double(size);
            auto* nameLbl = new QLabel(catNames[i], m_lastScanCard);
            nameLbl->setObjectName("CardBody");
            auto* bar = new UsageBar(m_lastScanCard);
            bar->setValue(sz, size);
            auto* numLbl = new QLabel(
                QString::fromUtf8("%1  ·  %2 %")
                    .arg(SizeFormatter::humanReadable(sz),
                         QLocale().toString(pct, 'f', 1)),
                m_lastScanCard);
            numLbl->setObjectName("CardBody");
            numLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            numLbl->setMinimumWidth(180);
            m_lastScanCatsGrid->addWidget(nameLbl, i, 0);
            m_lastScanCatsGrid->addWidget(bar, i, 1);
            m_lastScanCatsGrid->addWidget(numLbl, i, 2);
        }
    } else {
        m_lastScanCatsTitle->hide();
    }

    // Top files.
    const QStringList topFilePaths = s.value(QStringLiteral("lastScan/topFilePaths")).toStringList();
    const QStringList topFileSizesStr = s.value(QStringLiteral("lastScan/topFileSizes")).toStringList();
    if (overviewFlag(QStringLiteral("showLargestFiles")) && !topFilePaths.isEmpty()) {
        m_lastScanTopFilesTitle->show();
        const int n = std::min(topFilePaths.size(), topFileSizesStr.size());
        for (int i = 0; i < n; ++i) {
            // Show "filename — folder" so the file name is easy to scan,
            // with the full path always available via the tooltip.
            const QFileInfo fi(topFilePaths[i]);
            const QString display =
                QString::fromUtf8("%1  ·  %2")
                    .arg(fi.fileName(), fi.absolutePath());
            auto* nameLbl = new QLabel(display, m_lastScanCard);
            nameLbl->setObjectName("CardBody");
            nameLbl->setToolTip(topFilePaths[i]);
            // Long paths overflowed the card and bled into adjacent rows.
            // Elide in the middle so the file name and the drive prefix
            // both stay visible, and keep one row per file regardless of
            // path length.
            nameLbl->setTextFormat(Qt::PlainText);
            nameLbl->setWordWrap(false);
            nameLbl->setSizePolicy(QSizePolicy::Ignored,
                                   QSizePolicy::Preferred);
            nameLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            const qlonglong sz = topFileSizesStr[i].toLongLong();
            auto* sizeLbl = new QLabel(SizeFormatter::humanReadable(sz),
                                       m_lastScanCard);
            sizeLbl->setObjectName("CardBody");
            sizeLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_lastScanTopFilesGrid->addWidget(nameLbl, i, 0);
            m_lastScanTopFilesGrid->addWidget(sizeLbl, i, 1);
        }
    } else {
        m_lastScanTopFilesTitle->hide();
    }

    // File age histogram.
    const QStringList ageCounts = s.value(QStringLiteral("lastScan/ageCounts")).toStringList();
    const QStringList ageSizes  = s.value(QStringLiteral("lastScan/ageSizes")).toStringList();
    if (overviewFlag(QStringLiteral("showAgeHistogram"))
        && ageCounts.size() == 4 && ageSizes.size() == 4) {
        m_ageHistTitle->show();
        const QStringList labels = {
            tr_("Last 7 days"), tr_("Last 30 days"),
            tr_("Last year"),   tr_("Older"),
        };
        for (int i = 0; i < 4; ++i) {
            const qlonglong cnt = ageCounts[i].toLongLong();
            const qlonglong sz  = ageSizes[i].toLongLong();
            auto* nameLbl = new QLabel(labels[i], m_lastScanCard);
            nameLbl->setObjectName("CardBody");
            auto* bar = new UsageBar(m_lastScanCard);
            bar->setValue(sz, size > 0 ? size : 1);
            auto* numLbl = new QLabel(
                QString::fromUtf8("%1  ·  %2")
                    .arg(QLocale().toString(qlonglong(cnt)),
                         SizeFormatter::humanReadable(sz)),
                m_lastScanCard);
            numLbl->setObjectName("CardBody");
            numLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            numLbl->setMinimumWidth(180);
            m_ageHistGrid->addWidget(nameLbl, i, 0);
            m_ageHistGrid->addWidget(bar, i, 1);
            m_ageHistGrid->addWidget(numLbl, i, 2);
        }
    } else {
        m_ageHistTitle->hide();
    }

    // File size buckets.
    const QStringList sizeCounts = s.value(QStringLiteral("lastScan/sizeCounts")).toStringList();
    const QStringList sizeSizes  = s.value(QStringLiteral("lastScan/sizeSizes")).toStringList();
    if (overviewFlag(QStringLiteral("showSizeBuckets"))
        && sizeCounts.size() == 4 && sizeSizes.size() == 4) {
        m_sizeBucketsTitle->show();
        const QStringList labels = {
            tr_("< 1 MB"), tr_("1 MB – 100 MB"),
            tr_("100 MB – 1 GB"), tr_("> 1 GB"),
        };
        for (int i = 0; i < 4; ++i) {
            const qlonglong cnt = sizeCounts[i].toLongLong();
            const qlonglong sz  = sizeSizes[i].toLongLong();
            auto* nameLbl = new QLabel(labels[i], m_lastScanCard);
            nameLbl->setObjectName("CardBody");
            auto* bar = new UsageBar(m_lastScanCard);
            bar->setValue(sz, size > 0 ? size : 1);
            auto* numLbl = new QLabel(
                QString::fromUtf8("%1  ·  %2")
                    .arg(QLocale().toString(qlonglong(cnt)),
                         SizeFormatter::humanReadable(sz)),
                m_lastScanCard);
            numLbl->setObjectName("CardBody");
            numLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            numLbl->setMinimumWidth(180);
            m_sizeBucketsGrid->addWidget(nameLbl, i, 0);
            m_sizeBucketsGrid->addWidget(bar, i, 1);
            m_sizeBucketsGrid->addWidget(numLbl, i, 2);
        }
    } else {
        m_sizeBucketsTitle->hide();
    }

    m_lastScanCard->show();
}

void OverviewView::refresh()
{
    rebuildStatsCard();
    rebuildLastScanCard();

    QLayoutItem* item;
    while ((item = m_grid->takeAt(0)) != nullptr) {
        if (auto* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const auto drives = Drives::enumerate();
    int row = 0, col = 0;
    constexpr int kCols = 2;
    for (const DriveInfo& d : drives) {
        auto* card = new DriveCard(d, m_gridContainer);
        connect(card, &DriveCard::analyzeRequested,
                this, &OverviewView::driveActivated);
        m_grid->addWidget(card, row, col);
        if (++col >= kCols) {
            col = 0;
            ++row;
        }
    }
    m_grid->setRowStretch(row + 1, 1);
}
