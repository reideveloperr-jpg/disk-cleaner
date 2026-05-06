#include "ScanWorker.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

ScanWorker::ScanWorker(QObject* parent) : QObject(parent) {}

void ScanWorker::cancel()
{
    m_cancelRequested.store(true);
}

bool ScanWorker::isExcluded(const QString& absPath) const
{
    // Match prefixes only at directory boundaries: excluding "C:/Users"
    // must not also drop "C:/UsersBackup" or "C:/Users2".
    for (const QString& ex : m_excluded) {
        if (!absPath.startsWith(ex, Qt::CaseInsensitive)) {
            continue;
        }
        if (absPath.size() == ex.size() ||
            absPath[ex.size()] == QLatin1Char('/') ||
            absPath[ex.size()] == QLatin1Char('\\')) {
            return true;
        }
    }
    return false;
}

void ScanWorker::scanDirectory(const QString& dirPath, FolderNode& node, ScanResult& out)
{
    if (m_cancelRequested.load()) {
        return;
    }

    const QFileInfo dirInfo(dirPath);
    node.path = dirPath;
    node.name = dirInfo.fileName();
    if (node.name.isEmpty()) {
        node.name = dirPath;
    }
    // Carry the directory's own filesystem timestamps so the folder tree
    // can render them next to per-file rows. On Windows all three are
    // populated; on Linux birthTime() may be invalid — the model checks
    // isValid() and renders an empty cell in that case.
    node.modified = dirInfo.lastModified();
    node.created  = dirInfo.birthTime();
    node.accessed = dirInfo.lastRead();

    QDir dir(dirPath);
    const auto entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot |
            QDir::Hidden | QDir::System | QDir::NoSymLinks,
        QDir::NoSort);

    for (const QFileInfo& fi : entries) {
        if (m_cancelRequested.load()) {
            return;
        }
        const QString abs = fi.absoluteFilePath();
        if (isExcluded(abs)) {
            continue;
        }
        if (fi.isSymLink()) {
            continue;
        }

        if (fi.isDir()) {
            FolderNode child;
            scanDirectory(abs, child, out);
            node.totalSize += child.totalSize;
            node.totalFiles += child.totalFiles;
            out.totalDirs += 1;
            node.children.push_back(std::move(child));
        } else if (fi.isFile()) {
            FileEntry e;
            e.path = abs;
            e.name = fi.fileName();
            e.size = fi.size();
            e.modified = fi.lastModified();
            e.created = fi.birthTime();
            e.accessed = fi.lastRead();
            e.isHidden = fi.isHidden();
            e.isReadOnly = !fi.isWritable();
            e.isSystem = false;  // populated on Windows below if available
            e.category = FileType::classify(abs);
            out.files.push_back(e);
            out.totalFiles += 1;
            out.totalSize += e.size;
            node.totalSize += e.size;
            node.totalFiles += 1;
            node.directFiles += 1;

            // Keep at most ~30 recent file paths in the batch — the radar
            // overlay samples them as blips.
            m_recentBatch.append(abs);
            if (m_recentBatch.size() > 30) {
                m_recentBatch.removeFirst();
            }

            maybeEmitProgress(out.totalFiles, out.totalSize, dirPath);
        }
    }
}

void ScanWorker::maybeEmitProgress(qint64 totalFiles, qint64 totalSize,
                                   const QString& dir)
{
    if (!m_progressTimer.isValid()) {
        m_progressTimer.start();
    }
    if (m_progressTimer.elapsed() < 80) {
        return;
    }
    emit progress(totalFiles, totalSize, dir, m_recentBatch);
    m_recentBatch.clear();
    m_progressTimer.restart();
}

void ScanWorker::run()
{
    if (m_root.isEmpty() || !QFileInfo(m_root).exists()) {
        emit failed(QStringLiteral("Path does not exist: %1").arg(m_root));
        return;
    }

    ScanResult result;
    result.rootPath = m_root;
    result.root = FolderNode{};
    result.totalDirs = 1;

    scanDirectory(m_root, result.root, result);

    if (m_cancelRequested.load()) {
        emit cancelled();
        return;
    }

    emit progress(result.totalFiles, result.totalSize, m_root, m_recentBatch);
    m_recentBatch.clear();
    emit finished(std::move(result));
}

// ---------------- ScanRunner ----------------

ScanRunner::ScanRunner(QObject* parent) : QObject(parent) {}

ScanRunner::~ScanRunner()
{
    cancel();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(2000);
    }
}

void ScanRunner::start(const QString& root, const QStringList& excluded)
{
    if (m_running) {
        cancel();
    }
    m_thread = new QThread(this);
    m_worker = new ScanWorker;
    m_worker->moveToThread(m_thread);
    m_worker->setRootPath(root);
    m_worker->setExcludedPaths(excluded);

    connect(m_thread, &QThread::started, m_worker, &ScanWorker::run);
    connect(m_worker, &ScanWorker::progress,  this, &ScanRunner::progress);
    connect(m_worker, &ScanWorker::finished,  this, [this](ScanResult r) {
        m_running = false;
        emit finished(std::move(r));
        m_thread->quit();
    });
    connect(m_worker, &ScanWorker::cancelled, this, [this]() {
        m_running = false;
        emit cancelled();
        m_thread->quit();
    });
    connect(m_worker, &ScanWorker::failed,    this, [this](const QString& msg) {
        m_running = false;
        emit failed(msg);
        m_thread->quit();
    });
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    // Once Qt destroys the thread/worker via deleteLater above, null the
    // raw pointers so ~ScanRunner() and cancel() don't dereference a
    // dangling QThread* / ScanWorker*.
    connect(m_thread, &QObject::destroyed, this, [this] { m_thread = nullptr; });
    connect(m_worker, &QObject::destroyed, this, [this] { m_worker = nullptr; });

    m_running = true;
    m_thread->start();
}

void ScanRunner::cancel()
{
    if (m_worker && m_running) {
        m_worker->cancel();
    }
}
