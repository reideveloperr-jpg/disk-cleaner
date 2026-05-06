#pragma once

#include "core/FileType.h"

#include <QSortFilterProxyModel>

class FolderTreeModel;

// Proxy used by AnalyzerView. Folders are always shown so the user can
// keep navigating the tree; file rows are filtered by category and a
// substring match against the file name.
class FolderFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit FolderFilterProxyModel(QObject* parent = nullptr);

    void setCategory(FileCategory cat);
    void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex& sourceParent) const override;

private:
    FileCategory m_category = FileCategory::All;
    QString m_search;
};
