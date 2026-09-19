#!/usr/bin/env python3
"""Compare a ``kicad-cli pcb drc`` text report against the approved exception baseline.

Revision A carries a documented, deliberately unresolved set of DRC exceptions
while the USB-C fanout region awaits the interactive layout pass described in
``docs/reports/issue-6-drc-geometry-analysis-2026-09-12.md``. This gate keeps
that state honest: any *new*, *removed*, or *moved* violation changes the
report signature and fails the check, so regressions cannot hide behind the
accepted baseline.

Signature of one violation: the bracketed rule id plus the sorted list of its
``@(...):`` item lines. The report timestamp is ignored; violation identities
and geometry are not.

Usage:
    python3 verify_drc_baseline.py <drc-report.rpt> <exceptions.json>

Exit codes:
    0  report signature equals the baseline
    1  signature differs (regression or baseline drift)
    2  bad usage / unreadable inputs
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


def key(entry: dict) -> tuple:
    return (entry["rule"], tuple(entry["items"]))


def signature_from_report(text: str) -> list[dict]:
    """Extract [{rule, items}] entries from a kicad-cli DRC report."""
    entries: list[dict] = []
    current: dict | None = None
    for line in text.splitlines():
        stripped = line.strip()
        if line.startswith("[") and "]: " in line:
            rule = line[1 : line.index("]")]
            current = {"rule": rule, "items": []}
            entries.append(current)
        elif stripped.startswith("@(") and current is not None:
            current["items"].append(stripped)
        elif stripped == "" or stripped.startswith("**"):
            current = None
    for entry in entries:
        entry["items"].sort()
    entries.sort(key=key)
    return entries


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(__doc__)
        return 2
    try:
        report_text = Path(argv[1]).read_text(encoding="utf-8")
        baseline = json.loads(Path(argv[2]).read_text(encoding="utf-8"))
    except OSError as error:
        print(f"cannot read inputs: {error}", file=sys.stderr)
        return 2

    expected = sorted(
        ({"rule": e["rule"], "items": sorted(e["items"])} for e in baseline["exceptions"]),
        key=key,
    )
    actual = signature_from_report(report_text)

    if actual == expected:
        print(
            f"DRC baseline gate passed: {len(actual)} documented exception(s), "
            "no new, removed, or moved violations"
        )
        return 0

    print("DRC baseline gate FAILED: report differs from approved baseline", file=sys.stderr)
    print("\n--- expected (approved baseline) ---", file=sys.stderr)
    for e in expected:
        print(f"[{e['rule']}] {' | '.join(e['items'])}", file=sys.stderr)
    print("\n--- actual (current report) ---", file=sys.stderr)
    for e in actual:
        print(f"[{e['rule']}] {' | '.join(e['items'])}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
