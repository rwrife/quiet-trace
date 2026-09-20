#!/usr/bin/env python3
"""Vendor the exact baseline symbol definitions used by the schematic.

Why this exists
---------------
Two different KiCad 9.0.9 builds ship *different content* in the same
versioned system libraries. The schematic was authored and ERC-verified
against the Debian-packaged KiCad 9.0.9 baseline (PPA build), whose
``RF_Module:ESP32-S3-WROOM-1`` names module pads 13/14 ``IO19``/``IO20``
and whose ``Device:LED`` carries no ``Sim.Pins`` property. The official
``kicad/kicad:9.0.9`` image ships a library snapshot where those pads are
named ``USB_D-``/``USB_D+`` and ``Device:LED`` gained ``Sim.Pins``, so a
digest-pinned official image emits two ``lib_symbol_mismatch`` ERC
warnings against the cached symbols and the netlist export reports
different pin names than ``verify_netlist.py`` checks.

To make the design hermetic, the exact baseline symbol definitions are
vendored under ``hardware/kicad/libs/symbols/`` and referenced by the
tracked ``hardware/kicad/sym-lib-table``. ERC then compares the cached
symbols against content-pinned files instead of whatever snapshot the
host image happens to ship.

Regenerating (only as a reviewed library-uplift change)::

    python3 vendor_baseline_symbols.py [--symbols-dir /usr/share/kicad/symbols]

The symbol list is derived from the ``lib_id`` properties actually used
in ``quiet-trace.kicad_sch``; adding a component with a new library id
means extending ``NEEDED`` here and re-running the script plus ERC.

Symbol blocks are copied byte-exact from the source library files.
Exit codes: 0 wrote all files, 2 inputs missing / a symbol was not found.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# lib -> symbols, matching the (lib_id "...") properties in quiet-trace.kicad_sch
NEEDED: dict[str, list[str]] = {
    "Connector": ["TestPoint", "USB_C_Receptacle_USB2.0_16P"],
    "Connector_Generic": ["Conn_01x06"],
    "Device": ["C", "D_TVS", "LED", "Polyfuse", "R"],
    "Power_Protection": ["USBLC6-2SC6"],
    "Regulator_Linear": ["AP2112K-3.3"],
    "RF_Module": ["ESP32-S3-WROOM-1"],
    "Sensor_Audio": ["ICS-43434"],
    "Switch": ["SW_Push"],
    "power": ["PWR_FLAG"],
}


def extract_symbol(text: str, name: str) -> str | None:
    """Return the balanced ``(symbol "NAME" ...)`` block byte-exact, or None."""
    needle = f'(symbol "{name}"'
    start = text.find(needle)
    if start < 0:
        return None
    depth = 0
    i = start
    while i < len(text):
        ch = text[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return text[start : i + 1]
        i += 1
    return None


def library_header(text: str) -> str:
    """Return the source file's pre-symbol header (kicad_symbol_lib tokens)."""
    cut = text.find('(symbol "')
    if cut < 0:
        raise ValueError("library file contains no symbols")
    return text[:cut]


def extends_target(block: str) -> str | None:
    m = re.search(r'\(extends "([^"]+)"\)', block)
    return m.group(1) if m else None


def collect_closure(text: str, name: str) -> list[str] | None:
    """Return the ``NAME`` block plus any ``(extends "...")`` ancestor chain
    in dependency order (extend targets first). Unit/alternate blocks are
    nested inside the root block by the .kicad_sym format, so they ride along.
    None if the root symbol is missing."""
    root = extract_symbol(text, name)
    if root is None:
        return None
    chain: list[str] = []
    seen = {name}
    parent = extends_target(root)
    while parent and parent not in seen:
        seen.add(parent)
        pblock = extract_symbol(text, parent)
        if pblock is None:
            raise ValueError(f"{name}: extends missing library symbol {parent!r}")
        chain.append(pblock)
        parent = extends_target(pblock)
    chain.append(root)
    return chain


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--symbols-dir", type=Path, default=Path("/usr/share/kicad/symbols"))
    parser.add_argument("--project-dir", type=Path, default=Path("hardware/kicad"))
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv[1:])

    out_dir = args.project_dir / "libs" / "symbols"
    missing: list[str] = []
    table_lines: list[str] = []

    for lib, symbols in sorted(NEEDED.items()):
        src = args.symbols_dir / f"{lib}.kicad_sym"
        if not src.is_file():
            print(f"missing source library: {src}", file=sys.stderr)
            return 2
        text = src.read_text(encoding="utf-8")
        blocks: list[str] = []
        for sym in symbols:
            closure = collect_closure(text, sym)
            if closure is None:
                missing.append(f"{lib}:{sym}")
                continue
            blocks.extend(closure)
        if missing:
            print(f"symbols not found in baseline libraries: {missing}", file=sys.stderr)
            return 2
        out_dir.mkdir(parents=True, exist_ok=True)
        body = library_header(text) + "\n".join(blocks) + "\n)\n"
        (out_dir / f"{lib}.kicad_sym").write_text(body, encoding="utf-8")
        table_lines.append(
            f'  (lib (name "{lib}")(type "KiCad")'
            f'(uri "${{KIPRJMOD}}/libs/symbols/{lib}.kicad_sym")'
            f'(options "")(descr "vendored baseline library {lib}"))'
        )

    table = "(sym_lib_table\n" + "\n".join(table_lines) + "\n)\n"
    (args.project_dir / "sym-lib-table").write_text(table, encoding="utf-8")

    if not args.quiet:
        total = sum(len(v) for v in NEEDED.values())
        print(f"vendored {total} symbols in {len(NEEDED)} libs under {out_dir}; wrote sym-lib-table")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
