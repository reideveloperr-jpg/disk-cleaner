"""Тесты упаковки и доступа к ресурсам пакета."""

from __future__ import annotations

import os


def test_logo_path_returns_existing_file() -> None:
    """`_logo_path()` должен отдавать живой путь, не удалённый context manager-ом.

    Регрессия на случай, если кто-то вернёт строку из `with as_file(...) as p`,
    из-за чего временный файл будет удалён сразу после возврата.
    """
    from disk_cleaner.gui.main import _logo_path

    path = _logo_path()
    assert path, "_logo_path() returned empty string"
    assert os.path.isfile(path), f"logo file missing at {path}"

    # Повторный вызов должен возвращать тот же путь и файл — то есть мы не
    # пересоздаём временный файл на каждый вызов и он не пропадает.
    path_again = _logo_path()
    assert path_again == path
    assert os.path.isfile(path_again)


def test_logo_is_svg() -> None:
    """Логотип — корректный SVG (хотя бы по сигнатуре)."""
    from disk_cleaner.gui.main import _logo_path

    with open(_logo_path(), encoding="utf-8") as f:
        head = f.read(256)
    assert "<svg" in head
