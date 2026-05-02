"""Доступ к ресурсам пакета (логотип и т.п.).

Модуль намеренно НЕ импортирует PySide6 — иначе тесты, которые проверяют только
доступность ресурса, ломались бы в headless CI без libEGL.

Ресурсы PyInstaller / zip-import могут храниться в виртуальных файловых системах;
``importlib.resources.as_file`` экстрагирует их во временный файл и удаляет при
выходе из контекста. Держим ``ExitStack`` живым на время всей жизни процесса,
чтобы путь, который мы отдаём наружу (``QIcon`` и пр.), оставался валидным.
"""

from __future__ import annotations

from contextlib import ExitStack
from importlib import resources

_RESOURCE_STACK = ExitStack()
_LOGO_PATH = str(
    _RESOURCE_STACK.enter_context(
        resources.as_file(resources.files("disk_cleaner.assets") / "volchay_logo.svg")
    )
)


def logo_path() -> str:
    """Абсолютный путь к SVG-логотипу (валиден всю жизнь процесса)."""
    return _LOGO_PATH
