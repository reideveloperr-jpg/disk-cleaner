#include "MainWindow.h"

#include "AnalyzerView.h"
#include "FilesView.h"
#include "OverviewView.h"
#include "RecentView.h"
#include "RadarOverlay.h"
#include "RgbGlowOverlay.h"
#include "RgbStripe.h"
#include "SettingsView.h"
#include "Sidebar.h"
#include "SpinnerWidget.h"
#include "ThemeManager.h"
#include "audio/SoundEngine.h"
#include "core/DriveInfo.h"
#include "core/FileType.h"
#include "core/SizeFormatter.h"
#include "i18n/I18n.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileInfo>
#include <QHash>
#include <QFileDialog>
#include <QMimeData>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QStackedWidget>
#include <QStorageInfo>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Volchay Cleans"));
    resize(1280, 820);
    setAcceptDrops(true);
    buildUi();

    m_runner = new ScanRunner(this);
    connect(m_runner, &ScanRunner::progress,  this, &MainWindow::onScanProgress);
    connect(m_runner, &ScanRunner::finished,  this, &MainWindow::onScanFinished);
    connect(m_runner, &ScanRunner::cancelled, this, &MainWindow::onScanCancelled);
    connect(m_runner, &ScanRunner::failed,    this, &MainWindow::onScanFailed);

    populateDriveCombo();
    m_overview->refresh();

    connect(&I18n::instance(), &I18n::languageChanged, this,
            [this] { retranslate(); populateDriveCombo();
                     retranslateTrayIcon(); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](ThemeManager::Theme) { applyRgbVisuals(); });
    applyRgbVisuals();
    buildTrayIcon();
    retranslate();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_sidebar = new Sidebar(this);
    root->addWidget(m_sidebar);
    connect(m_sidebar, &Sidebar::sectionChanged, this, [this](Sidebar::SectionId id) {
        onSectionChanged(static_cast<int>(id));
    });
    connect(m_sidebar, &Sidebar::themeToggleRequested,
            this, &MainWindow::onThemeToggleRequested);

    // The main column holds the top bar, the page stack, and (at the very
    // bottom) the rainbow stripe.
    auto* mainCol = new QWidget(this);
    mainCol->setObjectName("MainColumn");
    m_mainCol = mainCol;
    mainCol->installEventFilter(this);
    auto* mainLayout = new QVBoxLayout(mainCol);
    mainLayout->setContentsMargins(28, 22, 28, 0);
    mainLayout->setSpacing(18);

    // Top bar.
    auto* topBar = new QWidget(mainCol);
    topBar->setObjectName("TopBar");
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(16, 12, 16, 12);
    topLayout->setSpacing(10);

    m_driveLabel = new QLabel(topBar);
    m_driveLabel->setObjectName("MutedLabel");
    m_driveCombo = new QComboBox(topBar);
    m_driveCombo->setMinimumWidth(160);
    connect(m_driveCombo, &QComboBox::currentTextChanged, this, [this](const QString&) {
        const QVariant data = m_driveCombo->currentData();
        if (data.isValid()) {
            onDriveSelected(data.toString());
        }
    });

    m_pathEdit = new QLineEdit(topBar);
    // Compact-but-readable input. We clamp the maximum width so the path
    // input doesn't dominate the top bar; the height stays at the QSS-driven
    // natural size so the text never gets clipped vertically.
    m_pathEdit->setMinimumHeight(28);
    m_pathEdit->setMaximumWidth(360);
    // Cursor follows the text edge instead of sitting at the start, so a long
    // path shows its filename rather than the drive root.
    connect(m_pathEdit, &QLineEdit::textChanged, m_pathEdit,
            [this](const QString&) {
                m_pathEdit->setToolTip(m_pathEdit->text());
                m_pathEdit->setCursorPosition(m_pathEdit->text().size());
            });

    m_browseBtn = new QPushButton(topBar);
    m_browseBtn->setObjectName("SecondaryButton");
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::onChooseFolder);

    m_scanBtn = new QPushButton(topBar);
    m_scanBtn->setObjectName("PrimaryButton");
    connect(m_scanBtn, &QPushButton::clicked, this, &MainWindow::onStartScan);

    m_cancelBtn = new QPushButton(topBar);
    m_cancelBtn->setObjectName("SecondaryButton");
    m_cancelBtn->setVisible(false);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelScan);

    m_spinner = new SpinnerWidget(topBar);
    m_spinner->setFixedSize(18, 18);
    m_spinner->setVisible(false);

    m_statusLabel = new QLabel(topBar);
    m_statusLabel->setObjectName("MutedLabel");

    topLayout->addWidget(m_driveLabel);
    topLayout->addWidget(m_driveCombo);
    topLayout->addSpacing(8);
    topLayout->addWidget(m_pathEdit, 1);
    topLayout->addWidget(m_browseBtn);
    topLayout->addWidget(m_scanBtn);
    topLayout->addWidget(m_cancelBtn);
    topLayout->addSpacing(6);
    topLayout->addWidget(m_spinner);
    topLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(topBar);

    m_stack = new QStackedWidget(mainCol);
    m_stack->setObjectName("ContentStack");

    m_overview = new OverviewView(this);
    m_analyzer = new AnalyzerView(this);
    m_files    = new FilesView(this);
    m_recent   = new RecentView(this);
    m_settings = new SettingsView(this);
    connect(m_overview, &OverviewView::driveActivated, this, [this](const QString& root) {
        m_pathEdit->setText(root);
        m_sidebar->setActiveSection(Sidebar::Analyzer);
        onSectionChanged(Sidebar::Analyzer);
        onStartScan();
    });
    connect(m_overview, &OverviewView::configureRequested, this, [this] {
        m_sidebar->setActiveSection(Sidebar::Settings);
        m_stack->setCurrentIndex(Sidebar::Settings);
        m_settings->focusOverviewSection();
    });
    connect(m_settings, &SettingsView::overviewSettingsChanged, this, [this] {
        m_overview->refresh();
    });

    m_stack->addWidget(m_overview);
    m_stack->addWidget(m_analyzer);
    m_stack->addWidget(m_files);
    m_stack->addWidget(m_recent);
    m_stack->addWidget(m_settings);
    mainLayout->addWidget(m_stack, 1);

    // Rainbow stripe runs along the very bottom of the main column.
    m_stripe = new RgbStripe(mainCol);
    mainLayout->addWidget(m_stripe);

    root->addWidget(mainCol, 1);

    // Glow overlay sits on top of the page stack but ignores mouse events;
    // it activates only when the RGB theme is selected.
    m_glow = new RgbGlowOverlay(mainCol);
    m_glow->setActive(false);
    m_glow->raise();

    // Radar overlay sits on top of everything during a scan.
    m_radar = new RadarOverlay(mainCol);
    m_radar->raise();
    {
        QSettings s;
        const bool radarOn =
            s.value(QStringLiteral("radarEnabled"), true).toBool();
        m_radar->setAnimationEnabled(radarOn);
    }
    connect(m_radar, &RadarOverlay::animationDisabled, this, [] {
        QSettings s;
        s.setValue(QStringLiteral("radarEnabled"), false);
    });
}

void MainWindow::retranslate()
{
    m_driveLabel->setText(tr_("Drive:"));
    m_pathEdit->setPlaceholderText(tr_("Path to scan…"));
    m_browseBtn->setText(tr_("Browse…"));
    m_scanBtn->setText(tr_("Scan"));
    m_cancelBtn->setText(tr_("Cancel"));
    if (!m_busy) {
        m_statusLabel->setText(tr_("Ready"));
    }
}

void MainWindow::applyRgbVisuals()
{
    const bool rgb = ThemeManager::instance().current() == ThemeManager::Rgb;
    if (m_glow) m_glow->setActive(rgb);
    if (m_stripe) m_stripe->setVisible(true);  // stripe is always shown
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_mainCol && event->type() == QEvent::Resize) {
        if (m_radar) {
            m_radar->setGeometry(0, 0, m_mainCol->width(), m_mainCol->height());
        }
        if (m_glow) {
            m_glow->setGeometry(0, 0, m_mainCol->width(), m_mainCol->height());
            m_glow->raise();
            if (m_radar) m_radar->raise();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::populateDriveCombo()
{
    m_driveCombo->blockSignals(true);
    const QString prev = m_driveCombo->currentData().toString();
    m_driveCombo->clear();
    const auto drives = Drives::enumerate();
    for (const DriveInfo& d : drives) {
        const QString labelText = d.label.isEmpty()
            ? d.rootPath
            : QString::fromUtf8("%1 (%2)").arg(d.rootPath, d.label);
        m_driveCombo->addItem(labelText, d.rootPath);
    }
    if (!prev.isEmpty()) {
        for (int i = 0; i < m_driveCombo->count(); ++i) {
            if (m_driveCombo->itemData(i).toString() == prev) {
                m_driveCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    m_driveCombo->blockSignals(false);

    if (m_driveCombo->count() > 0 && m_pathEdit->text().isEmpty()) {
        m_pathEdit->setText(m_driveCombo->itemData(0).toString());
    }
}

void MainWindow::onSectionChanged(int section)
{
    m_stack->setCurrentIndex(section);
    if (section == Sidebar::Overview) {
        m_overview->refresh();
    }
}

void MainWindow::onThemeToggleRequested()
{
    auto* app = qobject_cast<QApplication*>(QApplication::instance());
    ThemeManager::instance().toggle(app);
}

void MainWindow::onChooseFolder()
{
    const QString d = QFileDialog::getExistingDirectory(
        this, tr_("Pick a folder"),
        m_pathEdit->text().isEmpty() ? QDir::homePath() : m_pathEdit->text());
    if (!d.isEmpty()) {
        m_pathEdit->setText(d);
    }
}

void MainWindow::onDriveSelected(const QString& path)
{
    if (!m_busy) {
        m_pathEdit->setText(path);
    }
}

void MainWindow::onStartScan()
{
    if (m_busy) {
        return;
    }
    const QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        m_statusLabel->setText(tr_("Specify a path to scan"));
        return;
    }
    QSettings s;
    const QStringList excluded = s.value(QStringLiteral("excludedPaths"))
                                     .toStringList();

    // If the scan target is a drive root, we know the used bytes and can
    // drive the spinner with a real percentage.  Otherwise we asymptote.
    m_scanTargetBytes = 0;
    for (const DriveInfo& di : Drives::enumerate()) {
        if (QString::compare(di.rootPath, path, Qt::CaseInsensitive) == 0) {
            m_scanTargetBytes = di.usedBytes();
            break;
        }
    }
    if (m_spinner) {
        m_spinner->setProgress(0.0);
    }

    setBusy(true, tr_("Scanning…"));
    SoundEngine::instance().play(SoundEngine::ScanStart);
    if (m_radar) {
        QSettings st;
        const bool radarOn =
            st.value(QStringLiteral("radarEnabled"), true).toBool();
        m_radar->setAnimationEnabled(radarOn);
        if (radarOn) {
            m_radar->setGeometry(0, 0, m_mainCol->width(), m_mainCol->height());
            m_radar->start(path);
        }
    }
    m_runner->start(path, excluded);
}

void MainWindow::onCancelScan()
{
    m_runner->cancel();
    m_statusLabel->setText(tr_("Cancelling…"));
}

void MainWindow::onScanProgress(qint64 files, qint64 size, const QString& dir,
                                const QStringList& recentFiles)
{
    m_statusLabel->setText(tr_("Files: %1 · %2")
                               .arg(QLocale().toString(files),
                                    SizeFormatter::humanReadable(size)));
    if (m_radar && m_radar->isVisible()) {
        m_radar->updateStats(files, size, dir);
        m_radar->addRecentFiles(recentFiles);
    }

    if (m_spinner) {
        qreal frac = 0.0;
        if (m_scanTargetBytes > 0) {
            frac = qreal(size) / qreal(m_scanTargetBytes);
        } else {
            // Unknown total (arbitrary folder): asymptote toward 0.97 using
            // file count.  Visually feels like a fill that slows near the end.
            const qreal k = 8000.0;  // ~80% reached at 16k files
            frac = 0.97 * (1.0 - std::exp(-qreal(files) / k));
        }
        // Cap at 0.97 mid-scan so the final pop to 1.0 lands on completion.
        m_spinner->setProgress(qBound<qreal>(0.0, frac, 0.97));
    }
}

void MainWindow::onScanFinished(ScanResult r)
{
    if (m_spinner) {
        m_spinner->setProgress(1.0);
    }
    setBusy(false);
    m_statusLabel->setText(tr_("Done: %1 files · %2")
                               .arg(QLocale().toString(qlonglong(r.totalFiles)),
                                    SizeFormatter::humanReadable(r.totalSize)));
    if (m_radar) m_radar->stop();
    SoundEngine::instance().play(SoundEngine::ScanComplete);

    // Aggregate by category, then keep the top-3 for the Overview "By type"
    // bar chart on the Last-scan card.
    QHash<int, qint64> sizeByCat;
    for (const FileEntry& f : r.files) {
        sizeByCat[int(f.category)] += f.size;
    }
    QVector<QPair<int, qint64>> cats;
    cats.reserve(sizeByCat.size());
    for (auto it = sizeByCat.constBegin(); it != sizeByCat.constEnd(); ++it) {
        cats.push_back({ it.key(), it.value() });
    }
    std::sort(cats.begin(), cats.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    QStringList topCatNames;
    QStringList topCatSizes;
    const int kTopCats = 3;
    for (int i = 0; i < std::min<int>(kTopCats, cats.size()); ++i) {
        topCatNames << FileType::displayName(static_cast<FileCategory>(cats[i].first));
        topCatSizes << QString::number(cats[i].second);
    }

    // Top-5 largest files in the scan.
    QVector<const FileEntry*> sorted;
    sorted.reserve(r.files.size());
    for (const FileEntry& f : r.files) sorted.push_back(&f);
    const int kTopFiles = 5;
    std::partial_sort(sorted.begin(),
                      sorted.begin() + std::min<int>(kTopFiles, sorted.size()),
                      sorted.end(),
                      [](const FileEntry* a, const FileEntry* b) {
                          return a->size > b->size;
                      });
    QStringList topFilePaths;
    QStringList topFileSizes;
    for (int i = 0; i < std::min<int>(kTopFiles, sorted.size()); ++i) {
        topFilePaths << sorted[i]->path;
        topFileSizes << QString::number(sorted[i]->size);
    }

    // File age histogram (Recent ≤7d / ≤30d / ≤365d / Older).
    qint64 ageBuckets[4] = { 0, 0, 0, 0 };
    qint64 ageBucketSizes[4] = { 0, 0, 0, 0 };
    const QDateTime now = QDateTime::currentDateTime();
    for (const FileEntry& f : r.files) {
        const qint64 days = f.modified.isValid()
                                ? f.modified.daysTo(now)
                                : 9999;
        int bucket = 3;
        if (days <= 7) bucket = 0;
        else if (days <= 30) bucket = 1;
        else if (days <= 365) bucket = 2;
        ageBuckets[bucket] += 1;
        ageBucketSizes[bucket] += f.size;
    }
    QStringList ageCounts;
    QStringList ageSizes;
    for (int i = 0; i < 4; ++i) {
        ageCounts << QString::number(ageBuckets[i]);
        ageSizes  << QString::number(ageBucketSizes[i]);
    }

    // File size buckets (<1MB / 1MB-100MB / 100MB-1GB / >1GB).
    qint64 sizeBuckets[4] = { 0, 0, 0, 0 };
    qint64 sizeBucketSizes[4] = { 0, 0, 0, 0 };
    constexpr qint64 kMB = 1024LL * 1024LL;
    constexpr qint64 kGB = 1024LL * kMB;
    for (const FileEntry& f : r.files) {
        int bucket = 3;
        if (f.size < kMB) bucket = 0;
        else if (f.size < 100 * kMB) bucket = 1;
        else if (f.size < kGB) bucket = 2;
        sizeBuckets[bucket] += 1;
        sizeBucketSizes[bucket] += f.size;
    }
    QStringList sizeCounts;
    QStringList sizeSizes;
    for (int i = 0; i < 4; ++i) {
        sizeCounts << QString::number(sizeBuckets[i]);
        sizeSizes  << QString::number(sizeBucketSizes[i]);
    }

    // Aggregate stats: file size summary (avg / median / min / max).
    qint64 minFileSize = 0;
    qint64 maxFileSize = 0;
    qint64 avgFileSize = 0;
    qint64 medianFileSize = 0;
    if (!r.files.isEmpty()) {
        QVector<qint64> sizes;
        sizes.reserve(r.files.size());
        for (const FileEntry& f : r.files) sizes.push_back(f.size);
        std::sort(sizes.begin(), sizes.end());
        minFileSize = sizes.first();
        maxFileSize = sizes.last();
        avgFileSize = r.totalFiles > 0 ? r.totalSize / r.totalFiles : 0;
        medianFileSize = sizes.size() & 1
            ? sizes[sizes.size() / 2]
            : (sizes[sizes.size() / 2 - 1] + sizes[sizes.size() / 2]) / 2;
    }

    // Top file extensions by aggregate size (top-6 to give the chart
    // breathing room without flooding the card).
    QHash<QString, QPair<qint64, qint64>> extAgg;  // ext → (count, totalSize)
    for (const FileEntry& f : r.files) {
        const int dot = f.name.lastIndexOf(QLatin1Char('.'));
        QString ext;
        if (dot > 0 && dot < f.name.size() - 1) {
            ext = QStringLiteral(".") + f.name.mid(dot + 1).toLower();
        }
        auto& v = extAgg[ext];
        v.first  += 1;
        v.second += f.size;
    }
    QVector<QPair<QString, QPair<qint64, qint64>>> extList;
    extList.reserve(extAgg.size());
    for (auto it = extAgg.constBegin(); it != extAgg.constEnd(); ++it) {
        extList.push_back({ it.key(), it.value() });
    }
    std::sort(extList.begin(), extList.end(),
              [](const auto& a, const auto& b) {
                  return a.second.second > b.second.second;
              });
    const int kTopExts = 6;
    QStringList topExtNames;
    QStringList topExtSizes;
    QStringList topExtCounts;
    for (int i = 0; i < std::min<int>(kTopExts, extList.size()); ++i) {
        topExtNames  << extList[i].first;
        topExtCounts << QString::number(extList[i].second.first);
        topExtSizes  << QString::number(extList[i].second.second);
    }
    const qlonglong uniqExts = qlonglong(extAgg.size());

    // Free / total bytes on the volume that contains the scanned root —
    // helpful context for "you scanned 250 GB, your D: still has 1.2 TB free".
    qint64 rootFree = -1;
    qint64 rootTotal = -1;
    {
        QStorageInfo si(r.rootPath);
        if (si.isValid() && si.isReady()) {
            rootFree  = si.bytesAvailable();
            rootTotal = si.bytesTotal();
        }
    }

    QSettings s;
    s.setValue(QStringLiteral("lastScan/path"), r.rootPath);
    s.setValue(QStringLiteral("lastScan/files"), qlonglong(r.totalFiles));
    s.setValue(QStringLiteral("lastScan/size"), qlonglong(r.totalSize));
    s.setValue(QStringLiteral("lastScan/dirs"), qlonglong(r.totalDirs));
    s.setValue(QStringLiteral("lastScan/when"), QDateTime::currentDateTime());
    s.setValue(QStringLiteral("lastScan/topCatNames"), topCatNames);
    s.setValue(QStringLiteral("lastScan/topCatSizes"), topCatSizes);
    s.setValue(QStringLiteral("lastScan/topFilePaths"), topFilePaths);
    s.setValue(QStringLiteral("lastScan/topFileSizes"), topFileSizes);
    s.setValue(QStringLiteral("lastScan/ageCounts"), ageCounts);
    s.setValue(QStringLiteral("lastScan/ageSizes"),  ageSizes);
    s.setValue(QStringLiteral("lastScan/sizeCounts"), sizeCounts);
    s.setValue(QStringLiteral("lastScan/sizeSizes"),  sizeSizes);
    s.setValue(QStringLiteral("lastScan/avgSize"),    qlonglong(avgFileSize));
    s.setValue(QStringLiteral("lastScan/medianSize"), qlonglong(medianFileSize));
    s.setValue(QStringLiteral("lastScan/minSize"),    qlonglong(minFileSize));
    s.setValue(QStringLiteral("lastScan/maxSize"),    qlonglong(maxFileSize));
    s.setValue(QStringLiteral("lastScan/uniqExts"),   uniqExts);
    s.setValue(QStringLiteral("lastScan/topExtNames"),  topExtNames);
    s.setValue(QStringLiteral("lastScan/topExtSizes"),  topExtSizes);
    s.setValue(QStringLiteral("lastScan/topExtCounts"), topExtCounts);
    s.setValue(QStringLiteral("lastScan/rootFree"),  qlonglong(rootFree));
    s.setValue(QStringLiteral("lastScan/rootTotal"), qlonglong(rootTotal));

    m_analyzer->setScanResult(r);
    m_files->setScanResult(r);
    m_recent->setScanResult(r);
    m_overview->refresh();
    m_sidebar->setActiveSection(Sidebar::Analyzer);
    m_stack->setCurrentIndex(Sidebar::Analyzer);
}

void MainWindow::onScanCancelled()
{
    setBusy(false);
    m_statusLabel->setText(tr_("Scan cancelled"));
    if (m_radar) m_radar->stop();
    SoundEngine::instance().play(SoundEngine::ScanCancel);
}

void MainWindow::onScanFailed(const QString& msg)
{
    setBusy(false);
    m_statusLabel->setText(tr_("Error: %1").arg(msg));
    if (m_radar) m_radar->stop();
    SoundEngine::instance().play(SoundEngine::Error);
}

void MainWindow::setBusy(bool busy, const QString& message)
{
    m_busy = busy;
    m_scanBtn->setVisible(!busy);
    m_cancelBtn->setVisible(busy);
    m_browseBtn->setEnabled(!busy);
    m_pathEdit->setEnabled(!busy);
    m_driveCombo->setEnabled(!busy);
    m_spinner->setVisible(busy);
    if (busy) {
        m_spinner->start();
    } else {
        m_spinner->stop();
        // Reset to indeterminate mode for the next idle-time spinner usage.
        m_spinner->resetProgress();
    }
    if (!message.isEmpty()) {
        m_statusLabel->setText(message);
    }
}

void MainWindow::buildTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    // Reuse the embedded SVG application logo so the tray icon matches the
    // rest of the branding.  Fall back to the OS "computer" icon if the
    // resource somehow fails to load.
    QIcon icon(QStringLiteral(":/icons/logo.svg"));
    if (icon.isNull()) {
        icon = style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    setWindowIcon(icon);

    m_tray = new QSystemTrayIcon(icon, this);
    m_tray->setIcon(icon);

    m_trayMenu = new QMenu(this);
    m_trayOpenAct = m_trayMenu->addAction(QString());
    m_trayMenu->addSeparator();
    m_trayQuitAct = m_trayMenu->addAction(QString());
    m_tray->setContextMenu(m_trayMenu);

    connect(m_trayOpenAct, &QAction::triggered, this, [this] {
        showFromTray();
    });
    connect(m_trayQuitAct, &QAction::triggered, this, [this] {
        m_quittingFromTray = true;
        QApplication::quit();
    });

    // Left-click (or double-click on Windows) toggles window visibility,
    // matching the behaviour every other Windows tray app uses.
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                switch (reason) {
                    case QSystemTrayIcon::Trigger:
                    case QSystemTrayIcon::DoubleClick:
                    case QSystemTrayIcon::MiddleClick:
                        if (isVisible() && !isMinimized()) {
                            hide();
                        } else {
                            showFromTray();
                        }
                        break;
                    default:
                        break;
                }
            });

    retranslateTrayIcon();
    m_tray->show();
}

void MainWindow::retranslateTrayIcon()
{
    if (!m_tray) return;
    m_tray->setToolTip(QStringLiteral("Volchay Cleans"));
    if (m_trayOpenAct) m_trayOpenAct->setText(tr_("Open"));
    if (m_trayQuitAct) m_trayQuitAct->setText(tr_("Exit"));
}

void MainWindow::showFromTray()
{
    if (isMinimized()) {
        showNormal();
    } else {
        show();
    }
    raise();
    activateWindow();
}

void MainWindow::activateFromBackground()
{
    // Public entry point used by the single-instance hand-off and the
    // tray "Open" item.  The behaviour is intentionally identical to
    // showFromTray() so the user gets the same effect regardless of
    // which path triggered it.
    showFromTray();
}

void MainWindow::closeEvent(QCloseEvent* ev)
{
    // Quitting from the tray's "Exit" item — always honour it.
    if (m_quittingFromTray) {
        QMainWindow::closeEvent(ev);
        return;
    }

    // Honour the user-configurable close-behaviour. We default to "tray"
    // when a tray is available so first-run users still get the v0.2
    // behaviour, and to "exit" when no tray exists (Linux without a
    // system tray, etc.).
    const bool trayAvailable =
        m_tray && QSystemTrayIcon::isSystemTrayAvailable();
    QSettings s;
    const QString defaultBehavior =
        trayAvailable ? QStringLiteral("tray") : QStringLiteral("exit");
    const QString behavior =
        s.value(QStringLiteral("closeBehavior"), defaultBehavior).toString();

    if (behavior != QStringLiteral("tray") || !trayAvailable) {
        QMainWindow::closeEvent(ev);
        return;
    }

    // Hide to tray instead of quitting.  The first time we do this we
    // show a balloon hint so the user knows where the window went, then
    // remember that fact (per-install) so we don't pester them again.
    if (!m_shownTrayHint &&
        !s.value(QStringLiteral("trayHintShown"), false).toBool()) {
        m_tray->showMessage(QStringLiteral("Volchay Cleans"),
                            tr_("Still running here. Right-click the tray "
                                "icon for Open or Exit."),
                            QSystemTrayIcon::Information,
                            4000);
        m_shownTrayHint = true;
        s.setValue(QStringLiteral("trayHintShown"), true);
    }
    hide();
    ev->ignore();
}

namespace {
// Pick a folder to scan from a list of dropped URLs. We accept either a
// folder (used directly) or a file (its containing directory is used).
// Multi-drop falls back to the first usable URL — running several scans
// in parallel is not what the user expects from a tool that has one
// "current scan" concept.
QString scanTargetFromUrls(const QList<QUrl>& urls)
{
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) continue;
        const QString local = url.toLocalFile();
        if (local.isEmpty()) continue;
        QFileInfo fi(local);
        if (fi.isDir())  return fi.absoluteFilePath();
        if (fi.isFile()) return fi.absolutePath();
    }
    return {};
}
}  // namespace

void MainWindow::dragEnterEvent(QDragEnterEvent* ev)
{
    if (m_busy) {  // a scan is already running — don't accept new targets
        ev->ignore();
        return;
    }
    if (!ev->mimeData() || !ev->mimeData()->hasUrls()) {
        ev->ignore();
        return;
    }
    if (scanTargetFromUrls(ev->mimeData()->urls()).isEmpty()) {
        ev->ignore();
        return;
    }
    ev->setDropAction(Qt::LinkAction);
    ev->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent* ev)
{
    if (m_busy || !ev->mimeData() || !ev->mimeData()->hasUrls()) {
        ev->ignore();
        return;
    }
    ev->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* ev)
{
    if (m_busy || !ev->mimeData() || !ev->mimeData()->hasUrls()) {
        ev->ignore();
        return;
    }
    const QString target = scanTargetFromUrls(ev->mimeData()->urls());
    if (target.isEmpty()) {
        ev->ignore();
        return;
    }
    ev->acceptProposedAction();
    if (m_pathEdit) m_pathEdit->setText(target);
    onStartScan();
}
