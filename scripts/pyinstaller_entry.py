"""Точка входа для PyInstaller-сборки Volchay Cleans.

Используется как первый аргумент `pyinstaller`, чтобы GUI-приложение
запускалось как обычный процесс без зависимости от console_script-а
из pyproject.toml.
"""

from __future__ import annotations

import sys

from disk_cleaner.gui.main import main

if __name__ == "__main__":
    sys.exit(main())
