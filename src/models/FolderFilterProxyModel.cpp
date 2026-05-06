#include "FolderFilterProxyModel.h"

#include "FolderTreeModel.h"

FolderFilterProxyModel::FolderFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
}

void FolderFilterProxyModel::setCategory(FileCategory cat)
{
    if (m_category == cat) return;
    m_category = cat;
    invalidateFilter();
}

void FolderFilterProxyModel::setSearchText(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (m_search == trimmed) return;
    m_search = trimmed;
    invalidateFilter();
}

bool FolderFilterProxyModel::filterAcceptsRow(int sourceRow,
                                              const QModelIndex& sourceParent) const
{
    auto* model = qobject_cast<FolderTreeModel*>(sourceModel());
    if (!model) return true;
    const QModelIndex idx = model->index(sourceRow, 0, sourceParent);
    if (!idx.isValid()) return true;

    // Folder rows are always accepted so the user can drill down even when
    // the active filter would hide every file inside them.
    if (model->isFolder(idx)) return true;

    if (m_category != FileCategory::All) {
        if (model->categoryAt(idx) != m_category) return false;
    }
    if (!m_search.isEmpty()) {
        const QString name = model->nameAt(idx);
        if (!name.contains(m_search, Qt::CaseInsensitive)) return false;
    }
    return true;
}
