#pragma once

#include "core/ScanResult.h"
#include "core/ScanWorker.h"

#include <QMainWindow>

class Sidebar;
class OverviewView;
class AnalyzerView;
class FilesView;
class RecentView;
class SettingsView;
class QStackedWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class SpinnerWidget;
class QComboBox;
class RadarOverlay;
class RgbStripe;
class RgbGlowOverlay;
class QSystemTrayIcon;
class QMenu;
class QAction;
class QCloseEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

public slots:
    // Bring the window back from tray / minimised state and raise it to
    // the foreground.  Called both from the system tray menu and from
    // the single-instance hand-off in main() when the user launches the
    // executable a second time.
    void activateFromBackground();

private slots:
    void onSectionChanged(int section);
    void onThemeToggleRequested();
    void onChooseFolder();
    void onStartScan();
    void onCancelScan();
    void onScanProgress(qint64 files, qint64 size, const QString& dir,
                        const QStringList& recentFiles);
    void onScanFinished(ScanResult r);
    void onScanCancelled();
    void onScanFailed(const QString& msg);
    void onDriveSelected(const QString& path);

private:
    void buildUi();
    void setBusy(bool busy, const QString& message = {});
    void populateDriveCombo();
    void retranslate();
    void applyRgbVisuals();
    void buildTrayIcon();
    void retranslateTrayIcon();
    void showFromTray();

    Sidebar* m_sidebar = nullptr;
    QStackedWidget* m_stack = nullptr;
    OverviewView* m_overview = nullptr;
    AnalyzerView* m_analyzer = nullptr;
    FilesView* m_files = nullptr;
    RecentView* m_recent = nullptr;
    SettingsView* m_settings = nullptr;

    // Top bar controls.
    QLabel* m_driveLabel = nullptr;
    QComboBox* m_driveCombo = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;
    QPushButton* m_scanBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    SpinnerWidget* m_spinner = nullptr;
    QLabel* m_statusLabel = nullptr;

    RadarOverlay* m_radar = nullptr;
    RgbGlowOverlay* m_glow = nullptr;
    RgbStripe* m_stripe = nullptr;
    QWidget* m_mainCol = nullptr;

    ScanRunner* m_runner = nullptr;
    bool m_busy = false;

    // System tray. The icon lives for the lifetime of the window; on
    // platforms where QSystemTrayIcon::isSystemTrayAvailable() is false
    // (very rare on Windows / KDE / GNOME) m_tray stays null and the
    // window behaves like a regular QMainWindow.
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_trayMenu = nullptr;
    QAction* m_trayOpenAct = nullptr;
    QAction* m_trayQuitAct = nullptr;
    bool m_quittingFromTray = false;
    bool m_shownTrayHint = false;

    // Estimated total bytes for the active scan (drive used bytes when
    // scanning a drive root, 0 otherwise).  Used to drive the spinner's
    // determinate progress arc.
    qint64 m_scanTargetBytes = 0;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* ev) override;
    // Accept folders / files dropped onto the window from Explorer (or any
    // other app). When a single folder lands, it is loaded into the path
    // field and a scan starts automatically.
    void dragEnterEvent(QDragEnterEvent* ev) override;
    void dragMoveEvent(QDragMoveEvent* ev) override;
    void dropEvent(QDropEvent* ev) override;
};
