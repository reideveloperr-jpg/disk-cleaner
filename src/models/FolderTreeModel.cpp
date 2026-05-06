#include "FolderTreeModel.h"

#include "core/SizeFormatter.h"
#include "i18n/I18n.h"

#include <QColor>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QLocale>
#include <QMimeData>
#include <QMultiHash>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <algorithm>
#include <functional>

namespace {

QFileIconProvider& iconProvider()
{
    static QFileIconProvider p;
    return p;
}

QIcon folderIcon()
{
    static QIcon icon = iconProvider().icon(QFileIconProvider::Folder);
    return icon;
}

QIcon fileIconFor(const QString& path)
{
    // Cache by extension so we don't hit the filesystem for every row.
    static QHash<QString, QIcon> cache;
    const int dot = path.lastIndexOf('.');
    const QString ext = dot >= 0 ? path.mid(dot + 1).toLower() : QString();
    if (auto it = cache.constFind(ext); it != cache.constEnd()) {
        return it.value();
    }
    QIcon ico = iconProvider().icon(QFileInfo(path));
    if (ico.isNull()) {
        ico = iconProvider().icon(QFileIconProvider::File);
    }
    cache.insert(ext, ico);
    return ico;
}

}  // namespace

FolderTreeModel::FolderTreeModel(QObject* parent) : QAbstractItemModel(parent) {}

void FolderTreeModel::buildTree(const FolderNode& src, Node* parent)
{
    parent->folder = &src;

    // Sort sub-folder children descending by total size for the most useful view.
    std::vector<const FolderNode*> sorted;
    sorted.reserve(src.children.size());
    for (const auto& c : src.children) {
        sorted.push_back(&c);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const FolderNode* a, const FolderNode* b) {
                  return a->totalSize > b->totalSize;
              });

    parent->children.reserve(sorted.size());
    int row = 0;
    for (const FolderNode* child : sorted) {
        auto node = std::make_unique<Node>();
        node->parent = parent;
        node->rowInParent = row++;
        buildTree(*child, node.get());
        parent->children.push_back(std::move(node));
    }
}

// Cap the number of file leaves shown per folder.  Without a cap, a folder
// with hundreds of thousands of files would blow up the model and freeze the
// UI when expanded.  We show the largest N and rely on the Files view for
// full listings.
static constexpr int kMaxFilesPerFolder = 200;

void FolderTreeModel::attachFilesFromHash(Node* node)
{
    if (!node) return;

    // Depth-first so child folder nodes are filled in before we add file
    // leaves under the parent (keeps row ordering: folders first, then files).
    for (auto& child : node->children) {
        if (child->folder) {
            attachFilesFromHash(child.get());
        }
    }

    if (!node->folder) return;
    const QString folderPath = node->folder->path;
    if (folderPath.isEmpty()) return;

    auto it = m_filesByParent.constFind(folderPath);
    if (it == m_filesByParent.constEnd()) return;

    QVector<const FileEntry*> here = it.value();
    if (here.isEmpty()) return;

    std::sort(here.begin(), here.end(),
              [](const FileEntry* a, const FileEntry* b) {
                  return a->size > b->size;
              });

    const int total = here.size();
    const int show  = std::min(total, kMaxFilesPerFolder);
    int row = int(node->children.size());
    node->children.reserve(node->children.size() + show);
    for (int i = 0; i < show; ++i) {
        auto leaf = std::make_unique<Node>();
        leaf->parent = node;
        leaf->file = here[i];
        leaf->rowInParent = row++;
        node->children.push_back(std::move(leaf));
    }
    // The "and X more…" placeholder is intentionally omitted so the user
    // doesn't see an un-clickable filler row; the Files view shows all of
    // them anyway.  Capping keeps the tree responsive.
    Q_UNUSED(total);
}

void FolderTreeModel::setRoot(const FolderNode& root, const QVector<FileEntry>& files)
{
    beginResetModel();
    m_storage = root;
    m_files = files;
    m_root = std::make_unique<Node>();
    buildTree(m_storage, m_root.get());

    // Build a parent-path → file* hash once.  This turns the previous
    // O(folders × files) attachment loop into O(files) build + O(folders)
    // lookup.  For a million-file scan this is the difference between a
    // multi-second freeze and an imperceptible delay.
    m_filesByParent.clear();
    if (!m_files.isEmpty()) {
        m_filesByParent.reserve(int(m_files.size() / 4 + 16));
        // Plain string parent extraction — much cheaper than
        // QFileInfo(...).absolutePath() in a tight loop over millions of
        // entries (no stat() call, no QFileInfoPrivate alloc).  Drive-roots
        // are special-cased so "C:/foo.txt" maps to parent "C:/", not "C:".
        auto parentOf = [](const QString& path) -> QString {
            const int sep = std::max(path.lastIndexOf(QLatin1Char('/')),
                                     path.lastIndexOf(QLatin1Char('\\')));
            if (sep < 0) return {};
            if (sep == 2 && path.size() >= 3 && path[1] == QLatin1Char(':')) {
                return path.left(3);  // "C:/"
            }
            if (sep == 0) return QStringLiteral("/");
            return path.left(sep);
        };
        for (const FileEntry& f : m_files) {
            const QString parent = parentOf(f.path);
            if (parent.isEmpty()) continue;
            m_filesByParent[parent].push_back(&f);
        }
        attachFilesFromHash(m_root.get());
    }

    // Per-folder, per-category file counts (uses m_filesByParent).
    computeCategoryCounts(m_root.get());
    m_filesByParent.clear();

    // Mutable subtree aggregates — start equal to the scan numbers; will be
    // decremented as the user prunes rows from the tree.
    initAggregates(m_root.get());

    m_nodeByPath.clear();
    m_deletedPaths.clear();
    buildPathLookup(m_root.get());
    m_activeCategory = FileCategory::All;
    endResetModel();
    emit totalsChanged(totalSize(), totalFiles());
}

void FolderTreeModel::initAggregates(Node* node)
{
    if (!node) return;
    if (node->folder) {
        node->aggSize  = node->folder->totalSize;
        node->aggFiles = node->folder->totalFiles;
    } else if (node->file) {
        node->aggSize  = node->file->size;
        node->aggFiles = 1;
    }
    for (auto& c : node->children) initAggregates(c.get());
}

qint64 FolderTreeModel::totalSize() const
{
    return m_root ? m_root->aggSize : 0;
}

int FolderTreeModel::totalFiles() const
{
    return m_root ? m_root->aggFiles : 0;
}

void FolderTreeModel::computeCategoryCounts(Node* node)
{
    if (!node) return;
    int counts[9] = {};
    if (node->folder) {
        auto it = m_filesByParent.constFind(node->folder->path);
        if (it != m_filesByParent.constEnd()) {
            for (const FileEntry* f : it.value()) {
                const int idx = int(f->category);
                if (idx > 0 && idx < 9) counts[idx]++;
            }
        }
    }
    for (auto& c : node->children) {
        if (c->folder) {
            computeCategoryCounts(c.get());
            for (int i = 1; i < 9; ++i) counts[i] += c->categoryCounts[i];
        }
    }
    int total = 0;
    for (int i = 1; i < 9; ++i) total += counts[i];
    counts[0] = total;
    for (int i = 0; i < 9; ++i) node->categoryCounts[i] = counts[i];
}

void FolderTreeModel::buildPathLookup(Node* node)
{
    if (!node) return;
    if (node->folder) m_nodeByPath.insert(node->folder->path, node);
    if (node->file)   m_nodeByPath.insert(node->file->path, node);
    for (auto& c : node->children) buildPathLookup(c.get());
}

void FolderTreeModel::clear()
{
    beginResetModel();
    m_storage = FolderNode{};
    m_files.clear();
    m_root.reset();
    m_nodeByPath.clear();
    m_deletedPaths.clear();
    m_activeCategory = FileCategory::All;
    endResetModel();
}

QModelIndex FolderTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!m_root || row < 0 || column < 0 || column >= ColumnCount) {
        return {};
    }
    Node* p = parent.isValid() ? static_cast<Node*>(parent.internalPointer()) : m_root.get();
    if (!p || row >= int(p->children.size())) {
        return {};
    }
    return createIndex(row, column, p->children.at(row).get());
}

QModelIndex FolderTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) {
        return {};
    }
    Node* n = static_cast<Node*>(child.internalPointer());
    if (!n || !n->parent || n->parent == m_root.get()) {
        return {};
    }
    Node* p = n->parent;
    return createIndex(p->rowInParent, 0, p);
}

int FolderTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!m_root) {
        return 0;
    }
    Node* p = parent.isValid() ? static_cast<Node*>(parent.internalPointer()) : m_root.get();
    return p ? int(p->children.size()) : 0;
}

int FolderTreeModel::columnCount(const QModelIndex&) const
{
    return ColumnCount;
}

QVariant FolderTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    Node* n = static_cast<Node*>(index.internalPointer());
    if (!n) {
        return {};
    }

    // ---------- Deleted indicator (shared by file leaves and folders) ----
    if (role == Qt::FontRole) {
        if (n->deleted) {
            QFont f;
            f.setStrikeOut(true);
            return f;
        }
    }
    if (role == Qt::ForegroundRole) {
        if (n->deleted) return QColor(255, 95, 95);
    }

    // ---------- File leaf ----------
    if (n->file) {
        const FileEntry& f = *n->file;
        if (role == Qt::DecorationRole) {
            return index.column() == ColName ? fileIconFor(f.path) : QVariant{};
        }
        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case ColName: return f.name;
                case ColSize: return SizeFormatter::humanReadable(f.size);
                case ColPercent: {
                    if (n->parent && n->parent->aggSize > 0) {
                        const double p =
                            100.0 * double(f.size) /
                            double(n->parent->aggSize);
                        return QString::fromUtf8("%1 %")
                            .arg(QLocale().toString(p, 'f', 1));
                    }
                    return QString();
                }
                case ColFiles:    return QString();
                case ColModified: return f.modified.isValid()
                                        ? f.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                        : QString();
                case ColCreated:  return f.created.isValid()
                                        ? f.created.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                        : QString();
                case ColAccessed: return f.accessed.isValid()
                                        ? f.accessed.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                        : QString();
            }
        } else if (role == Qt::UserRole) {
            switch (index.column()) {
                case ColName: return f.name.toLower();
                case ColSize: return f.size;
                case ColPercent: {
                    if (n->parent && n->parent->aggSize > 0) {
                        return double(f.size) /
                               double(n->parent->aggSize);
                    }
                    return 0.0;
                }
                case ColFiles:    return 0;
                case ColModified: return f.modified;
                case ColCreated:  return f.created;
                case ColAccessed: return f.accessed;
            }
        } else if (role == Qt::ToolTipRole) {
            return f.path;
        } else if (role == Qt::TextAlignmentRole) {
            if (index.column() == ColSize || index.column() == ColPercent ||
                index.column() == ColFiles) {
                return int(Qt::AlignRight | Qt::AlignVCenter);
            }
        }
        return {};
    }

    // ---------- Folder ----------
    if (!n->folder) {
        return {};
    }
    const FolderNode& f = *n->folder;
    if (role == Qt::DecorationRole) {
        if (index.column() == ColName) return folderIcon();
        return {};
    }
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColName: return f.name.isEmpty() ? f.path : f.name;
            case ColSize: return SizeFormatter::humanReadable(n->aggSize);
            case ColPercent: {
                if (n->parent && n->parent->aggSize > 0) {
                    const double p =
                        100.0 * double(n->aggSize) /
                        double(n->parent->aggSize);
                    return QString::fromUtf8("%1 %")
                        .arg(QLocale().toString(p, 'f', 1));
                }
                return QString::fromUtf8("100.0 %");
            }
            case ColFiles: {
                // Show count for the active category only — when the user
                // filters by Video, the folder column shows how many videos
                // it contains rather than the unrelated total.
                const int idx = int(m_activeCategory);
                const int safeIdx = (idx >= 0 && idx < 9) ? idx : 0;
                const int count = n->categoryCounts[safeIdx];
                return QLocale().toString(qlonglong(count));
            }
            case ColModified: return f.modified.isValid()
                                    ? f.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                    : QString();
            case ColCreated:  return f.created.isValid()
                                    ? f.created.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                    : QString();
            case ColAccessed: return f.accessed.isValid()
                                    ? f.accessed.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                    : QString();
        }
    } else if (role == Qt::UserRole) {
        switch (index.column()) {
            case ColName:    return f.name.toLower();
            case ColSize:    return n->aggSize;
            case ColPercent: {
                if (n->parent && n->parent->aggSize > 0) {
                    return double(n->aggSize) /
                           double(n->parent->aggSize);
                }
                return 1.0;
            }
            case ColFiles: {
                const int idx = int(m_activeCategory);
                const int safeIdx = (idx >= 0 && idx < 9) ? idx : 0;
                return n->categoryCounts[safeIdx];
            }
            case ColModified: return f.modified;
            case ColCreated:  return f.created;
            case ColAccessed: return f.accessed;
        }
    } else if (role == Qt::ToolTipRole) {
        // Folder tooltip lists the per-category breakdown so the user can
        // see all numbers at a glance without flipping filters.
        QString breakdown = f.path;
        breakdown += QLatin1Char('\n');
        for (int i = 1; i < 9; ++i) {
            if (n->categoryCounts[i] > 0) {
                breakdown += QString::fromUtf8("\n%1: %2")
                                 .arg(FileType::displayName(static_cast<FileCategory>(i)),
                                      QLocale().toString(qlonglong(n->categoryCounts[i])));
            }
        }
        return breakdown;
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColSize || index.column() == ColPercent ||
            index.column() == ColFiles) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    return {};
}

QVariant FolderTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
        case ColName:     return tr_("Name");
        case ColSize:     return tr_("Size");
        case ColPercent:  return tr_("% of parent");
        case ColFiles:    return tr_("Files count");
        case ColModified: return tr_("Modified");
        case ColCreated:  return tr_("Created");
        case ColAccessed: return tr_("Accessed");
    }
    return {};
}

Qt::ItemFlags FolderTreeModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags base = QAbstractItemModel::flags(index);
    if (!index.isValid()) return base;
    Node* n = static_cast<Node*>(index.internalPointer());
    if (!n) return base;
    // Allow dragging only existing items; deleted/strikethrough rows are
    // not on disk anymore so dropping them into Explorer would be a lie.
    if (n->deleted) return base;
    const QString path = pathAt(index);
    if (path.isEmpty()) return base;
    return base | Qt::ItemIsDragEnabled;
}

QStringList FolderTreeModel::mimeTypes() const
{
    return {QStringLiteral("text/uri-list")};
}

QMimeData* FolderTreeModel::mimeData(const QModelIndexList& indexes) const
{
    QList<QUrl> urls;
    QSet<QString> seen;  // dedupe — a single row touches multiple columns
    for (const QModelIndex& idx : indexes) {
        if (!idx.isValid()) continue;
        const QString path = pathAt(idx);
        if (path.isEmpty() || seen.contains(path)) continue;
        if (!QFileInfo::exists(path)) continue;
        seen.insert(path);
        urls.append(QUrl::fromLocalFile(path));
    }
    if (urls.isEmpty()) return nullptr;
    auto* m = new QMimeData;
    m->setUrls(urls);
    return m;
}

Qt::DropActions FolderTreeModel::supportedDragActions() const
{
    // Copy is the safe default, Move is what Windows does when the user
    // holds Shift while dragging into Explorer, Link covers Alt-drag.
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

QString FolderTreeModel::pathAt(const QModelIndex& index) const
{
    if (!index.isValid()) return {};
    Node* n = static_cast<Node*>(index.internalPointer());
    if (!n) return {};
    if (n->file)   return n->file->path;
    if (n->folder) return n->folder->path;
    return {};
}

QString FolderTreeModel::nameAt(const QModelIndex& index) const
{
    if (!index.isValid()) return {};
    Node* n = static_cast<Node*>(index.internalPointer());
    if (!n) return {};
    if (n->file)   return n->file->name;
    if (n->folder) return n->folder->name.isEmpty() ? n->folder->path : n->folder->name;
    return {};
}

bool FolderTreeModel::isFile(const QModelIndex& index) const
{
    if (!index.isValid()) return false;
    Node* n = static_cast<Node*>(index.internalPointer());
    return n && n->file != nullptr;
}

bool FolderTreeModel::isFolder(const QModelIndex& index) const
{
    if (!index.isValid()) return false;
    Node* n = static_cast<Node*>(index.internalPointer());
    return n && n->folder != nullptr && n->file == nullptr;
}

FileCategory FolderTreeModel::categoryAt(const QModelIndex& index) const
{
    if (!index.isValid()) return FileCategory::All;
    Node* n = static_cast<Node*>(index.internalPointer());
    if (!n || !n->file) return FileCategory::All;
    return n->file->category;
}

void FolderTreeModel::setActiveCategory(FileCategory cat)
{
    if (m_activeCategory == cat) return;
    m_activeCategory = cat;
    if (!m_root) return;
    // ColFiles depends on the active category. Walk the whole tree and
    // emit dataChanged for that column on every folder so views repaint
    // the new counts. Previously we mixed dataChanged with a bare
    // layoutChanged() emit, which corrupted persistent indexes inside
    // QSortFilterProxyModel and crashed the next time the user picked a
    // different filter chip.
    emitFilesColumnChanged(m_root.get(), QModelIndex());
}

void FolderTreeModel::emitFilesColumnChanged(Node* parent,
                                             const QModelIndex& parentIdx)
{
    if (!parent) return;
    const int rows = int(parent->children.size());
    if (rows <= 0) return;
    emit dataChanged(index(0, ColFiles, parentIdx),
                     index(rows - 1, ColFiles, parentIdx),
                     { Qt::DisplayRole, Qt::UserRole, Qt::ToolTipRole });
    for (int i = 0; i < rows; ++i) {
        Node* c = parent->children[i].get();
        if (c && c->folder) {
            emitFilesColumnChanged(c, index(i, 0, parentIdx));
        }
    }
}

QModelIndex FolderTreeModel::indexForNode(Node* n) const
{
    if (!n || !n->parent) return {};
    return createIndex(n->rowInParent, 0, n);
}

void FolderTreeModel::markSubtreeDeleted(Node* n)
{
    if (!n) return;
    n->deleted = true;
    if (n->folder) m_deletedPaths.insert(n->folder->path, n);
    if (n->file)   m_deletedPaths.insert(n->file->path, n);
    for (auto& c : n->children) markSubtreeDeleted(c.get());
}

void FolderTreeModel::clearSubtreeDeleted(Node* n)
{
    if (!n) return;
    n->deleted = false;
    if (n->folder) m_deletedPaths.remove(n->folder->path);
    if (n->file)   m_deletedPaths.remove(n->file->path);
    for (auto& c : n->children) clearSubtreeDeleted(c.get());
}

void FolderTreeModel::collectSubtreePaths(Node* n, QVector<QString>* out) const
{
    if (!n || !out) return;
    if (n->folder) out->push_back(n->folder->path);
    if (n->file)   out->push_back(n->file->path);
    for (auto& c : n->children) collectSubtreePaths(c.get(), out);
}

void FolderTreeModel::emitChangedRecursive(Node* n)
{
    if (!n || !n->parent) return;
    const QModelIndex left  = createIndex(n->rowInParent, 0, n);
    const QModelIndex right = createIndex(n->rowInParent, ColumnCount - 1, n);
    emit dataChanged(left, right,
                     { Qt::FontRole, Qt::ForegroundRole, Qt::DisplayRole });
    for (auto& c : n->children) emitChangedRecursive(c.get());
}

bool FolderTreeModel::markDeleted(const QString& path)
{
    Node* n = m_nodeByPath.value(path, nullptr);
    if (!n) return false;
    markSubtreeDeleted(n);
    emitChangedRecursive(n);
    return true;
}

bool FolderTreeModel::clearDeleted(const QString& path)
{
    Node* n = m_nodeByPath.value(path, nullptr);
    if (!n) return false;
    clearSubtreeDeleted(n);
    emitChangedRecursive(n);
    return true;
}

bool FolderTreeModel::isDeleted(const QString& path) const
{
    Node* n = m_nodeByPath.value(path, nullptr);
    return n && n->deleted;
}

void FolderTreeModel::renumberChildren(Node* parent)
{
    if (!parent) return;
    for (size_t i = 0; i < parent->children.size(); ++i) {
        parent->children[i]->rowInParent = int(i);
    }
}

void FolderTreeModel::propagateRemoval(Node* gone, Node* parentChain)
{
    if (!gone || !parentChain) return;
    // Build the per-category delta to subtract.  Folder nodes already have
    // their categoryCounts populated by computeCategoryCounts(); file leaves
    // do NOT — they are attached after the count pass and their own
    // categoryCounts array stays at zero.  Without this branch the "Files
    // count" column (which reads ancestor categoryCounts[active]) stayed
    // stale after a leaf was removed.
    int delta[9] = {};
    if (gone->folder && !gone->file) {
        for (int i = 0; i < 9; ++i) delta[i] = gone->categoryCounts[i];
    } else if (gone->file) {
        const int catIdx = int(gone->file->category);
        if (catIdx > 0 && catIdx < 9) delta[catIdx] = 1;
        delta[0] = 1;  // index 0 = total across all categories
    }
    // Walk every ancestor and subtract the gone node's aggregates so size,
    // percent and per-category counts immediately reflect reality. Each
    // ancestor is then repainted across the columns that depend on those
    // numbers.
    for (Node* a = parentChain; a != nullptr; a = a->parent) {
        a->aggSize  = qMax<qint64>(0, a->aggSize  - gone->aggSize);
        a->aggFiles = qMax<int>(0,    a->aggFiles - gone->aggFiles);
        for (int i = 0; i < 9; ++i) {
            a->categoryCounts[i] =
                qMax(0, a->categoryCounts[i] - delta[i]);
        }
        // Don't try to emit dataChanged for the synthetic root — it has no
        // QModelIndex of its own (children of an invalid index live there).
        if (a->parent) {
            const QModelIndex left  = createIndex(a->rowInParent, ColSize, a);
            const QModelIndex right = createIndex(a->rowInParent, ColFiles, a);
            emit dataChanged(left, right,
                             { Qt::DisplayRole, Qt::UserRole, Qt::ToolTipRole });
        }
    }
}

void FolderTreeModel::purgeDeletedFrom(Node* parent, const QModelIndex& parentIdx,
                                       QVector<QString>* removedPaths)
{
    if (!parent) return;
    // First recurse into surviving children so deeply-nested deletions are
    // collected before we touch this level.
    for (auto& c : parent->children) {
        if (c->deleted) continue;  // its descendants are removed wholesale
        if (c->folder) {
            const QModelIndex cIdx = createIndex(c->rowInParent, 0, c.get());
            purgeDeletedFrom(c.get(), cIdx, removedPaths);
        }
    }
    // Then remove deleted children from this level, highest row first so
    // earlier indices stay stable.
    int n = int(parent->children.size());
    for (int i = n - 1; i >= 0; --i) {
        if (parent->children[i]->deleted) {
            Node* gone = parent->children[i].get();
            // Update parent-chain aggregates BEFORE the unique_ptr is
            // destroyed (we need gone->aggSize / categoryCounts).
            propagateRemoval(gone, parent);
            // Snapshot the subtree's paths before we erase the node so we
            // can drop them from m_nodeByPath surgically (rebuilding the
            // lookup is what made Refresh hang for several seconds on
            // large scans).
            if (removedPaths) {
                collectSubtreePaths(gone, removedPaths);
            }
            beginRemoveRows(parentIdx, i, i);
            parent->children.erase(parent->children.begin() + i);
            endRemoveRows();
        }
    }
    renumberChildren(parent);
}

void FolderTreeModel::purgeDeleted()
{
    if (!m_root || m_deletedPaths.isEmpty()) return;
    QVector<QString> removed;
    removed.reserve(m_deletedPaths.size());
    purgeDeletedFrom(m_root.get(), QModelIndex(), &removed);
    for (const QString& p : removed) {
        m_nodeByPath.remove(p);
        m_deletedPaths.remove(p);
    }
    emit totalsChanged(totalSize(), totalFiles());
}

int FolderTreeModel::markMissingFromDisk()
{
    if (!m_root) return 0;
    int newlyMarked = 0;
    // Local recursion over file leaves only — folders live as long as one
    // of their descendants is still on disk, and the recursive purge below
    // will collapse fully-empty folders away once all leaves are gone.
    std::function<void(Node*)> visit = [&](Node* n) {
        if (!n) return;
        if (n->file && !n->deleted && !QFileInfo::exists(n->file->path)) {
            n->deleted = true;
            m_deletedPaths.insert(n->file->path, n);
            ++newlyMarked;
        }
        for (auto& c : n->children) visit(c.get());
    };
    visit(m_root.get());
    return newlyMarked;
}

void FolderTreeModel::purgeDeletedIfMissing()
{
    if (!m_root) return;
    // Pick up files that disappeared outside the app (cloud sync, manual
    // move/rename in Explorer, etc.) so Refresh reconciles the tree with
    // disk reality, not just user-marked deletions.
    markMissingFromDisk();

    if (m_deletedPaths.isEmpty()) {
        emit totalsChanged(totalSize(), totalFiles());
        return;
    }
    // We only stat the small set of paths the user marked as deleted —
    // not the entire scan tree.  If a path's underlying file is back on
    // disk (e.g. user restored it from the recycle bin) we drop the
    // strikethrough; otherwise we'll remove the row from the model.
    QVector<Node*> revived;
    revived.reserve(m_deletedPaths.size());
    for (auto it = m_deletedPaths.constBegin(); it != m_deletedPaths.constEnd();
         ++it) {
        if (QFileInfo::exists(it.key())) {
            revived.push_back(it.value());
        }
    }
    for (Node* n : revived) {
        clearSubtreeDeleted(n);
        emitChangedRecursive(n);
    }
    if (m_deletedPaths.isEmpty()) {
        emit totalsChanged(totalSize(), totalFiles());
        return;
    }
    QVector<QString> removed;
    removed.reserve(m_deletedPaths.size());
    purgeDeletedFrom(m_root.get(), QModelIndex(), &removed);
    for (const QString& p : removed) {
        m_nodeByPath.remove(p);
        m_deletedPaths.remove(p);
    }
    emit totalsChanged(totalSize(), totalFiles());
}
