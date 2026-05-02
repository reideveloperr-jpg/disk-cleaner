"""Фоновые QThread-работники для сканирования и поиска дубликатов.

GUI-поток должен оставаться отзывчивым, поэтому всё IO-тяжёлое выполняется
в отдельных потоках, общающихся с UI через сигналы Qt.
"""

from __future__ import annotations

import threading

from PySide6.QtCore import QObject, QThread, Signal

from ..duplicates import DuplicateGroup, find_duplicates
from ..junk import JunkItem, find_all
from ..scanner import FileInfo, ScanResult, scan


class ScanWorker(QThread):
    """Сканирует ФС в отдельном потоке."""

    progress = Signal(int, int, str)   # files, bytes, current path
    finished_ok = Signal(object)        # ScanResult

    def __init__(self, root: str, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._root = root
        self._cancel = threading.Event()

    def cancel(self) -> None:
        self._cancel.set()

    def run(self) -> None:
        result: ScanResult = scan(
            self._root,
            cancel_event=self._cancel,
            progress_cb=lambda f, b, p: self.progress.emit(f, b, p),
        )
        self.finished_ok.emit(result)


class JunkWorker(QThread):
    """Поиск мусорных категорий поверх результата сканирования."""

    finished_ok = Signal(dict)  # dict[str, list[JunkItem]]

    def __init__(self, files: list[FileInfo], parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._files = files

    def run(self) -> None:
        groups: dict[str, list[JunkItem]] = find_all(self._files)
        self.finished_ok.emit(groups)


class DuplicatesWorker(QThread):
    """Поиск дубликатов в отдельном потоке."""

    progress = Signal(int, int)  # processed, total
    finished_ok = Signal(list)   # list[DuplicateGroup]

    def __init__(self, files: list[FileInfo], parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._files = files
        self._cancel = threading.Event()

    def cancel(self) -> None:
        self._cancel.set()

    def run(self) -> None:
        groups: list[DuplicateGroup] = find_duplicates(
            self._files,
            cancel_event=self._cancel,
            progress_cb=lambda p, t: self.progress.emit(p, t),
        )
        self.finished_ok.emit(groups)
