#pragma once

#include "ScanResult.h"

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QThread>
#include <atomic>

class ScanWorker : public QObject {
    Q_OBJECT
public:
    explicit ScanWorker(QObject* parent = nullptr);

    void setRootPath(const QString& root) { m_root = root; }
    void setExcludedPaths(const QStringList& excluded) { m_excluded = excluded; }
    void cancel();

public slots:
    void run();

signals:
    void progress(qint64 filesSeen, qint64 totalSizeSoFar,
                  const QString& currentDir, const QStringList& recentFiles);
    void finished(ScanResult result);
    void cancelled();
    void failed(const QString& message);

private:
    bool isExcluded(const QString& absPath) const;
    void scanDirectory(const QString& dir, FolderNode& node, ScanResult& out);
    void maybeEmitProgress(qint64 totalFiles, qint64 totalSize, const QString& dir);

    QString m_root;
    QStringList m_excluded;
    std::atomic<bool> m_cancelRequested{false};
    QStringList m_recentBatch;       // files discovered since the last emit
    QElapsedTimer m_progressTimer;
};

// Helper to run ScanWorker on a dedicated QThread.
class ScanRunner : public QObject {
    Q_OBJECT
public:
    explicit ScanRunner(QObject* parent = nullptr);
    ~ScanRunner() override;

    void start(const QString& root, const QStringList& excluded);
    void cancel();
    bool isRunning() const { return m_running; }

signals:
    void progress(qint64 filesSeen, qint64 totalSizeSoFar,
                  const QString& currentDir, const QStringList& recentFiles);
    void finished(ScanResult result);
    void cancelled();
    void failed(const QString& message);

private:
    QThread* m_thread = nullptr;
    ScanWorker* m_worker = nullptr;
    bool m_running = false;
};
