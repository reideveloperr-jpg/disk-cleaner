from __future__ import annotations

from pathlib import Path

from disk_cleaner.classifier import FileCategory
from disk_cleaner.duplicates import find_duplicates
from disk_cleaner.scanner import FileInfo


def _make(path: Path, content: bytes) -> FileInfo:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return FileInfo(path=str(path), size=len(content), mtime=0.0, category=FileCategory.OTHER)


def test_find_duplicates(tmp_path: Path) -> None:
    a = _make(tmp_path / "a.bin", b"hello-world" * 1000)
    b = _make(tmp_path / "b.bin", b"hello-world" * 1000)
    _make(tmp_path / "c.bin", b"different-content" * 1000)
    _make(tmp_path / "d.bin", b"unique" * 1000)

    groups = find_duplicates([a, b, _make(tmp_path / "c.bin", b"different-content" * 1000)], min_size=0)
    # одна группа из {a, b}
    assert len(groups) == 1
    g = groups[0]
    assert sorted(g.paths) == sorted([str(tmp_path / "a.bin"), str(tmp_path / "b.bin")])
    assert g.wasted == g.size  # одна копия экономится


def test_duplicates_min_size_skips_small(tmp_path: Path) -> None:
    a = _make(tmp_path / "a.bin", b"hi")
    b = _make(tmp_path / "b.bin", b"hi")
    groups = find_duplicates([a, b], min_size=10)
    assert groups == []
