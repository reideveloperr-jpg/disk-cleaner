#pragma once

#include "core/ScanResult.h"

#include <QWidget>

class QComboBox;
class QTreeView;
class QLabel;
class QLineEdit;
class QPushButton;
class FilterChips;
class FolderTreeModel;
class FolderFilterProxyModel;

class AnalyzerView : public QWidget {
    Q_OBJECT
public:
    explicit AnalyzerView(QWidget* parent = nullptr);

    void setScanResult(const ScanResult& r);
    void clear();

signals:
    // Emitted when the user removed a path; host should refresh state.
    void pathDeleted();

private slots:
    void onSelectionChanged();
    void onDeleteClicked();
    void onRevealClicked();
    void onOpenClicked();
    void onRefreshClicked();
    void onTreeContextMenu(const QPoint& pos);

private:
    void retranslate();
    QString currentSelectedPath() const;
    bool isPathMarkedDeleted(const QString& path) const;
    void restoreSelectedFromTrash();
    void rebuildSummary();
    void populateSortCombo();
    void applySortFromCombo();
    // Returns true if `path` is still on disk; otherwise warns the user and
    // marks the row as deleted so Refresh can clean it up.  Used before any
    // action that touches the filesystem (Open/Reveal/Delete) so a stale
    // cache after an external move never produces a raw "code 2" error.
    bool ensurePathExistsOrMark(const QString& path);

    QLabel* m_titleLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_sortCombo = nullptr;
    QPushButton* m_revealBtn = nullptr;
    QPushButton* m_openBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    FilterChips* m_chips = nullptr;
    QTreeView* m_tree = nullptr;
    FolderTreeModel* m_model = nullptr;
    FolderFilterProxyModel* m_proxy = nullptr;
    QString m_rootPath;
    int m_lastFolderCount = 0;
};
