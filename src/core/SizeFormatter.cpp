#include "SizeFormatter.h"

#include <QLocale>
#include <cmath>

namespace SizeFormatter {

QString humanReadable(qint64 bytes)
{
    if (bytes < 0) {
        return QStringLiteral("—");
    }
    constexpr double kKB = 1024.0;
    constexpr double kMB = kKB * 1024.0;
    constexpr double kGB = kMB * 1024.0;
    constexpr double kTB = kGB * 1024.0;

    const QLocale loc;
    const double b = static_cast<double>(bytes);
    if (b < kKB) {
        return QString::fromUtf8("%1 B").arg(loc.toString(bytes));
    }
    if (b < kMB) {
        return QString::fromUtf8("%1 KB").arg(loc.toString(b / kKB, 'f', 1));
    }
    if (b < kGB) {
        return QString::fromUtf8("%1 MB").arg(loc.toString(b / kMB, 'f', 1));
    }
    if (b < kTB) {
        return QString::fromUtf8("%1 GB").arg(loc.toString(b / kGB, 'f', 2));
    }
    return QString::fromUtf8("%1 TB").arg(loc.toString(b / kTB, 'f', 2));
}

QString grouped(qint64 bytes)
{
    QLocale loc;
    return loc.toString(bytes);
}

}  // namespace SizeFormatter
