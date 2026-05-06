#pragma once

#include <QWidget>

class QGridLayout;
class QLabel;
class QPushButton;
class QFrame;

class OverviewView : public QWidget {
    Q_OBJECT
public:
    explicit OverviewView(QWidget* parent = nullptr);

    void refresh();

signals:
    void driveActivated(const QString& rootPath);
    // Emitted when the user clicks the "⚙ Configure" button on the stats
    // card; the host should switch to the Settings page (typically
    // scrolling to the Overview-statistics group).
    void configureRequested();

private:
    void rebuildStatsCard();
    void rebuildLastScanCard();

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QGridLayout* m_grid = nullptr;
    QWidget* m_gridContainer = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_configureBtn = nullptr;

    // Summary stats for all drives at a glance (count, total/used/free bytes,
    // FS-type breakdown, biggest drive).
    QFrame* m_statsCard = nullptr;
    QLabel* m_statsTitle = nullptr;
    QGridLayout* m_statsGrid = nullptr;

    // Last-scan summary card — only visible after the user has run a scan
    // at least once in this session.
    QFrame* m_lastScanCard = nullptr;
    QLabel* m_lastScanTitle = nullptr;
    QLabel* m_lastScanBody = nullptr;
    QGridLayout* m_lastScanCatsGrid = nullptr;
    QGridLayout* m_lastScanTopFilesGrid = nullptr;
    QLabel* m_lastScanCatsTitle = nullptr;
    QLabel* m_lastScanTopFilesTitle = nullptr;
    QLabel* m_ageHistTitle = nullptr;
    QGridLayout* m_ageHistGrid = nullptr;
    QLabel* m_sizeBucketsTitle = nullptr;
    QGridLayout* m_sizeBucketsGrid = nullptr;

    // Per-drive usage bars (drive name on the left, coloured bar in the
    // middle, used/total + percent on the right). Built dynamically.
    QGridLayout* m_drivesBarsGrid = nullptr;
    QLabel* m_drivesBarsTitle = nullptr;

    // Extended statistics row on the Last-scan card: average / median /
    // largest / smallest file size, unique extensions count, etc.  We
    // compute these from FileEntry data on every scan and cache them in
    // QSettings so the Overview page can render them without re-scanning.
    QGridLayout* m_lastScanExtraGrid = nullptr;
    QLabel* m_lastScanExtraTitle = nullptr;

    // Top file extensions by aggregate size (e.g. ".mp4 — 4 entries · 9.2 GB").
    QGridLayout* m_topExtsGrid = nullptr;
    QLabel* m_topExtsTitle = nullptr;
};
