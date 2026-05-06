#pragma once

#include <QHashFunctions>
#include <QList>
#include <QString>

enum class FileCategory {
    All = 0,
    Video,
    Image,
    Audio,
    Document,
    Archive,
    Executable,
    Code,
    Other,
};

inline size_t qHash(FileCategory cat, size_t seed = 0) noexcept
{
    return qHash(static_cast<int>(cat), seed);
}

namespace FileType {

// Classify a file by extension only (fast, no IO).
FileCategory classify(const QString& filePath);

// User-facing localized name (Russian).
QString displayName(FileCategory cat);

// Short single-letter / glyph icon hint (used in chips).
QString glyph(FileCategory cat);

// All categories in display order.
QList<FileCategory> all();

}  // namespace FileType
