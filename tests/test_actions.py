from __future__ import annotations

import json
from pathlib import Path

from disk_cleaner.actions import summarize, trash_paths


def test_dry_run_does_not_delete(tmp_path: Path) -> None:
    f = tmp_path / "file.bin"
    f.write_bytes(b"hello")
    log = tmp_path / "log.jsonl"

    results = trash_paths([str(f)], dry_run=True, log_path=log)
    assert f.exists()
    assert len(results) == 1
    assert results[0].dry_run is True
    assert results[0].ok is True
    assert results[0].size == 5

    # лог содержит запись
    lines = log.read_text(encoding="utf-8").splitlines()
    assert len(lines) == 1
    rec = json.loads(lines[0])
    assert rec["dry_run"] is True
    assert rec["path"] == str(f)


def test_summarize() -> None:
    from disk_cleaner.actions import ActionResult

    rs = [
        ActionResult(path="a", size=100, ok=True, dry_run=False),
        ActionResult(path="b", size=50, ok=True, dry_run=False),
        ActionResult(path="c", size=10, ok=False, dry_run=False, error="boom"),
    ]
    ok, failed, freed = summarize(rs)
    assert ok == 2
    assert failed == 1
    assert freed == 150
