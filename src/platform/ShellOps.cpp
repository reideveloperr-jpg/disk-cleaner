// Make sure the modern shell API (IFileOperation et al., Vista+) is fully
// declared in mingw-w64 headers.  These have to be set BEFORE any header
// that may pull in <windows.h> (Qt's own headers do, transitively), so we
// keep them at the very top of the file.
#ifdef _WIN32
#  ifdef _WIN32_WINNT
#    undef _WIN32_WINNT
#  endif
#  ifdef NTDDI_VERSION
#    undef NTDDI_VERSION
#  endif
#  ifdef WINVER
#    undef WINVER
#  endif
#  define _WIN32_WINNT 0x0A00
#  define NTDDI_VERSION 0x0A000000
#  define WINVER 0x0A00
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#endif

#include "ShellOps.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <oleauto.h>
#include <objbase.h>
#include <shlguid.h>
#endif

namespace ShellOps {

void revealInFileManager(const QString& filePath)
{
    const QFileInfo fi(filePath);
    if (!fi.exists()) {
        return;
    }
#if defined(Q_OS_WIN)
    // explorer.exe is fussy about how /select,<path> is quoted: when Qt
    // splits the call into argv it tends to double-quote each item, which
    // makes Explorer fall back to opening the user's default folder
    // (Documents/Home). Pass the whole tail as a native argument string so
    // it reaches Explorer in the exact form it expects.
    const QString native = QDir::toNativeSeparators(fi.absoluteFilePath());
    QProcess proc;
    proc.setProgram(QStringLiteral("explorer.exe"));
    proc.setNativeArguments(QStringLiteral("/select,\"") + native +
                            QStringLiteral("\""));
    if (!proc.startDetached()) {
        // Fallback: at least open the parent folder.
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
    }
#elif defined(Q_OS_MAC)
    QProcess::startDetached(QStringLiteral("open"),
                            { QStringLiteral("-R"), fi.absoluteFilePath() });
#else
    // Fall back to opening the containing directory.
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
#endif
}

void copyToClipboard(const QString& text)
{
    if (QClipboard* cb = QApplication::clipboard()) {
        cb->setText(text);
    }
}

#if defined(Q_OS_WIN)
namespace {

bool runShFileOp(const QString& path, FILEOP_FLAGS flags, QString* err)
{
    const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    // SHFileOperation needs a *double-null-terminated* string list.
    QVector<wchar_t> buf(native.size() + 2, 0);
    native.toWCharArray(buf.data());

    SHFILEOPSTRUCTW op{};
    op.wFunc  = FO_DELETE;
    op.pFrom  = buf.data();
    op.fFlags = flags;
    const int rc = SHFileOperationW(&op);
    if (rc != 0) {
        if (err) *err = QStringLiteral("SHFileOperation failed (code %1)").arg(rc);
        return false;
    }
    if (op.fAnyOperationsAborted) {
        if (err) *err = QStringLiteral("Operation aborted");
        return false;
    }
    return true;
}

}  // namespace
#endif

bool moveToTrash(const QString& path, QString* err)
{
    if (path.isEmpty()) {
        if (err) *err = QStringLiteral("Empty path");
        return false;
    }
#if defined(Q_OS_WIN)
    return runShFileOp(path,
                       FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI |
                           FOF_SILENT | FOF_NOCONFIRMMKDIR,
                       err);
#else
    // Qt's portable trashing — works on Linux/macOS.
    QFile f(path);
    if (f.moveToTrash()) {
        return true;
    }
    if (err) *err = f.errorString();
    return false;
#endif
}

bool permanentDelete(const QString& path, QString* err)
{
    if (path.isEmpty()) {
        if (err) *err = QStringLiteral("Empty path");
        return false;
    }
#if defined(Q_OS_WIN)
    return runShFileOp(path,
                       FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT |
                           FOF_NOCONFIRMMKDIR,
                       err);
#else
    QFileInfo fi(path);
    if (fi.isDir()) {
        QDir d(path);
        if (d.removeRecursively()) return true;
        if (err) *err = QStringLiteral("Could not remove directory");
        return false;
    }
    QFile f(path);
    if (f.remove()) return true;
    if (err) *err = f.errorString();
    return false;
#endif
}

#if defined(Q_OS_WIN)
namespace {

// Fetch a string property from the recycle-bin shell folder for a given
// PIDL.  Returns an empty string when the property is missing or the call
// fails — both of which we treat as "skip this entry".
QString recycleBinPropString(IShellFolder2* bin,
                             LPCITEMIDLIST pidl,
                             const SHCOLUMNID& colId)
{
    VARIANT v;
    VariantInit(&v);
    if (FAILED(bin->GetDetailsEx(pidl, &colId, &v))) {
        VariantClear(&v);
        return {};
    }
    QString out;
    if (v.vt == VT_BSTR && v.bstrVal) {
        out = QString::fromWCharArray(v.bstrVal);
    } else if (v.vt != VT_EMPTY && v.vt != VT_NULL) {
        // Coerce other VARIANT types (e.g. VT_LPWSTR) into BSTR.
        VARIANT tmp;
        VariantInit(&tmp);
        if (SUCCEEDED(VariantChangeType(&tmp, &v, 0, VT_BSTR)) &&
            tmp.vt == VT_BSTR && tmp.bstrVal) {
            out = QString::fromWCharArray(tmp.bstrVal);
        }
        VariantClear(&tmp);
    }
    VariantClear(&v);
    return out;
}

// Get the in-folder display name for a recycle-bin entry.  This returns
// the *original* file name (e.g. "report.txt"), not the storage name
// ($RXY3K7.txt) that Windows uses internally.
//
// We pass SHGDN_INFOLDER | SHGDN_FORPARSING so the result includes the
// extension regardless of the user's "Hide extensions for known file
// types" Explorer setting — without SHGDN_FORPARSING the call returns the
// human-friendly name, which on default Windows installs strips ".mp4",
// ".jpg" etc., and our path comparison then fails with "Item not found".
QString recycleBinDisplayName(IShellFolder2* bin, LPCITEMIDLIST pidl)
{
    auto fetch = [&](DWORD flags) -> QString {
        STRRET sr;
        if (FAILED(bin->GetDisplayNameOf(pidl, flags, &sr))) {
            return {};
        }
        WCHAR buf[MAX_PATH] = { 0 };
        if (FAILED(StrRetToBufW(&sr, pidl, buf, ARRAYSIZE(buf)))) {
            return {};
        }
        QString s = QString::fromWCharArray(buf);
        // SHGDN_FORPARSING on a recycle-bin entry sometimes returns the
        // *full* path of the item ("D:\\Foo\\bar.mp4"); in that case keep
        // just the trailing file name.
        const int slash = std::max(s.lastIndexOf(QLatin1Char('\\')),
                                   s.lastIndexOf(QLatin1Char('/')));
        if (slash >= 0) {
            s = s.mid(slash + 1);
        }
        return s;
    };
    QString name = fetch(SHGDN_INFOLDER | SHGDN_FORPARSING);
    if (name.isEmpty()) {
        // Fallback to the friendly in-folder name in case FORPARSING is
        // unsupported on this entry.
        name = fetch(SHGDN_INFOLDER);
    }
    return name;
}

// Normalise a Windows path so two strings that point at the same on-disk
// item compare equal: forward slashes → backslashes, drive letter upper-
// cased, redundant "." / ".." resolved, trailing slash stripped, then
// case-folded.  We compare paths inside the recycle-bin enumerator with
// this representation.
QString normalizeWinPath(const QString& p)
{
    QString s = QDir::cleanPath(p);
    s = QDir::toNativeSeparators(s);
    while (s.endsWith(QLatin1Char('\\')) && s.size() > 3) {
        s.chop(1);
    }
    if (s.size() >= 2 && s.at(1) == QLatin1Char(':')) {
        s[0] = s.at(0).toUpper();
    }
    return s.toLower();
}

}  // namespace
#endif

bool restoreFromTrash(const QString& originalPath, QString* err)
{
    if (originalPath.isEmpty()) {
        if (err) *err = QStringLiteral("Empty path");
        return false;
    }
#if defined(Q_OS_WIN)
    // We compare paths case-insensitively, with normalised native
    // separators, so "D:/Foo/Bar.txt" / "d:\\foo\\bar.txt" / a path with
    // trailing backslash all match the same recycle-bin entry.
    const QString want = normalizeWinPath(originalPath);
    const QFileInfo wantInfo(originalPath);
    const QString wantBaseName = wantInfo.fileName().toLower();

    HRESULT hrInit = CoInitializeEx(nullptr,
                                    COINIT_APARTMENTTHREADED |
                                        COINIT_DISABLE_OLE1DDE);
    const bool needUninit = SUCCEEDED(hrInit);

    auto fail = [&](const QString& msg) {
        if (err) *err = msg;
        if (needUninit) CoUninitialize();
        return false;
    };

    LPITEMIDLIST recyclePidl = nullptr;
    if (FAILED(SHGetSpecialFolderLocation(nullptr, CSIDL_BITBUCKET,
                                          &recyclePidl))) {
        return fail(QStringLiteral("Could not locate the Recycle Bin"));
    }

    IShellFolder* desktop = nullptr;
    if (FAILED(SHGetDesktopFolder(&desktop)) || !desktop) {
        CoTaskMemFree(recyclePidl);
        return fail(QStringLiteral("SHGetDesktopFolder failed"));
    }

    IShellFolder2* bin = nullptr;
    HRESULT hr = desktop->BindToObject(recyclePidl, nullptr,
                                       IID_PPV_ARGS(&bin));
    desktop->Release();
    if (FAILED(hr) || !bin) {
        CoTaskMemFree(recyclePidl);
        return fail(QStringLiteral("Could not open the Recycle Bin"));
    }

    IEnumIDList* iter = nullptr;
    if (FAILED(bin->EnumObjects(nullptr,
                                SHCONTF_FOLDERS | SHCONTF_NONFOLDERS |
                                    SHCONTF_INCLUDEHIDDEN,
                                &iter)) || !iter) {
        bin->Release();
        CoTaskMemFree(recyclePidl);
        return fail(QStringLiteral("Could not enumerate the Recycle Bin"));
    }

    // Standard Recycle Bin shell columns.  See the "Displaced" property
    // set in shlguid.h:
    //   PID_DISPLACED_FROM = 2  → original folder
    //   PID_DISPLACED_DATE = 3  → delete time
    const SHCOLUMNID colOrigFolder = { PSGUID_DISPLACED, PID_DISPLACED_FROM };

    // First sweep: pick out every recycle-bin entry whose recorded
    // origFolder + origName matches us exactly, or whose origName matches
    // our basename.  We keep the PIDL for the exact match (preferred) or
    // — only if there is exactly one — for the basename match (fallback
    // for cases where the recorded origFolder is a 8.3 short path or
    // otherwise differs from what we asked).
    LPITEMIDLIST exactPidl = nullptr;       // owned by us; CoTaskMemFree'd below
    LPITEMIDLIST fuzzyPidl = nullptr;
    int fuzzyMatches = 0;
    int seenEntries = 0;
    QString fuzzyOrigFolder, fuzzyOrigName;
    QString exactOrigFolder, exactOrigName;
    {
        LPITEMIDLIST itemPidl = nullptr;
        while (iter->Next(1, &itemPidl, nullptr) == S_OK) {
            ++seenEntries;
            const QString origFolder = recycleBinPropString(bin, itemPidl,
                                                            colOrigFolder);
            const QString origName   = recycleBinDisplayName(bin, itemPidl);
            if (origFolder.isEmpty() || origName.isEmpty()) {
                CoTaskMemFree(itemPidl);
                itemPidl = nullptr;
                continue;
            }
            const QString full = normalizeWinPath(
                QDir(origFolder).filePath(origName));
            const bool nameMatches =
                origName.compare(wantInfo.fileName(), Qt::CaseInsensitive) == 0;
            if (full == want && !exactPidl) {
                exactPidl = ILCloneFull(itemPidl);
                exactOrigFolder = origFolder;
                exactOrigName   = origName;
            } else if (nameMatches) {
                ++fuzzyMatches;
                if (!fuzzyPidl) {
                    fuzzyPidl = ILCloneFull(itemPidl);
                    fuzzyOrigFolder = origFolder;
                    fuzzyOrigName   = origName;
                }
            }
            CoTaskMemFree(itemPidl);
            itemPidl = nullptr;
        }
    }

    LPITEMIDLIST chosenPidl = nullptr;
    QString chosenFolder, chosenName;
    if (exactPidl) {
        chosenPidl   = exactPidl;
        chosenFolder = exactOrigFolder;
        chosenName   = exactOrigName;
    } else if (fuzzyPidl && fuzzyMatches == 1) {
        // Unambiguous basename fallback — safer than guessing among
        // duplicates.
        chosenPidl   = fuzzyPidl;
        chosenFolder = fuzzyOrigFolder;
        chosenName   = fuzzyOrigName;
    }

    bool restored = false;
    QString lastErr;
    if (chosenPidl) {
        // Build IShellItem objects and run an IFileOperation move.
        IShellItem* itemSi = nullptr;
        if (SUCCEEDED(SHCreateItemWithParent(recyclePidl, nullptr,
                                             chosenPidl,
                                             IID_PPV_ARGS(&itemSi)))) {
            IShellItem* destSi = nullptr;
            const std::wstring destW =
                QDir::toNativeSeparators(chosenFolder).toStdWString();
            if (SUCCEEDED(SHCreateItemFromParsingName(
                    destW.c_str(), nullptr,
                    IID_PPV_ARGS(&destSi)))) {
                IFileOperation* op = nullptr;
                if (SUCCEEDED(CoCreateInstance(
                        CLSID_FileOperation, nullptr,
                        CLSCTX_ALL, IID_PPV_ARGS(&op)))) {
                    op->SetOperationFlags(FOF_NO_UI |
                                          FOF_NOCONFIRMATION |
                                          FOF_SILENT |
                                          FOFX_NOMINIMIZEBOX);
                    const std::wstring nameW = chosenName.toStdWString();
                    HRESULT mv = op->MoveItem(itemSi, destSi,
                                              nameW.c_str(), nullptr);
                    if (SUCCEEDED(mv)) {
                        mv = op->PerformOperations();
                    }
                    if (SUCCEEDED(mv)) {
                        BOOL aborted = FALSE;
                        op->GetAnyOperationsAborted(&aborted);
                        restored = !aborted;
                        if (aborted) {
                            lastErr = QStringLiteral("Restore was aborted");
                        }
                    } else {
                        lastErr = QStringLiteral(
                                      "IFileOperation::Move failed (0x%1)")
                                      .arg(quint32(mv), 0, 16);
                    }
                    op->Release();
                } else {
                    lastErr = QStringLiteral(
                        "Could not create IFileOperation");
                }
                destSi->Release();
            } else {
                lastErr = QStringLiteral(
                    "Could not open original folder \"%1\"").arg(chosenFolder);
            }
            itemSi->Release();
        } else {
            lastErr = QStringLiteral("SHCreateItemWithParent failed");
        }
    }

    if (exactPidl) CoTaskMemFree(exactPidl);
    if (fuzzyPidl) CoTaskMemFree(fuzzyPidl);
    iter->Release();
    bin->Release();
    CoTaskMemFree(recyclePidl);
    if (needUninit) CoUninitialize();

    if (!restored && err) {
        if (!lastErr.isEmpty()) {
            *err = lastErr;
        } else if (seenEntries == 0) {
            *err = QStringLiteral("The Recycle Bin is empty");
        } else if (fuzzyMatches > 1) {
            *err = QStringLiteral(
                "Found %1 items in the Recycle Bin with the same name; "
                "please restore manually from Explorer.").arg(fuzzyMatches);
        } else {
            *err = QStringLiteral(
                "Item not found in the Recycle Bin (checked %1 entries). "
                "It may have been emptied or restored already.")
                       .arg(seenEntries);
        }
    }
    return restored;
#else
    Q_UNUSED(originalPath);
    if (err) {
        *err = QStringLiteral("Restore from trash is only supported on Windows");
    }
    return false;
#endif
}

}  // namespace ShellOps
