#pragma once

#include "core/FileType.h"
#include "core/ScanResult.h"

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

class FileTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColName = 0,
        ColCategory,
        ColSize,
        ColModified,
        ColCreated,
        ColAccessed,
        ColPath,
        ColumnCount,
    };

    explicit FileTableModel(QObject* parent = nullptr);

    void setFiles(QVector<FileEntry> files);
    void clear();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    // Drag support: each existing row carries text/uri-list with the
    // file's absolute path so the user can drop into Explorer / other
    // apps.  No drop support — this is purely a read-only catalogue.
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    Qt::DropActions supportedDragActions() const override;

    const FileEntry& fileAt(int row) const { return m_files.at(row); }

    // Mark a row by absolute path as "deleted" (strikethrough red); a later
    // call to `purgeDeleted()` actually removes those rows from the model.
    bool markDeleted(const QString& path);
    // Inverse of markDeleted: drop the strikethrough/red colour from a row.
    bool clearDeleted(const QString& path);
    // True if the row at `path` is currently marked as deleted.
    bool isDeleted(const QString& path) const;

    void purgeDeleted();
    // Same as purgeDeleted() but skips rows whose underlying file still
    // exists on disk (e.g. was restored from the recycle bin) — those
    // simply lose the strikethrough.
    void purgeDeletedIfMissing();

private:
    QVector<FileEntry> m_files;
    QVector<bool> m_deleted;
};

class FileFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit FileFilterProxyModel(QObject* parent = nullptr);

    void setCategory(FileCategory cat);
    void setSearchText(const QString& text);
    FileCategory category() const { return m_category; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    FileCategory m_category = FileCategory::All;
    QString m_search;
};
