"""Категоризация файлов по типу на основе расширения."""

from __future__ import annotations

from enum import Enum


class FileCategory(str, Enum):
    VIDEO = "video"
    PHOTO = "photo"
    AUDIO = "audio"
    DOCUMENT = "document"
    ARCHIVE = "archive"
    CODE = "code"
    INSTALLER = "installer"
    OTHER = "other"


_EXT_MAP: dict[str, FileCategory] = {}


def _register(category: FileCategory, *exts: str) -> None:
    """Регистрирует расширения за категорией.

    Жёстко падает при коллизии — два разных правила, претендующих на одно
    расширение, скорее всего ошибка (как было с ``ts``: VIDEO vs CODE).
    Если расширение нужно в нескольких категориях, выбери одну явно.
    """
    for e in exts:
        key = e.lower()
        prev = _EXT_MAP.get(key)
        if prev is not None and prev is not category:
            raise ValueError(
                f"Extension {key!r} already registered for {prev!r}, "
                f"refusing to overwrite with {category!r}"
            )
        _EXT_MAP[key] = category


# `ts` оставлен в VIDEO (MPEG Transport Stream): для disk-cleaner такие файлы
# обычно крупные, а TypeScript-исходники — мелкие и редко занимают место.
_register(
    FileCategory.VIDEO,
    "mp4", "mkv", "mov", "avi", "webm", "m4v", "mpg", "mpeg",
    "flv", "wmv", "ts", "vob", "3gp", "ogv",
)
_register(
    FileCategory.PHOTO,
    "jpg", "jpeg", "png", "heic", "heif", "webp", "gif", "bmp",
    "tiff", "tif", "raw", "cr2", "cr3", "nef", "arw", "dng",
    "orf", "rw2", "svg", "ico",
)
_register(
    FileCategory.AUDIO,
    "mp3", "flac", "wav", "m4a", "aac", "ogg", "opus", "wma",
    "alac", "aiff", "aif", "ape", "mid", "midi",
)
_register(
    FileCategory.DOCUMENT,
    "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "txt",
    "md", "rtf", "odt", "ods", "odp", "epub", "mobi", "azw",
    "azw3", "djvu", "pages", "numbers", "key", "csv", "tsv",
)
_register(
    FileCategory.ARCHIVE,
    "zip", "rar", "7z", "tar", "gz", "tgz", "bz2", "tbz2",
    "xz", "txz", "zst", "lz", "lzma", "iso", "img", "dmg",
    "cab", "ar", "wim",
)
_register(
    FileCategory.CODE,
    "py", "js", "mjs", "cjs", "tsx", "jsx", "go", "rs",
    "c", "cc", "cpp", "cxx", "h", "hpp", "hh", "java", "kt",
    "kts", "swift", "scala", "rb", "php", "pl", "lua", "sh",
    "bash", "zsh", "fish", "ps1", "psm1", "bat", "cmd", "json",
    "yaml", "yml", "toml", "xml", "html", "htm", "css", "scss",
    "sass", "less", "vue", "svelte", "sql", "graphql", "proto",
    "ini", "cfg", "conf", "env", "lock", "nix", "dart", "r",
    "jl", "ex", "exs", "erl", "hs", "clj", "fs", "fsx",
)
_register(
    FileCategory.INSTALLER,
    "exe", "msi", "deb", "rpm", "pkg", "apk", "aab", "appimage",
    "snap", "flatpak", "msix",
)


def classify(name: str) -> FileCategory:
    """Возвращает категорию файла по его имени.

    Использует последний компонент после точки. Для имён без точки
    или скрытых файлов вроде ``.bashrc`` возвращает ``OTHER``.
    """
    dot = name.rfind(".")
    if dot <= 0 or dot == len(name) - 1:
        return FileCategory.OTHER
    ext = name[dot + 1 :].lower()
    return _EXT_MAP.get(ext, FileCategory.OTHER)


CATEGORY_LABEL: dict[FileCategory, str] = {
    FileCategory.VIDEO: "Видео",
    FileCategory.PHOTO: "Фото",
    FileCategory.AUDIO: "Аудио",
    FileCategory.DOCUMENT: "Документы",
    FileCategory.ARCHIVE: "Архивы",
    FileCategory.CODE: "Код",
    FileCategory.INSTALLER: "Установщики",
    FileCategory.OTHER: "Прочее",
}

CATEGORY_ICON: dict[FileCategory, str] = {
    FileCategory.VIDEO: "🎬",
    FileCategory.PHOTO: "🖼",
    FileCategory.AUDIO: "🎵",
    FileCategory.DOCUMENT: "📄",
    FileCategory.ARCHIVE: "🗜",
    FileCategory.CODE: "💻",
    FileCategory.INSTALLER: "📦",
    FileCategory.OTHER: "❔",
}
