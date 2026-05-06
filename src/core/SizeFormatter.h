#pragma once

#include <QString>
#include <cstdint>

namespace SizeFormatter {

// Returns "1.23 GB" / "987 MB" / "12.5 KB" / "345 B"
QString humanReadable(qint64 bytes);

// Returns "1234567890" with thousands separator like "1 234 567 890"
QString grouped(qint64 bytes);

}  // namespace SizeFormatter
