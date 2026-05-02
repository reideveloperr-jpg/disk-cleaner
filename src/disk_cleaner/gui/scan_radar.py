"""Радар сканирования: накладной виджет с вращающейся развёрткой и блипами.

Визуально он живёт поверх области вкладок во время скана; при старте плавно
проявляется, при остановке плавно гаснет, освобождая место для списка файлов.

Каждый файл, который попадает в `progress`-сигнал воркера, может быть превращён
в блип (точку) на радаре цветом своей категории. Блипы появляются в случайной
точке внутри окружности и постепенно затухают.
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass, field

from PySide6.QtCore import QPointF, Qt, QTimer
from PySide6.QtGui import (
    QColor,
    QConicalGradient,
    QFont,
    QPainter,
    QPaintEvent,
    QPen,
)
from PySide6.QtWidgets import QWidget

from ..classifier import CATEGORY_LABEL, FileCategory

# Цвета категорий — отличаются достаточно, чтобы их было видно на чёрном фоне.
_BLIP_COLORS: dict[FileCategory, str] = {
    FileCategory.VIDEO: "#E07B7B",      # красный
    FileCategory.PHOTO: "#7BCB7B",      # зелёный
    FileCategory.AUDIO: "#7B9BE0",      # синий
    FileCategory.DOCUMENT: "#E0CC7B",   # жёлтый
    FileCategory.ARCHIVE: "#CC785C",    # claude-orange
    FileCategory.CODE: "#9C7BE0",       # фиолетовый
    FileCategory.INSTALLER: "#7BE0CC",  # бирюзовый
    FileCategory.OTHER: "#9A9A9A",      # серый
}


@dataclass
class _Blip:
    angle: float       # радианы
    dist: float        # 0..1, доля от радиуса
    color: QColor
    age: float = 0.0
    lifetime: float = 1.6  # секунды


@dataclass
class _CategoryStat:
    found: int = 0
    last_blip_age: float = 999.0
    history: list[_Blip] = field(default_factory=list)


class ScanRadar(QWidget):
    """Круговой «радар» с вращающейся развёрткой и блипами по категориям."""

    _SWEEP_DEG_PER_FRAME = 2.0     # ~60° в секунду при 30fps
    _FADE_PER_FRAME = 0.06          # 0..1, скорость fade-in / fade-out
    _MAX_BLIPS = 80
    _ACCENT = "#CC785C"

    def __init__(self, parent: QWidget | None = None, *, fps: int = 30) -> None:
        super().__init__(parent)
        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents, True)
        self._frame_dt = 1.0 / max(1, fps)
        self._sweep_angle = 0.0
        self._opacity = 0.0
        self._target_opacity = 0.0
        self._blips: list[_Blip] = []
        self._stats: dict[FileCategory, _CategoryStat] = {
            cat: _CategoryStat() for cat in FileCategory
        }
        self._files_seen = 0

        self._timer = QTimer(self)
        self._timer.setInterval(max(1, int(1000 / max(1, fps))))
        self._timer.timeout.connect(self._tick)
        self.hide()

    # ------ публичный API ------

    def start(self) -> None:
        """Плавно показать радар."""
        self._target_opacity = 1.0
        # сбрасываем накопленную статистику в начале нового скана
        self._blips.clear()
        for s in self._stats.values():
            s.found = 0
            s.history.clear()
        self._files_seen = 0
        self.show()
        self._timer.start()

    def stop(self) -> None:
        """Плавно погасить радар (виджет скроется в `_tick`, когда opacity==0)."""
        self._target_opacity = 0.0

    def add_blip(self, category: FileCategory) -> None:
        """Добавить блип категории. Никаких эффектов, если радар не виден."""
        if self._opacity <= 0.05 and self._target_opacity <= 0.0:
            return
        self._files_seen += 1
        # Угол и расстояние — рандом, чтобы блипы распределялись по кругу.
        angle = random.uniform(0.0, 2.0 * math.pi)
        dist = random.uniform(0.20, 0.96)
        color = QColor(_BLIP_COLORS.get(category, "#FFFFFF"))
        blip = _Blip(angle=angle, dist=dist, color=color)
        self._blips.append(blip)
        if len(self._blips) > self._MAX_BLIPS:
            self._blips = self._blips[-self._MAX_BLIPS:]
        stat = self._stats[category]
        stat.found += 1
        stat.last_blip_age = 0.0

    # ------ внутренняя логика ------

    def _tick(self) -> None:
        self._sweep_angle = (self._sweep_angle + self._SWEEP_DEG_PER_FRAME) % 360.0
        if self._opacity < self._target_opacity:
            self._opacity = min(self._target_opacity, self._opacity + self._FADE_PER_FRAME)
        elif self._opacity > self._target_opacity:
            self._opacity = max(self._target_opacity, self._opacity - self._FADE_PER_FRAME)
            if self._opacity <= 0.0 and self._target_opacity <= 0.0:
                self._timer.stop()
                self.hide()
                return
        # Старим блипы и статистику
        for b in self._blips:
            b.age += self._frame_dt
        self._blips = [b for b in self._blips if b.age < b.lifetime]
        for s in self._stats.values():
            s.last_blip_age += self._frame_dt
        self.update()

    def paintEvent(self, event: QPaintEvent) -> None:
        del event
        rect = self.rect()
        if rect.isEmpty() or self._opacity <= 0.0:
            return
        painter = QPainter(self)
        try:
            painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
            painter.setOpacity(self._opacity)

            cx = rect.width() / 2.0
            cy = rect.height() / 2.0
            max_r = min(cx, cy) * 0.78

            # Затемняющая «линза» под радаром, чтобы он читался поверх любого фона.
            dim = QColor(0, 0, 0, 180)
            painter.setBrush(dim)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawEllipse(QPointF(cx, cy), max_r * 1.18, max_r * 1.18)

            # Концентрические кольца + перекрестие.
            ring_color = QColor(self._ACCENT)
            ring_color.setAlphaF(0.30)
            painter.setPen(QPen(ring_color, 1))
            painter.setBrush(Qt.BrushStyle.NoBrush)
            for r_frac in (0.25, 0.50, 0.75, 1.00):
                painter.drawEllipse(QPointF(cx, cy), max_r * r_frac, max_r * r_frac)
            painter.drawLine(QPointF(cx - max_r, cy), QPointF(cx + max_r, cy))
            painter.drawLine(QPointF(cx, cy - max_r), QPointF(cx, cy + max_r))

            # Развёртка — конический градиент по углу, оранжевый «след».
            wedge = QConicalGradient(QPointF(cx, cy), -self._sweep_angle)
            head = QColor(self._ACCENT)
            head.setAlphaF(0.55)
            tail_far = QColor(0, 0, 0, 0)
            wedge.setColorAt(0.0, head)
            wedge.setColorAt(0.18, QColor(self._ACCENT))
            wedge.setColorAt(0.18, tail_far)  # резкий обрыв
            wedge.setColorAt(1.0, tail_far)
            painter.setBrush(wedge)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawEllipse(QPointF(cx, cy), max_r, max_r)

            # Яркая линия развёртки.
            sweep_rad = math.radians(self._sweep_angle)
            ex = cx + math.cos(sweep_rad) * max_r
            ey = cy + math.sin(sweep_rad) * max_r
            painter.setPen(QPen(QColor(self._ACCENT), 2))
            painter.drawLine(QPointF(cx, cy), QPointF(ex, ey))

            # Блипы.
            for b in self._blips:
                bx = cx + math.cos(b.angle) * max_r * b.dist
                by = cy + math.sin(b.angle) * max_r * b.dist
                life = max(0.0, 1.0 - b.age / b.lifetime)
                color = QColor(b.color)
                color.setAlphaF(life)
                painter.setBrush(color)
                painter.setPen(Qt.PenStyle.NoPen)
                radius = 3.0 + 4.0 * (1.0 - life)  # растёт по мере затухания
                painter.drawEllipse(QPointF(bx, by), radius, radius)

            # Подпись «SCANNING …» + счётчик.
            painter.setPen(QColor("#F0F0F0"))
            font = QFont(painter.font())
            font.setPointSize(11)
            font.setBold(True)
            font.setLetterSpacing(QFont.SpacingType.AbsoluteSpacing, 2.0)
            painter.setFont(font)
            painter.drawText(
                rect,
                int(Qt.AlignmentFlag.AlignHCenter | Qt.AlignmentFlag.AlignTop),
                f"  SCANNING — {self._files_seen} файлов",
            )

            # Легенда категорий по дну.
            legend_font = QFont(painter.font())
            legend_font.setPointSize(9)
            legend_font.setBold(False)
            legend_font.setLetterSpacing(QFont.SpacingType.AbsoluteSpacing, 0.5)
            painter.setFont(legend_font)
            cats = list(FileCategory)
            slot_w = rect.width() / len(cats)
            for i, cat in enumerate(cats):
                stat = self._stats[cat]
                color = QColor(_BLIP_COLORS[cat])
                # точка
                px = slot_w * (i + 0.5)
                py = float(rect.height()) - 18.0
                painter.setBrush(color)
                painter.setPen(Qt.PenStyle.NoPen)
                painter.drawEllipse(QPointF(px - 28, py + 5), 4.0, 4.0)
                painter.setPen(QColor("#D0D0D0"))
                painter.drawText(
                    QPointF(px - 22, py + 9),
                    f"{CATEGORY_LABEL[cat]} {stat.found}",
                )
        finally:
            painter.end()
