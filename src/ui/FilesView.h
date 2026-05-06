#pragma once

#include "core/ScanResult.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class FilterChips;
class FileTableModel;
class FileFilterProxyModel;
class DetailsPanel;

class FilesView : public QWidget {
    Q_OBJECT
public:
    explicit FilesView(QWidget* parent = nullptr);

    void setScanResult(const ScanResult& r);
    void clear();

signals:
    // Emitted when the user removed file(s); host should re-trigger a scan
    // (or refresh the table) to pick up the new state.
    void filesDeleted();

private slots:
    void onSelectionChanged();
    void onDeleteClicked();
    void onRefreshClicked();
    void onTableContextMenu(const QPoint& pos);

private:
    QString currentSelectedPath() const;
    bool isPathMarkedDeleted(const QString& path) const;
    void restoreSelectedFromTrash();
    void retranslate();
    void refreshSummary();
    void populateSortCombo();
    void applySortFromCombo();
    // Stale-path guard: returns true if the path still exists; otherwise
    // marks the row deleted and tells the user instead of letting
    // ShellOps return a raw "code 2" error.
    bool ensurePathExistsOrMark(const QString& path);

    QLabel* m_titleLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_sortCombo = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    FilterChips* m_chips = nullptr;
    QTableView* m_table = nullptr;
    FileTableModel* m_model = nullptr;
    FileFilterProxyModel* m_proxy = nullptr;
    DetailsPanel* m_details = nullptr;
    QVector<FileEntry> m_files;  // for stats
};
