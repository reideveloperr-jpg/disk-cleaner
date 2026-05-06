#include "HashWorker.h"

#include <QCryptographicHash>
#include <QFile>

HashWorker::HashWorker(QObject* parent) : QObject(parent) {}

void HashWorker::run()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit failed(QStringLiteral("Could not open file: %1").arg(m_path));
        return;
    }
    QCryptographicHash hash(QCryptographicHash::Md5);
    constexpr qint64 kChunk = 1 << 20;  // 1 MiB
    while (!f.atEnd()) {
        const QByteArray chunk = f.read(kChunk);
        if (chunk.isEmpty()) {
            break;
        }
        hash.addData(chunk);
    }
    emit finished(QString::fromUtf8(hash.result().toHex()));
}

HashRunner::HashRunner(QObject* parent) : QObject(parent) {}

HashRunner::~HashRunner()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(2000);
    }
}

void HashRunner::compute(const QString& path)
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(2000);
        m_thread = nullptr;
        m_worker = nullptr;
    }
    m_thread = new QThread(this);
    m_worker = new HashWorker;
    m_worker->moveToThread(m_thread);
    m_worker->setFilePath(path);

    connect(m_thread, &QThread::started, m_worker, &HashWorker::run);
    connect(m_worker, &HashWorker::finished, this, [this](QString h) {
        emit finished(h);
        m_thread->quit();
    });
    connect(m_worker, &HashWorker::failed, this, [this](QString m) {
        emit failed(m);
        m_thread->quit();
    });
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    // Null raw pointers when Qt destroys the underlying objects so
    // ~HashRunner() and re-entrant compute() calls don't dereference
    // dangling pointers (deleteLater above hands them back to Qt).
    connect(m_thread, &QObject::destroyed, this, [this] { m_thread = nullptr; });
    connect(m_worker, &QObject::destroyed, this, [this] { m_worker = nullptr; });

    m_thread->start();
}
