"""Декларативные правила обнаружения "мусора".

Каждое правило знает, как найти кандидатов в своей категории.
Удаление не делается здесь — только обнаружение.
"""

from __future__ import annotations

import os
import sys
import time
from collections.abc import Iterable, Iterator
from dataclasses import dataclass
from pathlib import Path

from .scanner import FileInfo


@dataclass(frozen=True)
class JunkRule:
    """Описание одного правила поиска мусора."""

    key: str
    label: str
    description: str
    requires_admin: bool = False


@dataclass
class JunkItem:
    """Один найденный объект-кандидат на удаление."""

    rule: str
    path: str
    size: int
    reason: str


# Правила (для UI-отображения)
RULES: list[JunkRule] = [
    JunkRule("trash", "Корзина", "Файлы в корзине системы"),
    JunkRule("temp", "Временные файлы", "Файлы из %TEMP% / /tmp старше 7 дней"),
    JunkRule("browser_cache", "Кэш браузеров", "Chrome / Chromium / Firefox / Edge"),
    JunkRule("thumbs", "Превью / эскизы", "Thumbs.db, .DS_Store, IconCache.db"),
    JunkRule("old_large", "Большие старые файлы", "Файлы >100 MB не открывались >180 дней"),
]

DAY = 86400.0


def _exists(p: str | os.PathLike[str]) -> bool:
    try:
        return os.path.exists(p)
    except OSError:
        return False


def _iter_files(root: str, *, follow_symlinks: bool = False) -> Iterator[tuple[str, int, float]]:
    """Возвращает (path, size, mtime) для всех файлов под root. Тихо пропускает ошибки."""
    stack: list[str] = [root]
    while stack:
        d = stack.pop()
        try:
            it = os.scandir(d)
        except OSError:
            continue
        with it:
            for entry in it:
                try:
                    if entry.is_dir(follow_symlinks=follow_symlinks):
                        stack.append(entry.path)
                        continue
                    if not entry.is_file(follow_symlinks=follow_symlinks):
                        continue
                    st = entry.stat(follow_symlinks=follow_symlinks)
                    yield entry.path, st.st_size, st.st_mtime
                except OSError:
                    continue


# ----------- Корзина -----------


def _trash_paths() -> list[str]:
    paths: list[str] = []
    home = Path.home()
    if sys.platform == "darwin":
        paths.append(str(home / ".Trash"))
    elif sys.platform.startswith("win"):
        # На Windows корзину не сканируем напрямую — это виртуальная папка;
        # оставляем как заглушку: чистка делается через SHEmptyRecycleBin
        # или через \$Recycle.Bin (требует прав).
        pass
    else:
        # Linux — XDG
        xdg = os.environ.get("XDG_DATA_HOME") or str(home / ".local" / "share")
        paths.append(os.path.join(xdg, "Trash", "files"))
        paths.append(os.path.join(xdg, "Trash", "info"))
    return [p for p in paths if _exists(p)]


def find_trash() -> list[JunkItem]:
    items: list[JunkItem] = []
    for root in _trash_paths():
        for path, size, _mt in _iter_files(root):
            items.append(JunkItem("trash", path, size, "В корзине"))
    return items


# ----------- Временные файлы -----------


def _temp_dirs() -> list[str]:
    paths: list[str] = []
    if sys.platform.startswith("win"):
        for var in ("TEMP", "TMP", "LOCALAPPDATA"):
            v = os.environ.get(var)
            if not v:
                continue
            if var == "LOCALAPPDATA":
                paths.append(os.path.join(v, "Temp"))
            else:
                paths.append(v)
    else:
        paths.extend(["/tmp", "/var/tmp"])
        # пользовательский tmp на macOS
        for v in ("TMPDIR",):
            value = os.environ.get(v)
            if value:
                paths.append(value)
    # dedup, only existing
    seen: set[str] = set()
    out: list[str] = []
    for p in paths:
        ap = os.path.abspath(p)
        if ap in seen or not _exists(ap):
            continue
        seen.add(ap)
        out.append(ap)
    return out


def find_temp(min_age_days: float = 7.0) -> list[JunkItem]:
    items: list[JunkItem] = []
    cutoff = time.time() - min_age_days * DAY
    for root in _temp_dirs():
        for path, size, mtime in _iter_files(root):
            if mtime < cutoff:
                items.append(
                    JunkItem(
                        "temp",
                        path,
                        size,
                        f"Не менялся {int((time.time() - mtime) / DAY)} дн.",
                    )
                )
    return items


# ----------- Кэш браузеров -----------


def _browser_cache_dirs() -> list[str]:
    home = Path.home()
    candidates: list[str] = []
    if sys.platform == "darwin":
        base = home / "Library" / "Caches"
        candidates += [
            str(base / "Google" / "Chrome"),
            str(base / "Chromium"),
            str(base / "com.apple.Safari"),
            str(base / "Firefox"),
            str(base / "Microsoft Edge"),
        ]
    elif sys.platform.startswith("win"):
        local = os.environ.get("LOCALAPPDATA")
        if local:
            candidates += [
                os.path.join(local, "Google", "Chrome", "User Data", "Default", "Cache"),
                os.path.join(local, "Chromium", "User Data", "Default", "Cache"),
                os.path.join(local, "Microsoft", "Edge", "User Data", "Default", "Cache"),
                os.path.join(local, "Mozilla", "Firefox", "Profiles"),
            ]
    else:
        candidates += [
            str(home / ".cache" / "google-chrome"),
            str(home / ".cache" / "chromium"),
            str(home / ".cache" / "mozilla" / "firefox"),
            str(home / ".cache" / "microsoft-edge"),
            str(home / ".cache" / "BraveSoftware"),
        ]
    return [c for c in candidates if _exists(c)]


def find_browser_caches() -> list[JunkItem]:
    items: list[JunkItem] = []
    for root in _browser_cache_dirs():
        for path, size, _mt in _iter_files(root):
            items.append(JunkItem("browser_cache", path, size, "Кэш браузера"))
    return items


# ----------- Thumbs / .DS_Store / IconCache -----------


_THUMB_NAMES: frozenset[str] = frozenset({"Thumbs.db", ".DS_Store", "IconCache.db", "ehthumbs.db"})


def find_thumbs(scan_files: Iterable[FileInfo]) -> list[JunkItem]:
    """Эту категорию ищем поверх результатов общего сканирования (быстрее)."""
    out: list[JunkItem] = []
    for f in scan_files:
        if os.path.basename(f.path) in _THUMB_NAMES:
            out.append(JunkItem("thumbs", f.path, f.size, "Системный эскиз/превью"))
    return out


# ----------- Большие старые файлы -----------


def find_old_large(
    scan_files: Iterable[FileInfo],
    *,
    min_size_mb: float = 100.0,
    min_age_days: float = 180.0,
) -> list[JunkItem]:
    out: list[JunkItem] = []
    cutoff = time.time() - min_age_days * DAY
    min_size = int(min_size_mb * 1024 * 1024)
    for f in scan_files:
        if f.size >= min_size and f.mtime < cutoff:
            age_days = int((time.time() - f.mtime) / DAY)
            out.append(
                JunkItem(
                    "old_large",
                    f.path,
                    f.size,
                    f"{f.size / 1024 / 1024:.0f} MB, не менялся {age_days} дн.",
                )
            )
    out.sort(key=lambda x: x.size, reverse=True)
    return out


def find_all(scan_files: Iterable[FileInfo]) -> dict[str, list[JunkItem]]:
    """Запускает все правила и возвращает группы.

    ``scan_files`` — результат уже выполненного :func:`scanner.scan` (для
    правил, которым нужен общий список файлов: thumbs, old_large).
    """
    files_list = list(scan_files)
    return {
        "trash": find_trash(),
        "temp": find_temp(),
        "browser_cache": find_browser_caches(),
        "thumbs": find_thumbs(files_list),
        "old_large": find_old_large(files_list),
    }
