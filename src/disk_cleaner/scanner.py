"""Сканер диска: рекурсивный обход с агрегацией размеров и классификацией.

Ключевые особенности:
- Использует ``os.scandir`` (быстрее, чем ``os.walk``: stat-кэш).
- Дедуп по ``(st_dev, st_ino)`` — hard-link'и считаются один раз.
- Игнорирует переходы через границу ФС (по умолчанию).
- Поддерживает отмену через ``threading.Event``.
- Сообщает прогресс через колбэк.
"""

from __future__ import annotations

import os
import threading
from collections.abc import Callable, Iterable
from dataclasses import dataclass, field
from pathlib import Path

from .classifier import FileCategory, classify


@dataclass
class FileInfo:
    """Описание одного просканированного файла."""

    path: str
    size: int
    mtime: float
    category: FileCategory


@dataclass
class DirNode:
    """Узел дерева каталогов с агрегированным размером."""

    path: str
    name: str
    size: int = 0
    file_count: int = 0
    children: list[DirNode] = field(default_factory=list)
    parent: DirNode | None = None


@dataclass
class ScanResult:
    """Результат полного сканирования."""

    root: DirNode
    files: list[FileInfo]
    by_category: dict[FileCategory, int]
    total_size: int
    total_files: int
    errors: list[str]
    cancelled: bool = False


# Папки, которые точно не нужно обходить
_SKIP_DIR_NAMES: frozenset[str] = frozenset(
    {
        # Linux/macOS
        "/proc", "/sys", "/dev", "/run", "/snap", "/var/run",
        # Windows
        "System Volume Information", "$Recycle.Bin", "$RECYCLE.BIN",
        "Config.Msi", "Recovery", "MSOCache",
    }
)


def _should_skip_dir(path: str, name: str) -> bool:
    return name in _SKIP_DIR_NAMES or path in _SKIP_DIR_NAMES


ProgressCallback = Callable[[int, int, str], None]
"""Колбэк прогресса: ``(files_seen, bytes_seen, current_path) -> None``."""


def scan(
    root: str | os.PathLike[str],
    *,
    cancel_event: threading.Event | None = None,
    progress_cb: ProgressCallback | None = None,
    progress_every: int = 500,
    cross_filesystem: bool = False,
    follow_symlinks: bool = False,
) -> ScanResult:
    """Рекурсивно сканирует дерево, начиная с ``root``.

    :param root: корневой путь.
    :param cancel_event: установка флага останавливает обход.
    :param progress_cb: колбэк прогресса.
    :param progress_every: как часто звать колбэк (раз в N файлов).
    :param cross_filesystem: пересекать ли границу ФС (по умолчанию нет).
    :param follow_symlinks: следовать ли по симлинкам каталогов (по умолчанию нет).
    """
    root_path = os.fspath(Path(root).expanduser())
    root_path = os.path.abspath(root_path)

    errors: list[str] = []
    files: list[FileInfo] = []
    by_category: dict[FileCategory, int] = {c: 0 for c in FileCategory}
    seen_inodes: set[tuple[int, int]] = set()

    try:
        root_st = os.stat(root_path)
    except OSError as exc:
        return ScanResult(
            root=DirNode(path=root_path, name=os.path.basename(root_path) or root_path),
            files=[],
            by_category=by_category,
            total_size=0,
            total_files=0,
            errors=[f"{root_path}: {exc}"],
            cancelled=False,
        )

    root_dev = root_st.st_dev
    root_node = DirNode(path=root_path, name=os.path.basename(root_path) or root_path)
    cancelled = False

    # Стек для итеративного обхода: (директория ФС, узел дерева)
    stack: list[tuple[str, DirNode]] = [(root_path, root_node)]

    files_seen = 0
    bytes_seen = 0

    while stack:
        if cancel_event is not None and cancel_event.is_set():
            cancelled = True
            break

        dir_path, dir_node = stack.pop()

        try:
            entries: Iterable[os.DirEntry[str]] = list(os.scandir(dir_path))
        except OSError as exc:
            errors.append(f"{dir_path}: {exc}")
            continue

        for entry in entries:
            if cancel_event is not None and cancel_event.is_set():
                cancelled = True
                break

            try:
                # follow_symlinks=False: stat'им сам entry, не цель симлинка
                st = entry.stat(follow_symlinks=follow_symlinks)
            except OSError as exc:
                errors.append(f"{entry.path}: {exc}")
                continue

            try:
                is_dir = entry.is_dir(follow_symlinks=follow_symlinks)
            except OSError as exc:
                errors.append(f"{entry.path}: {exc}")
                continue

            if is_dir:
                if _should_skip_dir(entry.path, entry.name):
                    continue
                if not cross_filesystem and st.st_dev != root_dev:
                    continue
                child = DirNode(path=entry.path, name=entry.name, parent=dir_node)
                dir_node.children.append(child)
                stack.append((entry.path, child))
                continue

            # Обычный файл (или симлинк-на-файл, который мы НЕ следуем)
            try:
                is_file = entry.is_file(follow_symlinks=follow_symlinks)
            except OSError as exc:
                errors.append(f"{entry.path}: {exc}")
                continue
            if not is_file:
                continue

            # Дедуп по inode для hard-link'ов (только если nlink > 1)
            inode_key: tuple[int, int] | None = None
            if hasattr(st, "st_nlink") and st.st_nlink > 1 and st.st_ino:
                inode_key = (st.st_dev, st.st_ino)
                if inode_key in seen_inodes:
                    # уже учли через другой hard-link
                    continue
                seen_inodes.add(inode_key)

            size = st.st_size
            cat = classify(entry.name)
            files.append(
                FileInfo(
                    path=entry.path,
                    size=size,
                    mtime=st.st_mtime,
                    category=cat,
                )
            )
            by_category[cat] += size
            dir_node.file_count += 1
            dir_node.size += size

            files_seen += 1
            bytes_seen += size

            if progress_cb is not None and files_seen % progress_every == 0:
                progress_cb(files_seen, bytes_seen, entry.path)

        if cancelled:
            break

    # Поднимаем размеры вверх по дереву (агрегируем)
    _aggregate_sizes(root_node)

    if progress_cb is not None:
        progress_cb(files_seen, bytes_seen, root_path)

    return ScanResult(
        root=root_node,
        files=files,
        by_category=by_category,
        total_size=bytes_seen,
        total_files=files_seen,
        errors=errors,
        cancelled=cancelled,
    )


def _aggregate_sizes(node: DirNode) -> tuple[int, int]:
    """Рекурсивно агрегирует размер и количество файлов вверх по дереву.

    Возвращает ``(total_size, total_file_count)``. После вызова поля
    ``node.size`` и ``node.file_count`` отражают сумму по всему поддереву,
    включая непосредственные файлы и все вложенные папки.
    """
    total_size = node.size  # накоплено по непосредственным файлам
    total_count = node.file_count
    for child in node.children:
        child_size, child_count = _aggregate_sizes(child)
        total_size += child_size
        total_count += child_count
    node.size = total_size
    node.file_count = total_count
    return total_size, total_count


def top_n_largest(
    files: Iterable[FileInfo],
    n: int = 100,
    *,
    categories: Iterable[FileCategory] | None = None,
) -> list[FileInfo]:
    """Возвращает топ-N самых больших файлов, опционально отфильтрованных по категориям."""
    if categories is not None:
        cat_set = set(categories)
        filtered: list[FileInfo] = [f for f in files if f.category in cat_set]
    else:
        filtered = list(files)
    filtered.sort(key=lambda f: f.size, reverse=True)
    return filtered[:n]


def format_size(num_bytes: float) -> str:
    """Форматирует число байтов в человекочитаемый вид."""
    units = ["B", "KB", "MB", "GB", "TB", "PB"]
    n = float(num_bytes)
    for u in units:
        if abs(n) < 1024.0 or u == units[-1]:
            return f"{n:.1f} {u}" if u != "B" else f"{int(n)} {u}"
        n /= 1024.0
    return f"{n:.1f} PB"
