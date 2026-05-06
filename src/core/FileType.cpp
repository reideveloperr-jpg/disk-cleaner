#include "FileType.h"

#include "i18n/I18n.h"

#include <QFileInfo>
#include <QSet>

namespace FileType {

namespace {

const QSet<QString>& videoExt()
{
    static const QSet<QString> s = {
        "mp4", "m4v", "mov", "mkv", "avi", "wmv", "flv", "webm", "mpg", "mpeg",
        "3gp", "3g2", "ts", "mts", "m2ts", "vob", "rm", "rmvb", "ogv", "f4v",
    };
    return s;
}

const QSet<QString>& imageExt()
{
    static const QSet<QString> s = {
        "jpg", "jpeg", "png", "gif", "bmp", "tiff", "tif", "webp", "heic", "heif",
        "svg", "ico", "raw", "cr2", "nef", "arw", "dng", "psd", "ai", "eps",
        "jfif", "avif",
    };
    return s;
}

const QSet<QString>& audioExt()
{
    static const QSet<QString> s = {
        "mp3", "wav", "flac", "aac", "ogg", "wma", "m4a", "opus", "aiff", "ape",
        "alac", "amr", "mid", "midi", "dsf", "dff",
    };
    return s;
}

const QSet<QString>& documentExt()
{
    static const QSet<QString> s = {
        "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "odt", "ods", "odp",
        "rtf", "txt", "md", "csv", "tsv", "djvu", "epub", "mobi", "fb2", "tex",
        "pages", "numbers", "key", "xps", "log",
    };
    return s;
}

const QSet<QString>& archiveExt()
{
    static const QSet<QString> s = {
        "zip", "rar", "7z", "tar", "gz", "bz2", "xz", "tgz", "tbz2", "lz",
        "lzma", "zst", "iso", "img", "dmg", "cab", "arj", "lzh", "ace",
    };
    return s;
}

const QSet<QString>& executableExt()
{
    static const QSet<QString> s = {
        "exe", "msi", "msu", "msp", "bat", "cmd", "ps1", "vbs", "com", "scr",
        "dll", "sys", "appx", "appxbundle", "msix",
    };
    return s;
}

const QSet<QString>& codeExt()
{
    static const QSet<QString> s = {
        "cpp", "cxx", "cc", "c", "h", "hpp", "hxx", "ino",
        "py", "pyw", "pyx",
        "js", "mjs", "cjs", "ts", "tsx", "jsx",
        "java", "kt", "kts", "scala", "groovy",
        "cs", "vb", "fs",
        "rs", "go", "rb", "php", "swift", "m", "mm",
        "sh", "bash", "zsh", "fish",
        "html", "htm", "xhtml", "xml", "yaml", "yml", "json", "json5", "toml",
        "ini", "cfg", "conf",
        "sql", "r", "lua", "pl", "pm", "dart", "ex", "exs", "erl", "hrl",
        "clj", "cljs", "edn", "hs", "ml", "mli",
        "css", "scss", "sass", "less", "vue", "svelte",
        "gradle", "cmake", "make", "mk", "rake", "gemspec",
    };
    return s;
}

}  // namespace

FileCategory classify(const QString& filePath)
{
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext.isEmpty()) {
        return FileCategory::Other;
    }
    if (videoExt().contains(ext))      return FileCategory::Video;
    if (imageExt().contains(ext))      return FileCategory::Image;
    if (audioExt().contains(ext))      return FileCategory::Audio;
    if (documentExt().contains(ext))   return FileCategory::Document;
    if (archiveExt().contains(ext))    return FileCategory::Archive;
    if (executableExt().contains(ext)) return FileCategory::Executable;
    if (codeExt().contains(ext))       return FileCategory::Code;
    return FileCategory::Other;
}

QString displayName(FileCategory cat)
{
    switch (cat) {
        case FileCategory::All:        return tr_("All");
        case FileCategory::Video:      return tr_("Video");
        case FileCategory::Image:      return tr_("Photo");
        case FileCategory::Audio:      return tr_("Audio");
        case FileCategory::Document:   return tr_("Documents");
        case FileCategory::Archive:    return tr_("Archives");
        case FileCategory::Executable: return tr_("Executables");
        case FileCategory::Code:       return tr_("Code");
        case FileCategory::Other:      return tr_("Other");
    }
    return QStringLiteral("?");
}

QString glyph(FileCategory cat)
{
    switch (cat) {
        case FileCategory::All:        return QStringLiteral("◎");
        case FileCategory::Video:      return QStringLiteral("▶");
        case FileCategory::Image:      return QStringLiteral("◧");
        case FileCategory::Audio:      return QStringLiteral("♪");
        case FileCategory::Document:   return QStringLiteral("≡");
        case FileCategory::Archive:    return QStringLiteral("◰");
        case FileCategory::Executable: return QStringLiteral("⚙");
        case FileCategory::Code:       return QStringLiteral("</>");
        case FileCategory::Other:      return QStringLiteral("•");
    }
    return QStringLiteral("?");
}

QList<FileCategory> all()
{
    return {
        FileCategory::All,
        FileCategory::Video,
        FileCategory::Image,
        FileCategory::Audio,
        FileCategory::Document,
        FileCategory::Archive,
        FileCategory::Executable,
        FileCategory::Code,
        FileCategory::Other,
    };
}

}  // namespace FileType
