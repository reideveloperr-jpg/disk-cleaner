"""Анимированная RGB-полоска для нижней кромки главного окна.

Малозатратный виджет: при включённой анимации просто двигает hue-смещение
и вызывает ``update()`` ~30 раз в секунду. Заливка — горизонтальный
``QLinearGradient`` со stop'ами по всему диапазону HSV.
"""

from __future__ import annotations

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QColor, QLinearGradient, QPainter, QPaintEvent
from PySide6.QtWidgets import QWidget


class RgbBar(QWidget):
    """Тонкая (~4 px) полоска с бегущим радужным градиентом."""

    _STOPS = 18  # количество цветовых остановок градиента — больше = плавнее

    def __init__(
        self,
        parent: QWidget | None = None,
        *,
        height: int = 4,
        fps: int = 30,
        speed: float = 0.012,
    ) -> None:
        super().__init__(parent)
        self.setFixedHeight(height)
        self.setAttribute(Qt.WidgetAttribute.WA_OpaquePaintEvent, True)
        self._offset = 0.0
        self._speed = speed
        self._timer = QTimer(self)
        self._timer.setInterval(max(1, int(1000 / max(1, fps))))
        self._timer.timeout.connect(self._tick)
        self._timer.start()

    # ----- API -----

    def stop(self) -> None:
        if self._timer.isActive():
            self._timer.stop()

    def start(self) -> None:
        if not self._timer.isActive():
            self._timer.start()

    # ----- внутреннее -----

    def _tick(self) -> None:
        self._offset = (self._offset + self._speed) % 1.0
        self.update()

    def paintEvent(self, event: QPaintEvent) -> None:
        del event
        rect = self.rect()
        if rect.isEmpty():
            return
        painter = QPainter(self)
        try:
            painter.setRenderHint(QPainter.RenderHint.Antialiasing, False)
            grad = QLinearGradient(rect.topLeft(), rect.topRight())
            for i in range(self._STOPS + 1):
                pos = i / self._STOPS
                hue = (pos + self._offset) % 1.0
                grad.setColorAt(pos, QColor.fromHsvF(hue, 1.0, 1.0))
            painter.fillRect(rect, grad)
        finally:
            painter.end()
