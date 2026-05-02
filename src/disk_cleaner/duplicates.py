"""Поиск файлов-дубликатов.

Двухпроходный алгоритм:
1. Группируем кандидатов по размеру (ничего не читаем с диска).
2. Внутри групп с одинаковым размером считаем хеш BLAKE2b.

BLAKE2b быстрее SHA-256 и достаточно стоек для дедупа. Читаем чанками,
чтобы не грузить большие файлы целиком.
"""

from __future__ import annotations

import hashlib
import threading
from collections import defaultdict
from collections.abc import Callable, Iterable
from dataclasses import dataclass

from .scanner import FileInfo

CHUNK = 1 << 20  # 1 MiB


@dataclass
class DuplicateGroup:
    """Группа файлов с одинаковым содержимым."""

    digest: str
    size: int
    paths: list[str]

    @property
    def wasted(self) -> int:
        """Сколько байтов будет освобождено, если оставить один файл."""
        return self.size * (len(self.paths) - 1)


def _hash_file(path: str, *, cancel_event: threading.Event | None = None) -> str | None:
    h = hashlib.blake2b(digest_size=16)
    try:
        with open(path, "rb", buffering=0) as fp:
            while True:
                if cancel_event is not None and cancel_event.is_set():
                    return None
                chunk = fp.read(CHUNK)
                if not chunk:
                    break
                h.update(chunk)
    except OSError:
        return None
    return h.hexdigest()


def find_duplicates(
    files: Iterable[FileInfo],
    *,
    min_size: int = 4096,
    cancel_event: threading.Event | None = None,
    progress_cb: Callable[[int, int], None] | None = None,
) -> list[DuplicateGroup]:
    """Находит дубликаты среди ``files``.

    :param min_size: пропускает файлы меньше этого размера (мелочь не интересна).
    :param progress_cb: ``(processed, total) -> None`` — для индикатора прогресса.
    """
    by_size: dict[int, list[FileInfo]] = defaultdict(list)
    for f in files:
        if f.size >= min_size:
            by_size[f.size].append(f)

    candidates: list[FileInfo] = [
        f for group in by_size.values() if len(group) > 1 for f in group
    ]
    total = len(candidates)

    by_hash: dict[tuple[int, str], list[str]] = defaultdict(list)
    processed = 0
    for f in candidates:
        if cancel_event is not None and cancel_event.is_set():
            break
        digest = _hash_file(f.path, cancel_event=cancel_event)
        processed += 1
        if digest is None:
            if progress_cb is not None:
                progress_cb(processed, total)
            continue
        by_hash[(f.size, digest)].append(f.path)
        if progress_cb is not None and (processed % 50 == 0 or processed == total):
            progress_cb(processed, total)

    groups: list[DuplicateGroup] = [
        DuplicateGroup(digest=digest, size=size, paths=paths)
        for (size, digest), paths in by_hash.items()
        if len(paths) > 1
    ]
    groups.sort(key=lambda g: g.wasted, reverse=True)
    return groups
