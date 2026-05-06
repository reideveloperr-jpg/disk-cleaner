#pragma once

#include <QObject>
#include <QString>
#include <QThread>

class HashWorker : public QObject {
    Q_OBJECT
public:
    explicit HashWorker(QObject* parent = nullptr);

    void setFilePath(const QString& path) { m_path = path; }

public slots:
    void run();

signals:
    void finished(QString hash);
    void failed(QString message);

private:
    QString m_path;
};

// Helper that runs HashWorker on a dedicated thread and emits the result.
class HashRunner : public QObject {
    Q_OBJECT
public:
    explicit HashRunner(QObject* parent = nullptr);
    ~HashRunner() override;

    void compute(const QString& path);

signals:
    void finished(const QString& hash);
    void failed(const QString& message);

private:
    QThread* m_thread = nullptr;
    HashWorker* m_worker = nullptr;
};
