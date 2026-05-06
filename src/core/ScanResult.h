#pragma once

#include "FileType.h"

#include <QDateTime>
#include <QString>
#include <QVector>
#include <cstdint>

struct FileEntry {
    QString path;            // absolute path
    QString name;            // file name
    qint64 size = 0;
    QDateTime modified;
    QDateTime created;
    QDateTime accessed;
    bool isHidden = false;
    bool isReadOnly = false;
    bool isSystem = false;
    FileCategory category = FileCategory::Other;
};

// A folder summary node in the directory tree.
struct FolderNode {
    QString path;
    QString name;
    qint64 totalSize = 0;     // recursive size
    int totalFiles = 0;       // recursive file count
    int directFiles = 0;      // files directly inside this folder
    QDateTime modified;
    QDateTime created;
    QDateTime accessed;
    QVector<FolderNode> children;
};

struct ScanResult {
    QString rootPath;
    qint64 totalSize = 0;
    int totalFiles = 0;
    int totalDirs = 0;
    QVector<FileEntry> files;
    FolderNode root;
};
