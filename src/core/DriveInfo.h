#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

struct DriveInfo {
    QString rootPath;        // e.g. "C:/" or "/"
    QString label;           // volume label or "" on Linux
    QString fileSystem;      // "NTFS", "exFAT", ""
    qint64 totalBytes = 0;
    qint64 freeBytes = 0;
    bool isFixed = true;
    bool isReadable = true;

    qint64 usedBytes() const { return totalBytes - freeBytes; }
    double usedFraction() const {
        return totalBytes > 0 ? double(usedBytes()) / double(totalBytes) : 0.0;
    }
};

namespace Drives {

QVector<DriveInfo> enumerate();

}  // namespace Drives
