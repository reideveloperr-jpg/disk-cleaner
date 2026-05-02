from __future__ import annotations

import os
from pathlib import Path

from disk_cleaner.classifier import FileCategory
from disk_cleaner.scanner import format_size, scan, top_n_largest


def _write(p: Path, content: bytes) -> None:
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(content)


def test_scan_basic(tmp_path: Path) -> None:
    _write(tmp_path / "a.mp4", b"x" * 5000)
    _write(tmp_path / "b.jpg", b"x" * 3000)
    _write(tmp_path / "sub" / "c.txt", b"x" * 1000)

    result = scan(tmp_path)
    assert result.total_files == 3
    assert result.total_size == 9000
    assert not result.cancelled
    assert result.by_category[FileCategory.VIDEO] == 5000
    assert result.by_category[FileCategory.PHOTO] == 3000
    assert result.by_category[FileCategory.DOCUMENT] == 1000
    # дерево
    assert result.root.size == 9000
    assert any(c.name == "sub" for c in result.root.children)


def test_top_n_filter_by_category(tmp_path: Path) -> None:
    _write(tmp_path / "v1.mp4", b"x" * 200)
    _write(tmp_path / "v2.mkv", b"x" * 500)
    _write(tmp_path / "p.jpg", b"x" * 1000)

    result = scan(tmp_path)
    only_video = top_n_largest(result.files, n=10, categories=[FileCategory.VIDEO])
    assert {os.path.basename(f.path) for f in only_video} == {"v1.mp4", "v2.mkv"}
    # сортировка по убыванию
    assert only_video[0].size >= only_video[-1].size


def test_hardlink_dedup(tmp_path: Path) -> None:
    a = tmp_path / "a.bin"
    a.write_bytes(b"x" * 4096)
    b = tmp_path / "b.bin"
    try:
        os.link(a, b)
    except OSError:
        return  # ФС не поддерживает hard-links — тест неприменим
    result = scan(tmp_path)
    # учли только один файл, не оба
    assert result.total_files == 1
    assert result.total_size == 4096


def test_format_size() -> None:
    assert format_size(0) == "0 B"
    assert format_size(1024) == "1.0 KB"
    assert format_size(1024 * 1024) == "1.0 MB"
    assert format_size(1.5 * 1024**3) == "1.5 GB"


def test_scan_nonexistent_path(tmp_path: Path) -> None:
    result = scan(tmp_path / "no-such-dir")
    assert result.total_files == 0
    assert result.errors  # есть запись об ошибке


def test_dirnode_file_count_aggregates(tmp_path: Path) -> None:
    """Поле file_count должно подниматься вверх по дереву, как и size."""
    _write(tmp_path / "a.txt", b"x" * 100)
    _write(tmp_path / "sub" / "b.txt", b"x" * 200)
    _write(tmp_path / "sub" / "c.txt", b"x" * 300)
    _write(tmp_path / "sub" / "deeper" / "d.txt", b"x" * 400)

    result = scan(tmp_path)
    assert result.total_files == 4
    # корень: 1 файл прямо + 3 в поддереве = 4
    assert result.root.file_count == 4
    assert result.root.size == 1000
    # sub: 2 файла прямо + 1 в deeper = 3
    sub = next(c for c in result.root.children if c.name == "sub")
    assert sub.file_count == 3
    assert sub.size == 900
    # deeper: 1 файл
    deeper = next(c for c in sub.children if c.name == "deeper")
    assert deeper.file_count == 1
    assert deeper.size == 400
