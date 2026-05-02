"""Крутящийся индикатор занятости (вместо прогресс-бара).

Кастомный виджет, рисующий вращающуюся дугу акцентным цветом темы.
Без внешних ассетов и анимированных GIF — просто `QPainter` + `QTimer`.
"""

from __future__ import annotations

from PySide6.QtCore import QRectF, Qt, QTimer
from PySide6.QtGui import QColor, QPainter, QPaintEvent, QPen
from PySide6.QtWidgets import QWidget


class BusySpinner(QWidget):
    """Маленький круговой индикатор занятости.

    - Дуга длиной ~270° акцентного цвета крутится по часовой стрелке.
    - Под ней — кольцо приглушённого цвета (фон).
    - Старт/стоп управляются ``start()`` / ``stop()``.
    - Цвета подхватываются из текущего ``palette()`` виджета и могут быть
      переопределены через ``set_colors``.
    """

    def __init__(
        self,
        parent: QWidget | None = None,
        *,
        diameter: int = 22,
        thickness: int = 3,
        period_ms: int = 1000,
    ) -> None:
        super().__init__(parent)
        self._diameter = diameter
        self._thickness = thickness
        # 360° за period_ms; кадр каждые ~30 мс
        self._frame_ms = 30
        self._step_deg = 360.0 * self._frame_ms / max(period_ms, 1)
        self._angle = 0.0
        self._arc_span_deg = 270.0
        self._accent = QColor("#CC785C")
        self._track = QColor(0, 0, 0, 40)

        self._timer = QTimer(self)
        self._timer.setInterval(self._frame_ms)
        self._timer.timeout.connect(self._on_tick)

        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setFixedSize(diameter + 2, diameter + 2)
        self.setVisible(False)

    # ------------- Public API -------------

    def set_colors(self, accent: QColor | str, track: QColor | str | None = None) -> None:
        self._accent = QColor(accent)
        if track is not None:
            self._track = QColor(track)
        self.update()

    def start(self) -> None:
        if not self._timer.isActive():
            self._timer.start()
        self.setVisible(True)
        self.update()

    def stop(self) -> None:
        self._timer.stop()
        self.setVisible(False)

    def isRunning(self) -> bool:
        return self._timer.isActive()

    # ------------- Painting -------------

    def _on_tick(self) -> None:
        self._angle = (self._angle + self._step_deg) % 360.0
        self.update()

    def paintEvent(self, event: QPaintEvent) -> None:
        del event
        side = min(self.width(), self.height())
        margin = self._thickness + 1
        rect = QRectF(
            (self.width() - side) / 2 + margin / 2,
            (self.height() - side) / 2 + margin / 2,
            side - margin,
            side - margin,
        )

        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        # Кольцо-трек
        track_pen = QPen(self._track)
        track_pen.setWidth(self._thickness)
        track_pen.setCapStyle(Qt.PenCapStyle.FlatCap)
        painter.setPen(track_pen)
        painter.drawEllipse(rect)

        # Дуга акцента
        arc_pen = QPen(self._accent)
        arc_pen.setWidth(self._thickness)
        arc_pen.setCapStyle(Qt.PenCapStyle.RoundCap)
        painter.setPen(arc_pen)
        # QPainter.drawArc принимает 1/16 градуса; 0° — 3 часа, против часовой
        start_angle_16 = int(-self._angle * 16)
        span_16 = int(-self._arc_span_deg * 16)
        painter.drawArc(rect, start_angle_16, span_16)
        painter.end()
