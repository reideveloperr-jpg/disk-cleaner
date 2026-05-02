"""Простой CLI для проверки сканера и поиска дубликатов без GUI."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .classifier import CATEGORY_LABEL, FileCategory
from .duplicates import find_duplicates
from .junk import RULES, find_all
from .scanner import format_size, scan, top_n_largest


def _print_progress(files: int, bytes_: int, current: str) -> None:
    sys.stdout.write(f"\r  scanning… files={files}  size={format_size(bytes_)}  {current[:60]}     ")
    sys.stdout.flush()


def _category_arg(value: str) -> FileCategory:
    try:
        return FileCategory(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"unknown category: {value}") from exc


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="volchay-cleans-cli", description="Volchay Cleans — disk analyzer (CLI mode)"
    )
    parser.add_argument("path", type=Path, help="root directory to scan")
    parser.add_argument("--top", type=int, default=20, help="show top-N largest files")
    parser.add_argument(
        "--category",
        type=_category_arg,
        action="append",
        help="filter by file category (can be repeated): "
        + ", ".join(c.value for c in FileCategory),
    )
    parser.add_argument("--duplicates", action="store_true", help="also search for duplicates")
    parser.add_argument("--junk", action="store_true", help="also detect junk categories")
    args = parser.parse_args(argv)

    print(f"Scanning {args.path}…")
    result = scan(args.path, progress_cb=_print_progress)
    sys.stdout.write("\n")

    print(f"\nTotal: {result.total_files} files, {format_size(result.total_size)}")
    if result.errors:
        print(f"Errors: {len(result.errors)} (first 3 below)")
        for e in result.errors[:3]:
            print(f"  - {e}")

    print("\nBy category:")
    for cat, size in sorted(result.by_category.items(), key=lambda kv: kv[1], reverse=True):
        if size == 0:
            continue
        label = CATEGORY_LABEL[cat]
        print(f"  {label:<14} {format_size(size)}")

    print(
        f"\nTop {args.top} largest files"
        + (f" in {[c.value for c in args.category]}" if args.category else "")
        + ":"
    )
    top = top_n_largest(result.files, n=args.top, categories=args.category)
    for f in top:
        print(f"  [{CATEGORY_LABEL[f.category]:<10}] {format_size(f.size):>10}  {f.path}")

    if args.junk:
        print("\nJunk categories:")
        junk_groups = find_all(result.files)
        rule_by_key = {r.key: r for r in RULES}
        for key, items in junk_groups.items():
            total = sum(i.size for i in items)
            label = rule_by_key[key].label if key in rule_by_key else key
            print(f"  - {label}: {len(items)} files, {format_size(total)}")

    if args.duplicates:
        print("\nSearching duplicates…")
        dup_groups = find_duplicates(result.files)
        wasted = sum(g.wasted for g in dup_groups)
        print(f"  groups: {len(dup_groups)}; reclaimable: {format_size(wasted)}")
        for g in dup_groups[:10]:
            print(f"  - {format_size(g.size)} × {len(g.paths)} (saves {format_size(g.wasted)})")
            for p in g.paths[:5]:
                print(f"      {p}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
