#include "DriveInfo.h"

#include <QStorageInfo>

namespace Drives {

QVector<DriveInfo> enumerate()
{
    QVector<DriveInfo> result;
    const auto volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo& v : volumes) {
        if (!v.isValid() || !v.isReady()) {
            continue;
        }
        // Skip pseudo filesystems on Linux (snap, /sys, /proc, etc.).
#ifndef Q_OS_WIN
        const QByteArray fs = v.fileSystemType();
        if (fs == "tmpfs" || fs == "devtmpfs" || fs == "proc" || fs == "sysfs" ||
            fs == "cgroup" || fs == "cgroup2" || fs == "overlay" ||
            fs == "squashfs" || fs == "fuse.gvfsd-fuse" || fs == "autofs") {
            continue;
        }
#endif
        DriveInfo d;
        d.rootPath   = v.rootPath();
        d.label      = v.name();
        d.fileSystem = QString::fromUtf8(v.fileSystemType());
        d.totalBytes = v.bytesTotal();
        d.freeBytes  = v.bytesAvailable();
        d.isReadable = v.isReady();
        result.push_back(d);
    }
    return result;
}

}  // namespace Drives
