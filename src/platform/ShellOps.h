#pragma once

#include <QString>

namespace ShellOps {

// Open Windows Explorer (or the system file manager) and select a file.
// On Windows: uses /select,<path>. On other platforms: opens parent folder.
void revealInFileManager(const QString& filePath);

// Copy text to the clipboard.
void copyToClipboard(const QString& text);

// Move a file/folder to the OS recycle bin / trash.
// Returns true on success; on failure, sets *errorMessage if non-null.
bool moveToTrash(const QString& path, QString* errorMessage = nullptr);

// Permanently delete a file or directory tree (no recycle bin).
// Returns true on success; on failure, sets *errorMessage if non-null.
bool permanentDelete(const QString& path, QString* errorMessage = nullptr);

// Restore a previously-recycled file/folder to its original location.
// `originalPath` is the absolute path the file used to live at (i.e. the
// path the user passed to moveToTrash).  Returns true on success; on
// failure sets *errorMessage if non-null.
//
// Implemented for Windows only.  On other platforms returns false with a
// "not supported on this platform" message — the in-app strikethrough
// state is still cleared by the caller so the row stays visible.
bool restoreFromTrash(const QString& originalPath,
                      QString* errorMessage = nullptr);

}  // namespace ShellOps
