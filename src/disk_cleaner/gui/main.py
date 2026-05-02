"""Главное окно PySide6 GUI для Volchay Cleans."""

from __future__ import annotations

import os
import sys
from importlib import resources
from pathlib import Path
from typing import cast

from PySide6.QtCore import QSize, Qt
from PySide6.QtGui import QAction, QActionGroup, QIcon
from PySide6.QtWidgets import (
    QAbstractItemView,
    QApplication,
    QCheckBox,
    QFileDialog,
    QFrame,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QStatusBar,
    QTabWidget,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)

from .. import __version__
from ..actions import summarize, trash_paths
from ..classifier import CATEGORY_LABEL, FileCategory
from ..duplicates import DuplicateGroup
from ..junk import RULES, JunkItem
from ..scanner import DirNode, ScanResult, format_size, top_n_largest
from .spinner import BusySpinner
from .theme import ThemeMode, apply_theme, load_saved_mode, save_mode
from .workers import DuplicatesWorker, JunkWorker, ScanWorker

APP_NAME = "Volchay Cleans"
ORG_NAME = "volchay-cleans"
LOG_PATH = Path.home() / ".volchay-cleans" / "actions.log.jsonl"


def _logo_path() -> str:
    """Абсолютный путь к SVG-логотипу внутри пакета."""
    with resources.as_file(resources.files("disk_cleaner.assets") / "volchay_logo.svg") as p:
        return str(p)


class _SizeItem(QTreeWidgetItem):
    """QTreeWidgetItem с правильной сортировкой по числовому размеру."""

    def __init__(self, columns: list[str], size: int) -> None:
        super().__init__(columns)
        self._size = size
        # выравниваем колонку размера вправо
        self.setTextAlignment(1, Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)

    def __lt__(self, other: QTreeWidgetItem) -> bool:
        col = self.treeWidget().sortColumn() if self.treeWidget() else 0
        if isinstance(other, _SizeItem) and col == 1:
            return self._size < other._size
        return super().__lt__(other)


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(f"{APP_NAME} {__version__}")
        self.setWindowIcon(QIcon(_logo_path()))
        self.resize(1100, 720)

        self._scan_result: ScanResult | None = None
        self._scan_worker: ScanWorker | None = None
        self._junk_worker: JunkWorker | None = None
        self._dup_worker: DuplicatesWorker | None = None

        # Центральный layout
        central = QWidget(self)
        root = QVBoxLayout(central)
        root.setContentsMargins(10, 10, 10, 6)

        # Бренд-хедер: логотип волка («Volchay») + слово марки «Cleans»
        brand_row = QHBoxLayout()
        brand_row.setContentsMargins(0, 2, 0, 6)
        brand_row.setSpacing(10)
        brand_logo = QLabel()
        brand_logo.setPixmap(
            QIcon(_logo_path()).pixmap(QSize(36, 36))
        )
        brand_logo.setFixedSize(36, 36)
        brand_text = QLabel("Cleans")
        brand_text.setObjectName("wordmark")
        brand_row.addWidget(brand_logo)
        brand_row.addWidget(brand_text)
        brand_row.addStretch(1)
        root.addLayout(brand_row)

        # Панель пути + кнопок
        path_bar = QHBoxLayout()
        self.path_edit = QLineEdit(str(Path.home()))
        self.path_edit.setPlaceholderText("Путь для сканирования")
        # компактное поле — примерно треть ширины окна
        self.path_edit.setFixedWidth(330)
        browse_btn = QPushButton("Обзор…")
        browse_btn.clicked.connect(self._browse)
        self.scan_btn = QPushButton("Сканировать")
        self.scan_btn.setDefault(True)
        self.scan_btn.clicked.connect(self._toggle_scan)
        path_bar.addWidget(QLabel("Папка:"))
        path_bar.addWidget(self.path_edit)
        path_bar.addWidget(browse_btn)
        path_bar.addWidget(self.scan_btn)
        path_bar.addStretch(1)
        root.addLayout(path_bar)

        # Прогресс — спиннер вместо полоски
        progress_row = QHBoxLayout()
        progress_row.setContentsMargins(0, 0, 0, 0)
        progress_row.setSpacing(10)
        self.spinner = BusySpinner(self, diameter=22, thickness=3)
        self.progress_label = QLabel("")
        self.progress_label.setObjectName("secondary")
        progress_row.addWidget(self.spinner)
        progress_row.addWidget(self.progress_label, 1)
        root.addLayout(progress_row)

        # Сводка по категориям
        self.summary_label = QLabel("Нет данных. Нажми «Сканировать».")
        root.addWidget(self.summary_label)

        # Контентная зона: слева — сайдбар «Тип», справа — вкладки
        content_row = QHBoxLayout()
        content_row.setContentsMargins(0, 0, 0, 0)
        content_row.setSpacing(12)

        # Сайдбар: вертикальный список чипов категорий + быстрые пресеты
        sidebar = QFrame()
        sidebar.setObjectName("sidebar")
        sidebar.setFixedWidth(180)
        sidebar_layout = QVBoxLayout(sidebar)
        sidebar_layout.setContentsMargins(10, 10, 10, 10)
        sidebar_layout.setSpacing(6)
        type_label = QLabel("Тип")
        type_label.setObjectName("secondary")
        sidebar_layout.addWidget(type_label)
        self.category_checks: dict[FileCategory, QCheckBox] = {}
        for cat in FileCategory:
            cb = QCheckBox(CATEGORY_LABEL[cat])
            cb.setChecked(True)
            cb.setProperty("role", "chip")
            cb.setCursor(Qt.CursorShape.PointingHandCursor)
            cb.stateChanged.connect(self._refresh_top_files)
            sidebar_layout.addWidget(cb)
            self.category_checks[cat] = cb
        sidebar_layout.addSpacing(12)
        presets_label = QLabel("Быстрый выбор")
        presets_label.setObjectName("secondary")
        sidebar_layout.addWidget(presets_label)
        only_video_btn = QPushButton("Только видео")
        only_video_btn.clicked.connect(lambda: self._set_categories({FileCategory.VIDEO}))
        only_photo_btn = QPushButton("Только фото")
        only_photo_btn.clicked.connect(lambda: self._set_categories({FileCategory.PHOTO}))
        all_btn = QPushButton("Все")
        all_btn.clicked.connect(lambda: self._set_categories(set(FileCategory)))
        sidebar_layout.addWidget(only_video_btn)
        sidebar_layout.addWidget(only_photo_btn)
        sidebar_layout.addWidget(all_btn)
        sidebar_layout.addStretch(1)
        content_row.addWidget(sidebar)

        # Вкладки
        self.tabs = QTabWidget()
        content_row.addWidget(self.tabs, 1)
        root.addLayout(content_row, 1)

        # Вкладка 1: Топ файлов с фильтром по типу
        self.files_tree = QTreeWidget()
        self.files_tree.setHeaderLabels(["Файл", "Размер", "Тип", "Изменён"])
        self.files_tree.setRootIsDecorated(False)
        self.files_tree.setSortingEnabled(True)
        self.files_tree.setAlternatingRowColors(True)
        self.files_tree.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        self.files_tree.header().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.files_tree.header().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        self.files_tree.header().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        self.files_tree.header().setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        self.tabs.addTab(self.files_tree, "По типу")

        # Вкладка 2: Дерево папок
        self.dirs_tree = QTreeWidget()
        self.dirs_tree.setHeaderLabels(["Папка", "Размер", "Файлов"])
        self.dirs_tree.setSortingEnabled(False)
        self.dirs_tree.setAlternatingRowColors(True)
        self.dirs_tree.header().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.dirs_tree.header().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        self.dirs_tree.header().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        self.tabs.addTab(self.dirs_tree, "По папкам")

        # Вкладка 3: Мусор
        self.junk_tree = QTreeWidget()
        self.junk_tree.setHeaderLabels(["Файл / Категория", "Размер", "Причина"])
        self.junk_tree.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        self.junk_tree.header().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.junk_tree.header().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        self.junk_tree.header().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        self.tabs.addTab(self.junk_tree, "Мусор")

        # Вкладка 4: Дубликаты
        dup_widget = QWidget()
        dup_layout = QVBoxLayout(dup_widget)
        dup_layout.setContentsMargins(0, 0, 0, 0)
        self.dup_btn = QPushButton("Найти дубликаты в текущем сканировании")
        self.dup_btn.clicked.connect(self._find_duplicates)
        self.dup_btn.setEnabled(False)
        self.dup_tree = QTreeWidget()
        self.dup_tree.setHeaderLabels(["Группа / Файл", "Размер", "Можно освободить"])
        self.dup_tree.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)
        self.dup_tree.header().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.dup_tree.header().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        self.dup_tree.header().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        dup_layout.addWidget(self.dup_btn)
        dup_layout.addWidget(self.dup_tree, 1)
        self.tabs.addTab(dup_widget, "Дубликаты")

        # Низ: dry-run + удалить
        bottom = QHBoxLayout()
        self.dry_run_cb = QCheckBox("Dry run (не удалять, только показать план)")
        self.dry_run_cb.setChecked(True)
        self.delete_btn = QPushButton("Удалить выбранное (в корзину)")
        self.delete_btn.setProperty("role", "danger")
        self.delete_btn.clicked.connect(self._delete_selected)
        bottom.addWidget(self.dry_run_cb)
        bottom.addStretch(1)
        bottom.addWidget(self.delete_btn)
        root.addLayout(bottom)

        self.setCentralWidget(central)
        self.setStatusBar(QStatusBar(self))

        # File menu
        file_menu = self.menuBar().addMenu("Файл")
        quit_action = QAction("Выход", self)
        quit_action.setShortcut("Ctrl+Q")
        quit_action.triggered.connect(self.close)
        file_menu.addAction(quit_action)

        # View menu — выбор темы
        view_menu = self.menuBar().addMenu("Вид")
        theme_menu = view_menu.addMenu("Тема")
        self._theme_group = QActionGroup(self)
        self._theme_group.setExclusive(True)
        current_mode = load_saved_mode()
        for mode, label in (
            (ThemeMode.DARK, "Тёмная"),
            (ThemeMode.LIGHT, "Светлая"),
            (ThemeMode.SYSTEM, "Системная"),
        ):
            act = QAction(label, self, checkable=True)
            act.setData(mode.value)
            act.setChecked(mode is current_mode)
            act.triggered.connect(lambda _checked, m=mode: self._on_theme_selected(m))
            self._theme_group.addAction(act)
            theme_menu.addAction(act)

    # ---------- Действия пользователя ----------

    def _browse(self) -> None:
        d = QFileDialog.getExistingDirectory(self, "Выбери папку", self.path_edit.text())
        if d:
            self.path_edit.setText(d)

    def _toggle_scan(self) -> None:
        if self._scan_worker is not None and self._scan_worker.isRunning():
            self._scan_worker.cancel()
            self.scan_btn.setText("Останавливаю…")
            self.scan_btn.setEnabled(False)
            return
        self._start_scan()

    def _start_scan(self) -> None:
        path = self.path_edit.text().strip()
        if not path or not os.path.isdir(path):
            QMessageBox.warning(self, "Ошибка", "Выбери существующую папку.")
            return
        self._reset_results()
        self.spinner.start()
        self.progress_label.setText(f"Сканирую {path}…")
        self.scan_btn.setText("Остановить")
        worker = ScanWorker(path, self)
        worker.progress.connect(self._on_scan_progress)
        worker.finished_ok.connect(self._on_scan_done)
        self._scan_worker = worker
        worker.start()

    def _on_scan_progress(self, files: int, bytes_: int, current: str) -> None:
        self.progress_label.setText(
            f"Файлов: {files}, размер: {format_size(bytes_)}, текущий: …{current[-60:]}"
        )

    def _on_scan_done(self, result: ScanResult) -> None:
        self._scan_result = result
        self._scan_worker = None
        self.spinner.stop()
        self.progress_label.setText(
            f"Готово: {result.total_files} файлов, {format_size(result.total_size)}"
            + (" (отменено)" if result.cancelled else "")
            + (f"; ошибок: {len(result.errors)}" if result.errors else "")
        )
        self.scan_btn.setText("Сканировать")
        self.scan_btn.setEnabled(True)
        self._populate_summary(result)
        self._refresh_top_files()
        self._populate_dirs(result.root)
        self.dup_btn.setEnabled(True)
        # Сразу запускаем поиск мусора (быстро)
        jw = JunkWorker(result.files, self)
        jw.finished_ok.connect(self._on_junk_done)
        self._junk_worker = jw
        jw.start()

    def _populate_summary(self, result: ScanResult) -> None:
        parts: list[str] = [
            f"Всего: {format_size(result.total_size)} в {result.total_files} файлах"
        ]
        for cat, size in sorted(result.by_category.items(), key=lambda kv: kv[1], reverse=True):
            if size == 0:
                continue
            parts.append(f"{CATEGORY_LABEL[cat]}: {format_size(size)}")
        self.summary_label.setText("   ".join(parts))

    def _set_categories(self, cats: set[FileCategory]) -> None:
        for c, cb in self.category_checks.items():
            cb.blockSignals(True)
            cb.setChecked(c in cats)
            cb.blockSignals(False)
        self._refresh_top_files()

    def _refresh_top_files(self) -> None:
        self.files_tree.clear()
        if self._scan_result is None:
            return
        selected: set[FileCategory] = {
            c for c, cb in self.category_checks.items() if cb.isChecked()
        }
        if not selected:
            return
        top = top_n_largest(self._scan_result.files, n=500, categories=selected)
        for f in top:
            mtime = _format_mtime(f.mtime)
            label = CATEGORY_LABEL[f.category]
            item = _SizeItem([f.path, format_size(f.size), label, mtime], f.size)
            item.setData(0, Qt.ItemDataRole.UserRole, f.path)
            self.files_tree.addTopLevelItem(item)
        self.files_tree.sortItems(1, Qt.SortOrder.DescendingOrder)
        # Сводка по выбранным категориям
        total = sum(f.size for f in self._scan_result.files if f.category in selected)
        count = sum(1 for f in self._scan_result.files if f.category in selected)
        self.statusBar().showMessage(
            f"Выбранные типы: {count} файлов, {format_size(total)}"
        )

    def _populate_dirs(self, root: DirNode) -> None:
        self.dirs_tree.clear()
        top = _SizeItem([root.name or root.path, format_size(root.size), str(root.file_count)], root.size)
        top.setData(0, Qt.ItemDataRole.UserRole, root.path)
        self.dirs_tree.addTopLevelItem(top)
        _build_dir_items(top, root)
        top.setExpanded(True)

    def _on_junk_done(self, groups: dict[str, list[JunkItem]]) -> None:
        self.junk_tree.clear()
        rule_by_key = {r.key: r for r in RULES}
        for r in RULES:
            items = groups.get(r.key, [])
            total = sum(i.size for i in items)
            head = _SizeItem(
                [f"{r.label} ({len(items)} файлов)", format_size(total), r.description],
                total,
            )
            head.setFirstColumnSpanned(False)
            self.junk_tree.addTopLevelItem(head)
            for it in items[:1000]:  # cap visible items per group
                child = _SizeItem([it.path, format_size(it.size), it.reason], it.size)
                child.setData(0, Qt.ItemDataRole.UserRole, it.path)
                head.addChild(child)
            head.setExpanded(False)
        # сводка
        total_junk = sum(sum(i.size for i in items) for items in groups.values())
        self.statusBar().showMessage(
            self.statusBar().currentMessage()
            + f"  •  Мусор: {format_size(total_junk)}"
        )
        _ = rule_by_key  # for future use

    # ---------- Дубликаты ----------

    def _find_duplicates(self) -> None:
        if self._scan_result is None:
            return
        self.dup_tree.clear()
        self.dup_btn.setEnabled(False)
        self.spinner.start()
        self.progress_label.setText("Поиск дубликатов…")
        worker = DuplicatesWorker(self._scan_result.files, self)
        worker.progress.connect(self._on_dup_progress)
        worker.finished_ok.connect(self._on_dup_done)
        self._dup_worker = worker
        worker.start()

    def _on_dup_progress(self, processed: int, total: int) -> None:
        if total > 0:
            self.progress_label.setText(f"Поиск дубликатов: {processed}/{total}")

    def _on_theme_selected(self, mode: ThemeMode) -> None:
        save_mode(mode)
        instance = QApplication.instance()
        if instance is None:
            return
        app = cast(QApplication, instance)
        p = apply_theme(app, mode)
        # Обновляем цвет спиннера (он рисуется вручную, не через QSS)
        self.spinner.set_colors(p.accent, p.border_subtle)

    def _on_dup_done(self, groups: list[DuplicateGroup]) -> None:
        self.spinner.stop()
        self.dup_btn.setEnabled(True)
        wasted = sum(g.wasted for g in groups)
        self.progress_label.setText(
            f"Дубликаты: {len(groups)} групп, можно освободить {format_size(wasted)}"
        )
        for g in groups:
            head = _SizeItem(
                [f"{len(g.paths)} копий, {format_size(g.size)} каждая", format_size(g.size), format_size(g.wasted)],
                g.wasted,
            )
            self.dup_tree.addTopLevelItem(head)
            for p in g.paths:
                child = _SizeItem([p, format_size(g.size), ""], g.size)
                child.setData(0, Qt.ItemDataRole.UserRole, p)
                head.addChild(child)
            head.setExpanded(False)

    # ---------- Удаление ----------

    def _gather_selected_paths(self) -> list[str]:
        # Активная вкладка определяет источник выделения
        widget = self.tabs.currentWidget()
        tree: QTreeWidget | None = None
        if widget is self.files_tree:
            tree = self.files_tree
        elif widget is self.junk_tree:
            tree = self.junk_tree
        elif widget is not None and widget is not self.dirs_tree:
            # дубликаты
            tree = self.dup_tree
        if tree is None:
            return []
        paths: list[str] = []
        for item in tree.selectedItems():
            p = item.data(0, Qt.ItemDataRole.UserRole)
            if isinstance(p, str) and p and os.path.isfile(p):
                paths.append(p)
        # уникализируем, сохраняя порядок
        return list(dict.fromkeys(paths))

    def _delete_selected(self) -> None:
        paths = self._gather_selected_paths()
        if not paths:
            QMessageBox.information(
                self,
                "Нечего удалять",
                "Выдели файлы во вкладках «По типу», «Мусор» или «Дубликаты».",
            )
            return
        dry = self.dry_run_cb.isChecked()
        size_total = sum(os.path.getsize(p) for p in paths if os.path.exists(p))
        title = "План удаления" if dry else "Подтверждение"
        msg = (
            f"Будет {'показано' if dry else 'отправлено в корзину'} файлов: {len(paths)}\n"
            f"Освободится: {format_size(size_total)}\n"
            f"Лог: {LOG_PATH}"
        )
        button = QMessageBox.question(
            self,
            title,
            msg,
            QMessageBox.StandardButton.Ok | QMessageBox.StandardButton.Cancel,
        )
        if button != QMessageBox.StandardButton.Ok:
            return
        results = trash_paths(paths, dry_run=dry, log_path=LOG_PATH)
        ok, failed, freed = summarize(results)
        kind = "DRY RUN" if dry else "Удалено"
        QMessageBox.information(
            self,
            "Готово",
            f"{kind}: успешно {ok}, ошибок {failed}, освобождено {format_size(freed)}.",
        )

    # ---------- Вспомогательное ----------

    def _reset_results(self) -> None:
        self._scan_result = None
        self.files_tree.clear()
        self.dirs_tree.clear()
        self.junk_tree.clear()
        self.dup_tree.clear()
        self.dup_btn.setEnabled(False)


def _format_mtime(mtime: float) -> str:
    import datetime as _dt

    try:
        return _dt.datetime.fromtimestamp(mtime).strftime("%Y-%m-%d")
    except (OSError, ValueError, OverflowError):
        return ""


def _build_dir_items(parent_item: QTreeWidgetItem, parent: DirNode) -> None:
    children = sorted(parent.children, key=lambda n: n.size, reverse=True)
    for c in children[:200]:  # ограничиваем глубину/ширину для производительности
        item = _SizeItem([c.name or c.path, format_size(c.size), str(c.file_count)], c.size)
        item.setData(0, Qt.ItemDataRole.UserRole, c.path)
        parent_item.addChild(item)
        if c.children:
            _build_dir_items(item, c)


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    app.setOrganizationName(ORG_NAME)
    app.setWindowIcon(QIcon(_logo_path()))
    mode = load_saved_mode()
    p = apply_theme(app, mode)
    win = MainWindow()
    win.spinner.set_colors(p.accent, p.border_subtle)
    win.show()
    return int(app.exec())


if __name__ == "__main__":
    raise SystemExit(main())
