"""Анимированный «RGB-фон» для темы ThemeMode.RGB.

Рисует чёрный фон + несколько ярких радиальных «глоу-блобов»,
которые плавно дрейфуют по экрану по синусоидальным траекториям и
одновременно меняют hue (HSV), создавая эффект бегущей радуги.

UI-виджеты (сайдбар, табы, кнопки) непрозрачны и закрывают глоу
в местах своих фонов — свечение видно в свободных зонах окна.
"""

from __future__ import annotations

import math

from PySide6.QtCore import QPointF, Qt, QTimer
from PySide6.QtGui import QColor, QPainter, QPaintEvent, QRadialGradient
from PySide6.QtWidgets import QWidget


class RgbBackground(QWidget):
    """Виджет-задник с движущимися цветными блобами."""

    # параметры дрейфа: (offset_x_phase, offset_y_phase, hue_offset, speed_mul)
    # Меньше блобов и они меньшего радиуса — свечение точечное, а не «заливает» окно.
    _BLOBS: tuple[tuple[float, float, float, float], ...] = (
        (0.00, 0.00, 0.00, 1.00),
        (1.80, 1.10, 0.50, 0.80),
    )

    def __init__(
        self,
        parent: QWidget | None = None,
        *,
        fps: int = 30,
        speed: float = 0.0035,
    ) -> None:
        super().__init__(parent)
        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents, True)
        self.setAttribute(Qt.WidgetAttribute.WA_OpaquePaintEvent, True)
        # На задний план; центральный виджет рисуется поверх.
        self.lower()
        self._t = 0.0
        self._speed = speed
        self._timer = QTimer(self)
        self._timer.setInterval(max(1, int(1000 / max(1, fps))))
        self._timer.timeout.connect(self._tick)

    # ----- API -----

    def start(self) -> None:
        if not self._timer.isActive():
            self._timer.start()
            self.show()

    def stop(self) -> None:
        if self._timer.isActive():
            self._timer.stop()
        self.hide()

    # ----- внутреннее -----

    def _tick(self) -> None:
        self._t = (self._t + self._speed) % 1.0
        self.update()

    def paintEvent(self, event: QPaintEvent) -> None:
        del event
        rect = self.rect()
        if rect.isEmpty():
            return
        w = float(rect.width())
        h = float(rect.height())
        painter = QPainter(self)
        try:
            painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
            # Базовая чёрная подложка — гарантирует, что просвечивающие зоны
            # не оголят системный фон.
            painter.fillRect(rect, QColor(0, 0, 0))

            # Радиус блоба ≈ 28% от min(width, height) — точечное свечение.
            radius = min(w, h) * 0.28

            for (px, py, hue_off, speed_mul) in self._BLOBS:
                phase_x = (self._t * speed_mul) * 2.0 * math.pi + px
                phase_y = (self._t * speed_mul) * 2.0 * math.pi + py
                # Дрейф в пределах 20..80% ширины/высоты — блобы заметно движутся,
                # но не уходят за края.
                cx = w * (0.5 + 0.30 * math.sin(phase_x * 0.7))
                cy = h * (0.5 + 0.30 * math.cos(phase_y * 0.5))
                hue = (self._t + hue_off) % 1.0
                center_color = QColor.fromHsvF(hue, 1.0, 1.0)
                center_color.setAlphaF(0.60)
                edge_color = QColor(0, 0, 0, 0)
                grad = QRadialGradient(QPointF(cx, cy), radius)
                grad.setColorAt(0.0, center_color)
                grad.setColorAt(1.0, edge_color)
                painter.fillRect(rect, grad)
        finally:
            painter.end()
