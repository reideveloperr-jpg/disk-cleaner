#pragma once

#include <QElapsedTimer>
#include <QQueue>
#include <QString>
#include <QVector>
#include <QWidget>

class QTimer;
class QPushButton;
class QLabel;

// Center-of-screen radar overlay shown during scanning. Files that the scanner
// finds appear as blips on the dial and stream as text above the radar. Hidden
// once scanning ends. Can also be disabled permanently from a button.
class RadarOverlay : public QWidget {
    Q_OBJECT
public:
    explicit RadarOverlay(QWidget* parent);

    void start(const QString& rootPath);
    void stop();
    void updateStats(qint64 filesSeen, qint64 totalSize, const QString& currentDir);
    void addRecentFiles(const QStringList& paths);

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool enabled);

signals:
    void hideRequested();              // hide for the current scan only
    void animationDisabled();          // user disabled radar animation entirely

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private slots:
    void onTick();

private:
    struct Blip {
        double angle;       // radians, 0 = right, increasing CCW
        double radius;      // 0..1 (fraction of dial radius)
        qint64 birthMs;     // QElapsedTimer ms when blip was added
        QString name;       // file basename for the trailing label
    };

    void layoutChildren();
    QRectF dialRect() const;
    QString shorten(const QString& s, int max) const;

    QTimer* m_timer = nullptr;
    QElapsedTimer m_clock;
    double m_sweepAngle = 0.0;       // radians, CCW
    QVector<Blip> m_blips;
    QQueue<QString> m_recentList;    // last N file basenames for the side ribbon
    bool m_animationEnabled = true;
    bool m_running = false;

    qint64 m_filesSeen = 0;
    qint64 m_totalSize = 0;
    QString m_currentDir;
    QString m_rootPath;

    // Inner panel widgets (overlaid on top of the painted radar).
    QPushButton* m_hideBtn = nullptr;
    QPushButton* m_disableBtn = nullptr;
};
