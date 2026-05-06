#pragma once

#include "core/HashWorker.h"
#include "core/ScanResult.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QPlainTextEdit;
class SpinnerWidget;

class DetailsPanel : public QWidget {
    Q_OBJECT
public:
    explicit DetailsPanel(QWidget* parent = nullptr);

    void showFile(const FileEntry& f);
    void clear();

private slots:
    void onComputeMd5();
    void onMd5Done(const QString& hash);
    void onMd5Failed(const QString& msg);
    void onCopyPath();
    void onRevealRequested();

private:
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QPlainTextEdit* m_details = nullptr;
    QLabel* m_hashLabel = nullptr;
    QPushButton* m_md5Btn = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_revealBtn = nullptr;
    SpinnerWidget* m_spinner = nullptr;
    HashRunner* m_hash = nullptr;
    FileEntry m_current;
    bool m_hasCurrent = false;
};
