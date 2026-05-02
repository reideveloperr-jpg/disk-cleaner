"""Безопасные действия над файлами: удаление в корзину + журналирование."""

from __future__ import annotations

import contextlib
import json
import logging
import os
import time
from collections.abc import Iterable, Iterator
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import IO

import send2trash

logger = logging.getLogger(__name__)


@dataclass
class ActionResult:
    """Результат одной попытки удаления."""

    path: str
    size: int
    ok: bool
    dry_run: bool
    error: str | None = None
    timestamp: float = 0.0


def _file_size_safe(path: str) -> int:
    try:
        return os.path.getsize(path)
    except OSError:
        return 0


@contextlib.contextmanager
def _open_log(log_path: str | os.PathLike[str] | None) -> Iterator[IO[str] | None]:
    if log_path is None:
        yield None
        return
    p = Path(log_path)
    p.parent.mkdir(parents=True, exist_ok=True)
    with open(p, "a", encoding="utf-8") as fp:
        yield fp


def trash_paths(
    paths: Iterable[str],
    *,
    dry_run: bool = True,
    log_path: str | os.PathLike[str] | None = None,
) -> list[ActionResult]:
    """Перемещает указанные файлы в системную корзину.

    :param dry_run: если True, ничего не удаляет — только записывает план в лог.
    :param log_path: путь к JSONL-файлу с журналом действий.
    """
    results: list[ActionResult] = []
    with _open_log(log_path) as log_fp:
        for p in paths:
            size = _file_size_safe(p)
            ts = time.time()
            if dry_run:
                res = ActionResult(path=p, size=size, ok=True, dry_run=True, timestamp=ts)
            else:
                try:
                    send2trash.send2trash(p)
                    res = ActionResult(path=p, size=size, ok=True, dry_run=False, timestamp=ts)
                except Exception as exc:  # send2trash bubbles many OS-specific exceptions
                    res = ActionResult(
                        path=p, size=size, ok=False, dry_run=False, error=str(exc), timestamp=ts
                    )
            results.append(res)
            if log_fp is not None:
                log_fp.write(json.dumps(asdict(res), ensure_ascii=False) + "\n")
        if log_fp is not None:
            log_fp.flush()

    return results


def summarize(results: Iterable[ActionResult]) -> tuple[int, int, int]:
    """Возвращает ``(ok_count, failed_count, freed_bytes)``."""
    ok = 0
    failed = 0
    freed = 0
    for r in results:
        if r.ok:
            ok += 1
            freed += r.size
        else:
            failed += 1
    return ok, failed, freed
