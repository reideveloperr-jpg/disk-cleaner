#pragma once

#include "core/FileType.h"
#include "core/ScanResult.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QStringList>
#include <memory>
#include <vector>

class QMimeData;

class FolderTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column {
        ColName = 0,
        ColSize,
        ColPercent,
        ColFiles,
        ColModified,
        ColCreated,
        ColAccessed,
        ColumnCount,
    };

    explicit FolderTreeModel(QObject* parent = nullptr);

    // Loads the folder tree. If `files` is provided, each folder additionally
    // exposes its directly-contained files as leaf rows.
    void setRoot(const FolderNode& root, const QVector<FileEntry>& files = {});
    void clear();

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    // Drag support: every existing folder/file row carries text/uri-list
    // pointing at its absolute path so the user can drop the items into
    // Explorer / another application.
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    Qt::DropActions supportedDragActions() const override;

    QString pathAt(const QModelIndex& index) const;
    QString nameAt(const QModelIndex& index) const;
    bool isFile(const QModelIndex& index) const;
    bool isFolder(const QModelIndex& index) const;
    FileCategory categoryAt(const QModelIndex& index) const;

    // Active category: drives the displayed file count (for folders) and the
    // file-leaf visibility through the proxy.  Recomputing is cheap because
    // per-folder counts are pre-aggregated.
    void setActiveCategory(FileCategory cat);
    FileCategory activeCategory() const { return m_activeCategory; }

    // Mark the row at `path` (and all descendants if it is a folder) as
    // "deleted" — they are still visible but rendered with strikethrough red
    // text.  Returns true if a matching row was found.
    bool markDeleted(const QString& path);

    // Clear the "deleted" flag on the row at `path` (and all descendants if
    // it is a folder) and repaint that subtree without strikethrough. Used
    // when a file gets restored from the recycle bin (or simply re-appears
    // on disk during Refresh).
    bool clearDeleted(const QString& path);

    // True if the row at `path` is currently marked as deleted.
    bool isDeleted(const QString& path) const;

    // Permanently remove every row currently in the deleted state from the
    // tree.  Called from the Analyzer's Refresh button.
    void purgeDeleted();

    // Same as purgeDeleted(), but only removes rows whose underlying file
    // or folder no longer exists on disk.  Rows that were marked deleted
    // but have since reappeared (e.g. restored from the recycle bin) lose
    // the strikethrough instead of being removed.
    void purgeDeletedIfMissing();

    // Walk the tree and stat() every file leaf; any whose file is no longer
    // on disk is marked as deleted.  Called from Refresh so that files moved
    // or removed outside the app are reconciled too (not just files the user
    // deleted from inside Volchay Cleans).  Returns the number of newly
    // marked rows.
    int markMissingFromDisk();

    // Aggregates after the tree has been pruned/reconciled. They mirror
    // ScanResult::totalSize / totalFiles for the root and let views refresh
    // their summary labels without holding stale numbers.
    qint64 totalSize() const;
    int    totalFiles() const;

signals:
    // Emitted when purge / reconcile changes the aggregates of the root or
    // one of its descendants. Views listen to this to repaint their summary
    // text instead of caching the stale values from the original scan.
    void totalsChanged(qint64 totalSize, int totalFiles);

private:
    struct Node {
        const FolderNode* folder = nullptr;
        const FileEntry* file = nullptr;
        Node* parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
        int rowInParent = 0;
        bool deleted = false;
        // Mutable subtree aggregates. They start out matching
        // FolderNode::totalSize / totalFiles (or the file's own size for a
        // leaf) but get decremented as rows are pruned, so they always
        // reflect what is actually still in the tree.
        qint64 aggSize = 0;
        int    aggFiles = 0;
        // Recursive file count grouped by category (index 0 = total of all
        // categories, indices 1..8 follow FileCategory ordering).
        int categoryCounts[9] = {};
    };

    void buildTree(const FolderNode& src, Node* parent);
    void attachFilesFromHash(Node* node);
    void computeCategoryCounts(Node* node);
    void buildPathLookup(Node* node);
    void markSubtreeDeleted(Node* n);
    void clearSubtreeDeleted(Node* n);
    void emitChangedRecursive(Node* n);
    void emitFilesColumnChanged(Node* parent, const QModelIndex& parentIdx);
    void purgeDeletedFrom(Node* parent, const QModelIndex& parentIdx,
                          QVector<QString>* removedPaths = nullptr);
    // Subtract the aggregates of `gone` from every ancestor and emit
    // dataChanged so size/percent/files columns repaint with the new totals.
    void propagateRemoval(Node* gone, Node* parentChain);
    // Reset Node aggregates to match folder->totalSize / file size for the
    // whole subtree.  Used by setRoot() because aggregates start equal to
    // the original scan numbers.
    void initAggregates(Node* node);
    void renumberChildren(Node* parent);
    QModelIndex indexForNode(Node* n) const;
    void collectSubtreePaths(Node* n, QVector<QString>* out) const;

    FolderNode m_storage;
    QVector<FileEntry> m_files;
    std::unique_ptr<Node> m_root;
    QHash<QString, QVector<const FileEntry*>> m_filesByParent;
    QHash<QString, Node*> m_nodeByPath;
    // Set of paths the user has marked as deleted. Subset of m_nodeByPath;
    // kept in sync with the `deleted` flag on each Node so Refresh can
    // iterate just the marked rows instead of the full tree (which can
    // hold > 100 k entries on a full-drive scan and was the source of
    // the 3-second UI hang on Refresh).
    QHash<QString, Node*> m_deletedPaths;
    FileCategory m_activeCategory = FileCategory::All;
};
