#!/usr/bin/env python3
"""Generate project-local KiCad library tables pointing at system libraries.

Headless ``kicad-cli`` does not receive the ``${KICAD*_DIR}`` environment
variables that the KiCad GUI defines, so the packaged template tables (which
rely on them) cannot resolve libraries inside CI containers. This script writes
``sym-lib-table`` and ``fp-lib-table`` into the project directory using
absolute URIs to the system-installed symbol/footprint libraries; KiCad loads
project-local tables deterministically with no environment setup.

Usage (defaults match Debian-packaged KiCad layouts):
    python3 make_system_lib_tables.py [--symbols-dir DIR]
        [--footprints-dir DIR] [--project-dir DIR] [--quiet]

Exit codes: 0 wrote both tables, 2 inputs missing.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--symbols-dir", type=Path, default=Path("/usr/share/kicad/symbols"))
    parser.add_argument("--footprints-dir", type=Path, default=Path("/usr/share/kicad/footprints"))
    parser.add_argument("--project-dir", type=Path, default=Path("hardware/kicad"))
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv[1:])

    if not args.symbols_dir.is_dir() or not args.footprints_dir.is_dir():
        print(
            f"missing library dirs: {args.symbols_dir} / {args.footprints_dir}",
            file=sys.stderr,
        )
        return 2

    args.project_dir.mkdir(parents=True, exist_ok=True)
    syms = sorted(args.symbols_dir.glob("*.kicad_sym"))
    fps = sorted(d for d in args.footprints_dir.iterdir() if d.is_dir() and d.name.endswith(".pretty"))
    if not syms or not fps:
        print("no system libraries found", file=sys.stderr)
        return 2

    sym_lines = "".join(
        f'  (lib (name "{p.stem}")(type "KiCad")(uri "{p}")(options "")'
        f'(descr "system symbol library {p.stem}"))\n'
        for p in syms
    )
    (args.project_dir / "sym-lib-table").write_text(f"(sym_lib_table\n{sym_lines})\n", encoding="utf-8")

    fp_lines = "".join(
        f'  (lib (name {p.stem})(type KiCad)(uri {p})(options "")'
        f'(descr "system footprint library {p.stem}"))\n'
        for p in fps
    )
    (args.project_dir / "fp-lib-table").write_text(f"(fp_lib_table\n{fp_lines})\n", encoding="utf-8")

    if not args.quiet:
        print(f"wrote {args.project_dir}/sym-lib-table ({len(syms)} libs) and fp-lib-table ({len(fps)} libs)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
