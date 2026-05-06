#include "FileTableModel.h"

#include "core/SizeFormatter.h"
#include "i18n/I18n.h"

#include <QColor>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QLocale>
#include <QMimeData>
#include <QSet>
#include <QStringList>
#include <QUrl>

namespace {
QIcon iconForCategory(FileCategory c)
{
    static QFileIconProvider provider;
    static QHash<int, QIcon> cache;
    auto it = cache.constFind(int(c));
    if (it != cache.constEnd()) return it.value();
    QIcon icon = provider.icon(QFileIconProvider::File);
    cache.insert(int(c), icon);
    return icon;
}
}  // namespace

FileTableModel::FileTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void FileTableModel::setFiles(QVector<FileEntry> files)
{
    beginResetModel();
    m_files = std::move(files);
    m_deleted.fill(false, m_files.size());
    endResetModel();
}

void FileTableModel::clear()
{
    beginResetModel();
    m_files.clear();
    m_deleted.clear();
    endResetModel();
}

bool FileTableModel::markDeleted(const QString& path)
{
    bool changed = false;
    for (int i = 0; i < m_files.size(); ++i) {
        if (!m_deleted.at(i) && m_files.at(i).path == path) {
            m_deleted[i] = true;
            const QModelIndex left  = index(i, 0);
            const QModelIndex right = index(i, ColumnCount - 1);
            emit dataChanged(left, right,
                             { Qt::FontRole, Qt::ForegroundRole });
            changed = true;
        }
    }
    return changed;
}

bool FileTableModel::clearDeleted(const QString& path)
{
    bool changed = false;
    for (int i = 0; i < m_files.size(); ++i) {
        if (i < m_deleted.size() && m_deleted.at(i) &&
            m_files.at(i).path == path) {
            m_deleted[i] = false;
            const QModelIndex left  = index(i, 0);
            const QModelIndex right = index(i, ColumnCount - 1);
            emit dataChanged(left, right,
                             { Qt::FontRole, Qt::ForegroundRole });
            changed = true;
        }
    }
    return changed;
}

bool FileTableModel::isDeleted(const QString& path) const
{
    for (int i = 0; i < m_files.size(); ++i) {
        if (m_files.at(i).path == path) {
            return i < m_deleted.size() && m_deleted.at(i);
        }
    }
    return false;
}

void FileTableModel::purgeDeleted()
{
    int n = m_files.size();
    for (int i = n - 1; i >= 0; --i) {
        if (m_deleted.at(i)) {
            beginRemoveRows({}, i, i);
            m_files.removeAt(i);
            m_deleted.removeAt(i);
            endRemoveRows();
        }
    }
}

void FileTableModel::purgeDeletedIfMissing()
{
    int n = m_files.size();
    // Pass 1 — revive rows whose file is back on disk.
    for (int i = 0; i < n; ++i) {
        if (i < m_deleted.size() && m_deleted.at(i) &&
            QFileInfo::exists(m_files.at(i).path)) {
            m_deleted[i] = false;
            const QModelIndex left  = index(i, 0);
            const QModelIndex right = index(i, ColumnCount - 1);
            emit dataChanged(left, right,
                             { Qt::FontRole, Qt::ForegroundRole });
        }
    }
    // Pass 2 — drop the rest.
    for (int i = n - 1; i >= 0; --i) {
        if (m_deleted.at(i)) {
            beginRemoveRows({}, i, i);
            m_files.removeAt(i);
            m_deleted.removeAt(i);
            endRemoveRows();
        }
    }
}

int FileTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_files.size();
}

int FileTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant FileTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size()) {
        return {};
    }
    const bool deleted =
        index.row() < m_deleted.size() && m_deleted.at(index.row());
    if (role == Qt::FontRole && deleted) {
        QFont font;
        font.setStrikeOut(true);
        return font;
    }
    if (role == Qt::ForegroundRole && deleted) {
        return QColor(255, 95, 95);
    }
    const FileEntry& f = m_files.at(index.row());
    if (role == Qt::DecorationRole) {
        if (index.column() == ColName) return iconForCategory(f.category);
        return {};
    }
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColName:     return f.name;
            case ColCategory: return FileType::displayName(f.category);
            case ColSize:     return SizeFormatter::humanReadable(f.size);
            case ColModified: return f.modified.isValid()
                                    ? f.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                    : QString();
            case ColCreated:  return f.created.isValid()
                                    ? f.created.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                    : QString();
            case ColAccessed: return f.accessed.isValid()
                                    ? f.accessed.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                    : QString();
            case ColPath:     return f.path;
        }
    } else if (role == Qt::UserRole) {
        // Sort role: provide raw comparable values per column.
        switch (index.column()) {
            case ColName:     return f.name.toLower();
            case ColCategory: return static_cast<int>(f.category);
            case ColSize:     return f.size;
            case ColModified: return f.modified;
            case ColCreated:  return f.created;
            case ColAccessed: return f.accessed;
            case ColPath:     return f.path.toLower();
        }
    } else if (role == Qt::ToolTipRole) {
        return f.path;
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColSize) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    return {};
}

QVariant FileTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) {
        return {};
    }
    if (role == Qt::DisplayRole) {
        switch (section) {
            case ColName:     return tr_("Name");
            case ColCategory: return tr_("Type");
            case ColSize:     return tr_("Size");
            case ColModified: return tr_("Modified");
            case ColCreated:  return tr_("Created");
            case ColAccessed: return tr_("Accessed");
            case ColPath:     return tr_("Path");
        }
    }
    return {};
}

Qt::ItemFlags FileTableModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags base = QAbstractTableModel::flags(index);
    if (!index.isValid()) return base;
    const int row = index.row();
    if (row < 0 || row >= m_files.size()) return base;
    if (row < m_deleted.size() && m_deleted[row]) return base;
    return base | Qt::ItemIsDragEnabled;
}

QStringList FileTableModel::mimeTypes() const
{
    return {QStringLiteral("text/uri-list")};
}

QMimeData* FileTableModel::mimeData(const QModelIndexList& indexes) const
{
    QList<QUrl> urls;
    QSet<int> seen;
    for (const QModelIndex& idx : indexes) {
        if (!idx.isValid()) continue;
        const int row = idx.row();
        if (seen.contains(row)) continue;
        seen.insert(row);
        if (row < 0 || row >= m_files.size()) continue;
        if (row < m_deleted.size() && m_deleted[row]) continue;
        const QString& path = m_files[row].path;
        if (path.isEmpty() || !QFileInfo::exists(path)) continue;
        urls.append(QUrl::fromLocalFile(path));
    }
    if (urls.isEmpty()) return nullptr;
    auto* m = new QMimeData;
    m->setUrls(urls);
    return m;
}

Qt::DropActions FileTableModel::supportedDragActions() const
{
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

// ---------------- Proxy ----------------

FileFilterProxyModel::FileFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(Qt::UserRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setDynamicSortFilter(true);
}

void FileFilterProxyModel::setCategory(FileCategory cat)
{
    if (m_category == cat) {
        return;
    }
    m_category = cat;
    invalidateFilter();
}

void FileFilterProxyModel::setSearchText(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (m_search == trimmed) {
        return;
    }
    m_search = trimmed;
    invalidateFilter();
}

bool FileFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& parent) const
{
    auto* src = qobject_cast<FileTableModel*>(sourceModel());
    if (!src) {
        return true;
    }
    Q_UNUSED(parent);
    const FileEntry& f = src->fileAt(sourceRow);
    if (m_category != FileCategory::All && f.category != m_category) {
        return false;
    }
    if (!m_search.isEmpty() && !f.name.contains(m_search, Qt::CaseInsensitive) &&
        !f.path.contains(m_search, Qt::CaseInsensitive)) {
        return false;
    }
    return true;
}

bool FileFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QVariant l = sourceModel()->data(left, Qt::UserRole);
    const QVariant r = sourceModel()->data(right, Qt::UserRole);
    if (l.userType() == QMetaType::LongLong || l.userType() == QMetaType::Int) {
        return l.toLongLong() < r.toLongLong();
    }
    if (l.userType() == QMetaType::QDateTime) {
        return l.toDateTime() < r.toDateTime();
    }
    return l.toString() < r.toString();
}
