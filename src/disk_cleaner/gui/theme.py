"""Темы оформления: Dark (Claude-inspired) и Light, с акцентом Claude orange.

Дизайн отталкивается от современных «Luna-подобных» интерфейсов: скруглённые
карточки, мягкие границы, увеличенные отступы, минимальный хром.
Акцент `#CC785C` (Claude orange) используется одинаково в обеих темах для
выделения активных элементов.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import cast

from PySide6.QtCore import QSettings
from PySide6.QtGui import QPalette
from PySide6.QtWidgets import QApplication


class ThemeMode(str, Enum):
    DARK = "dark"
    LIGHT = "light"
    SYSTEM = "system"


@dataclass(frozen=True)
class Palette:
    name: str
    bg: str
    surface: str
    surface_elev: str
    border_subtle: str
    border_strong: str
    text_primary: str
    text_secondary: str
    text_disabled: str
    accent: str
    accent_hover: str
    accent_pressed: str
    accent_on: str
    danger: str
    selection_bg: str  # фон выделения в списках
    alt_row: str       # alt row in tree views


# Claude orange — единый акцент для обеих тем
_CLAUDE_ORANGE = "#CC785C"
_CLAUDE_ORANGE_HOVER_DARK = "#D88B71"
_CLAUDE_ORANGE_HOVER_LIGHT = "#B36A50"


DARK = Palette(
    name="dark",
    bg="#161616",
    surface="#1F1F1F",
    surface_elev="#2A2A2A",
    border_subtle="#2E2E2E",
    border_strong="#3A3A3A",
    text_primary="#ECECEC",
    text_secondary="#9E9E9E",
    text_disabled="#5C5C5C",
    accent=_CLAUDE_ORANGE,
    accent_hover=_CLAUDE_ORANGE_HOVER_DARK,
    accent_pressed="#B36A50",
    accent_on="#FFFFFF",
    danger="#E07B7B",
    selection_bg="#3A2820",
    alt_row="#1B1B1B",
)


LIGHT = Palette(
    name="light",
    bg="#FAFAF7",
    surface="#FFFFFF",
    surface_elev="#F3F3EE",
    border_subtle="#E8E8E2",
    border_strong="#D4D4CE",
    text_primary="#1F1F1F",
    text_secondary="#5F5F5F",
    text_disabled="#9E9E9E",
    accent=_CLAUDE_ORANGE,
    accent_hover=_CLAUDE_ORANGE_HOVER_LIGHT,
    accent_pressed="#9C5D45",
    accent_on="#FFFFFF",
    danger="#C24A4A",
    selection_bg="#FBE9E0",
    alt_row="#F6F6F1",
)


def palette_for(mode: ThemeMode) -> Palette:
    if mode is ThemeMode.LIGHT:
        return LIGHT
    if mode is ThemeMode.DARK:
        return DARK
    # SYSTEM — определяем по текущему QPalette приложения
    try:
        instance = QApplication.instance()
        if instance is not None:
            app = cast(QApplication, instance)
            pal = app.palette()
            window = pal.color(QPalette.ColorRole.Window)
            # luminance Y = 0.2126 R + 0.7152 G + 0.0722 B
            lum = 0.2126 * window.redF() + 0.7152 * window.greenF() + 0.0722 * window.blueF()
            return DARK if lum < 0.5 else LIGHT
    except Exception:
        pass
    return DARK


def build_qss(p: Palette) -> str:
    """Возвращает QSS для приложения на основе палитры ``p``."""
    # Размеры, общие для тем
    radius = 10
    btn_radius = 8
    chip_radius = 14

    return f"""
QWidget {{
    background-color: {p.bg};
    color: {p.text_primary};
    font-size: 13px;
}}

QMainWindow, QDialog {{
    background-color: {p.bg};
}}

QFrame, QWidget#central {{
    background-color: {p.bg};
}}

QLabel {{
    background: transparent;
    color: {p.text_primary};
}}

QLabel#secondary {{
    color: {p.text_secondary};
}}

/* ---------- Inputs ---------- */
QLineEdit {{
    background-color: {p.surface};
    border: 1px solid {p.border_subtle};
    border-radius: {btn_radius}px;
    padding: 7px 11px;
    selection-background-color: {p.accent};
    selection-color: {p.accent_on};
}}
QLineEdit:focus {{
    border: 1px solid {p.accent};
}}
QLineEdit:disabled {{
    color: {p.text_disabled};
    background-color: {p.surface};
}}

/* ---------- Buttons ---------- */
QPushButton {{
    background-color: {p.surface};
    color: {p.text_primary};
    border: 1px solid {p.border_subtle};
    border-radius: {btn_radius}px;
    padding: 7px 14px;
    font-weight: 500;
}}
QPushButton:hover {{
    background-color: {p.surface_elev};
    border-color: {p.border_strong};
}}
QPushButton:pressed {{
    background-color: {p.surface_elev};
}}
QPushButton:disabled {{
    color: {p.text_disabled};
    background-color: {p.surface};
    border-color: {p.border_subtle};
}}
QPushButton:default {{
    background-color: {p.accent};
    color: {p.accent_on};
    border: 1px solid {p.accent};
}}
QPushButton:default:hover {{
    background-color: {p.accent_hover};
    border-color: {p.accent_hover};
}}
QPushButton:default:pressed {{
    background-color: {p.accent_pressed};
    border-color: {p.accent_pressed};
}}

QPushButton[role="danger"] {{
    color: {p.danger};
    border-color: {p.border_strong};
}}
QPushButton[role="danger"]:hover {{
    background-color: {p.surface_elev};
}}

/* ---------- Chips (категории файлов) ---------- */
QCheckBox[role="chip"] {{
    background-color: {p.surface};
    color: {p.text_secondary};
    border: 1px solid {p.border_subtle};
    border-radius: {chip_radius}px;
    padding: 4px 12px;
    spacing: 0px;
}}
QCheckBox[role="chip"]::indicator {{
    width: 0px;
    height: 0px;
    margin: 0px;
}}
QCheckBox[role="chip"]:hover {{
    border-color: {p.border_strong};
    color: {p.text_primary};
}}
QCheckBox[role="chip"]:checked {{
    background-color: {p.selection_bg};
    color: {p.accent};
    border: 1px solid {p.accent};
}}

/* ---------- Дефолтные QCheckBox (например, dry-run) ---------- */
QCheckBox {{
    color: {p.text_primary};
    spacing: 8px;
    background: transparent;
}}
QCheckBox::indicator {{
    width: 16px;
    height: 16px;
    border-radius: 4px;
    border: 1px solid {p.border_strong};
    background-color: {p.surface};
}}
QCheckBox::indicator:checked {{
    background-color: {p.accent};
    border-color: {p.accent};
    image: none;
}}

/* ---------- Tabs ---------- */
QTabWidget::pane {{
    background-color: {p.surface};
    border: 1px solid {p.border_subtle};
    border-radius: {radius}px;
    top: -1px;
}}
QTabBar::tab {{
    background: transparent;
    color: {p.text_secondary};
    padding: 7px 14px;
    margin-right: 4px;
    border: 1px solid transparent;
    border-radius: {btn_radius}px;
    font-weight: 500;
}}
QTabBar::tab:hover {{
    color: {p.text_primary};
    background-color: {p.surface_elev};
}}
QTabBar::tab:selected {{
    color: {p.accent};
    background-color: {p.selection_bg};
    border-color: {p.accent};
}}

/* ---------- Tree / lists ---------- */
QTreeWidget, QTreeView, QListView {{
    background-color: {p.surface};
    alternate-background-color: {p.alt_row};
    color: {p.text_primary};
    border: 1px solid {p.border_subtle};
    border-radius: {radius}px;
    outline: 0;
    selection-background-color: {p.selection_bg};
    selection-color: {p.text_primary};
}}
QTreeView::item, QTreeWidget::item {{
    padding: 4px 6px;
    border: 0;
}}
QTreeView::item:hover, QTreeWidget::item:hover {{
    background-color: {p.surface_elev};
}}
QTreeView::item:selected, QTreeWidget::item:selected {{
    background-color: {p.selection_bg};
    color: {p.text_primary};
}}
QHeaderView::section {{
    background-color: {p.surface};
    color: {p.text_secondary};
    border: 0;
    border-bottom: 1px solid {p.border_subtle};
    padding: 6px 8px;
    font-weight: 600;
}}
QHeaderView {{
    background-color: {p.surface};
}}

/* ---------- ProgressBar ---------- */
QProgressBar {{
    background-color: {p.surface};
    border: 1px solid {p.border_subtle};
    border-radius: 6px;
    height: 8px;
    text-align: center;
    color: {p.text_secondary};
}}
QProgressBar::chunk {{
    background-color: {p.accent};
    border-radius: 6px;
}}

/* ---------- StatusBar / Menu ---------- */
QStatusBar {{
    background-color: {p.bg};
    color: {p.text_secondary};
    border-top: 1px solid {p.border_subtle};
}}
QMenuBar {{
    background-color: {p.bg};
    color: {p.text_primary};
    border-bottom: 1px solid {p.border_subtle};
}}
QMenuBar::item:selected {{
    background-color: {p.surface_elev};
}}
QMenu {{
    background-color: {p.surface};
    color: {p.text_primary};
    border: 1px solid {p.border_subtle};
    border-radius: 8px;
    padding: 4px;
}}
QMenu::item {{
    padding: 6px 24px 6px 12px;
    border-radius: 4px;
}}
QMenu::item:selected {{
    background-color: {p.selection_bg};
    color: {p.text_primary};
}}

/* ---------- Scrollbars ---------- */
QScrollBar:vertical {{
    background: transparent;
    width: 10px;
    margin: 0;
}}
QScrollBar::handle:vertical {{
    background: {p.border_strong};
    border-radius: 5px;
    min-height: 24px;
}}
QScrollBar::handle:vertical:hover {{
    background: {p.text_disabled};
}}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
    height: 0;
}}
QScrollBar:horizontal {{
    background: transparent;
    height: 10px;
    margin: 0;
}}
QScrollBar::handle:horizontal {{
    background: {p.border_strong};
    border-radius: 5px;
    min-width: 24px;
}}
QScrollBar::handle:horizontal:hover {{
    background: {p.text_disabled};
}}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
    width: 0;
}}

/* ---------- Tooltips ---------- */
QToolTip {{
    background-color: {p.surface_elev};
    color: {p.text_primary};
    border: 1px solid {p.border_subtle};
    border-radius: 6px;
    padding: 4px 8px;
}}

/* ---------- MessageBox / Dialogs ---------- */
QMessageBox {{
    background-color: {p.surface};
}}
"""


_SETTINGS_KEY = "ui/theme"


def load_saved_mode() -> ThemeMode:
    s = QSettings("disk-cleaner", "disk-cleaner")
    raw = s.value(_SETTINGS_KEY, ThemeMode.DARK.value)
    try:
        return ThemeMode(raw)
    except ValueError:
        return ThemeMode.DARK


def save_mode(mode: ThemeMode) -> None:
    s = QSettings("disk-cleaner", "disk-cleaner")
    s.setValue(_SETTINGS_KEY, mode.value)


def apply_theme(app: QApplication, mode: ThemeMode) -> Palette:
    """Применяет тему к приложению. Возвращает выбранную палитру."""
    p = palette_for(mode)
    app.setStyleSheet(build_qss(p))
    return p
