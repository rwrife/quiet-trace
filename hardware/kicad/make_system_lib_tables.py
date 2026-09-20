#!/usr/bin/env python3
"""Generate a project-local footprint library table pointing at system libraries.

Headless ``kicad-cli`` does not receive the ``${KICAD*_DIR}`` environment
variables that the KiCad GUI defines, so the packaged template tables (which
rely on them) cannot resolve libraries inside CI containers. This script writes
``fp-lib-table`` into the project directory using absolute URIs to the
system-installed footprint libraries; KiCad loads project-local tables
deterministically with no environment setup.

Symbol resolution is *not* generated here: the schematic's symbol content is
pinned in-repo via ``vendor_baseline_symbols.py`` (tracked
``libs/symbols/*.kicad_sym`` + tracked ``sym-lib-table``), because different
distributions of the same KiCad version ship different system symbol content
(see ``vendor_baseline_symbols.py`` docstring). The default mode therefore
refuses to touch a tracked ``sym-lib-table``; pass ``--symbols-dir`` explicitly
only if you really want a generated system symbol table (this overwrites the
tracked file and is a reviewed library-uplift action, never a CI step).

Usage (defaults match Debian-packaged KiCad layouts):
    python3 make_system_lib_tables.py [--footprints-dir DIR]
        [--project-dir DIR] [--quiet] [--with-symbols [--symbols-dir DIR]]

Exit codes: 0 wrote requested tables, 2 inputs missing.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--symbols-dir", type=Path, default=None)
    parser.add_argument("--footprints-dir", type=Path, default=Path("/usr/share/kicad/footprints"))
    parser.add_argument("--project-dir", type=Path, default=Path("hardware/kicad"))
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv[1:])

    want_symbols = args.symbols_dir is not None
    if want_symbols and not args.symbols_dir.is_dir():
        print(f"missing symbol library dir: {args.symbols_dir}", file=sys.stderr)
        return 2
    if not args.footprints_dir.is_dir():
        print(f"missing footprint library dir: {args.footprints_dir}", file=sys.stderr)
        return 2

    args.project_dir.mkdir(parents=True, exist_ok=True)
    fps = sorted(d for d in args.footprints_dir.iterdir() if d.is_dir() and d.name.endswith(".pretty"))
    if not fps:
        print("no system footprint libraries found", file=sys.stderr)
        return 2

    if want_symbols:
        syms = sorted(args.symbols_dir.glob("*.kicad_sym"))
        if not syms:
            print("no system symbol libraries found", file=sys.stderr)
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
        msg = f"wrote {args.project_dir}/fp-lib-table ({len(fps)} libs)"
        if want_symbols:
            msg += " (+ generated system sym-lib-table — library-uplift mode)"
        print(msg)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
