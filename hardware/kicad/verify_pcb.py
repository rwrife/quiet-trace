#!/usr/bin/env python3
"""Cross-check the generated KiCad PCB against the schematic XML netlist.

This is intentionally stdlib-only so CI/reviewers can inspect every pad-to-net
assignment without importing KiCad's version-specific pcbnew Python module.
"""

from __future__ import annotations

import json
import re
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path


def parse_sexp(text: str) -> list:
    token_re = re.compile(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()]+')
    stack: list[list] = []
    roots: list = []
    for match in token_re.finditer(text):
        token = match.group(0)
        if token == "(":
            node: list = []
            if stack:
                stack[-1].append(node)
            else:
                roots.append(node)
            stack.append(node)
        elif token == ")":
            if not stack:
                raise ValueError("unexpected closing parenthesis")
            stack.pop()
        else:
            value = json.loads(token) if token.startswith('"') else token
            if not stack:
                raise ValueError("token outside root expression")
            stack[-1].append(value)
    if stack:
        raise ValueError("unterminated expression")
    if len(roots) != 1:
        raise ValueError(f"expected one root expression, found {len(roots)}")
    return roots[0]


def children(node: list, keyword: str) -> list[list]:
    return [item for item in node if isinstance(item, list) and item and item[0] == keyword]


def child(node: list, keyword: str) -> list | None:
    found = children(node, keyword)
    return found[0] if found else None


def parse_expected(xml_path: Path) -> tuple[dict, dict, int]:
    root = ET.parse(xml_path).getroot()
    components = {}
    for comp in root.findall("./components/comp"):
        ref = comp.attrib["ref"]
        components[ref] = {
            "value": comp.findtext("value", default=""),
            "footprint": comp.findtext("footprint", default=""),
        }

    expected: dict[str, dict[str, str]] = defaultdict(dict)
    pin_count = 0
    for net in root.findall("./nets/net"):
        raw_name = net.attrib["name"]
        if raw_name.startswith("unconnected-"):
            continue
        name = raw_name.removeprefix("/")
        for node in net.findall("node"):
            expected[node.attrib["ref"]][node.attrib["pin"]] = name
            pin_count += 1
    return components, expected, pin_count


def parse_board(pcb_path: Path) -> tuple[list, dict]:
    root = parse_sexp(pcb_path.read_text(encoding="utf-8"))
    if not root or root[0] != "kicad_pcb":
        raise ValueError("not a KiCad PCB")

    footprints = {}
    for fp in children(root, "footprint"):
        ref = None
        value = None
        for prop in children(fp, "property"):
            if len(prop) >= 3 and prop[1] == "Reference":
                ref = prop[2]
            elif len(prop) >= 3 and prop[1] == "Value":
                value = prop[2]
        if not ref:
            continue
        pad_nets: dict[str, set[str]] = defaultdict(set)
        for pad in children(fp, "pad"):
            if len(pad) < 2 or not pad[1]:
                continue
            net = child(pad, "net")
            if net and len(net) >= 3:
                pad_nets[str(pad[1])].add(str(net[2]))
        footprints[ref] = {
            "footprint": str(fp[1]) if len(fp) > 1 else "",
            "value": value or "",
            "pad_nets": pad_nets,
            "node": fp,
        }
    return root, footprints


def normalized_footprint(value: str) -> str:
    return value.split(":", 1)[-1]


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: verify_pcb.py <schematic.xml> <board.kicad_pcb>", file=sys.stderr)
        return 2

    xml_path = Path(sys.argv[1])
    pcb_path = Path(sys.argv[2])
    components, expected, pin_count = parse_expected(xml_path)
    root, footprints = parse_board(pcb_path)
    errors = []

    for ref, comp in sorted(components.items()):
        board_comp = footprints.get(ref)
        if board_comp is None:
            errors.append(f"{ref}: missing footprint")
            continue
        if normalized_footprint(board_comp["footprint"]) != normalized_footprint(comp["footprint"]):
            errors.append(
                f"{ref}: footprint {board_comp['footprint']!r} != schematic {comp['footprint']!r}"
            )
        if board_comp["value"] != comp["value"]:
            errors.append(f"{ref}: value {board_comp['value']!r} != schematic {comp['value']!r}")
        for pin, net in sorted(expected.get(ref, {}).items()):
            board_nets = board_comp["pad_nets"].get(pin)
            if not board_nets:
                errors.append(f"{ref}.{pin}: pad/net missing; expected {net}")
            elif board_nets != {net}:
                errors.append(f"{ref}.{pin}: board nets {sorted(board_nets)} != {net}")

    # Four mounting fixtures are board-only mechanical features.
    for ref in ["H1", "H2", "H3", "H4"]:
        if ref not in footprints:
            errors.append(f"{ref}: mounting hole missing")

    # Manufacturer-backed physical constraints represented in editable KiCad data.
    u1 = footprints.get("U1", {}).get("node", [])
    u1_keepouts = [z for z in children(u1, "zone") if child(z, "keepout")]
    if not u1_keepouts:
        errors.append("U1: embedded antenna keepout/rule area missing")

    u4 = footprints.get("U4", {}).get("node", [])
    acoustic_holes = []
    for pad in children(u4, "pad"):
        if len(pad) >= 4 and pad[1] == "" and pad[2] == "np_thru_hole":
            drill = child(pad, "drill")
            if drill and len(drill) >= 2:
                acoustic_holes.append(float(drill[1]))
    if not acoustic_holes or min(acoustic_holes) < 0.5:
        errors.append("U4: manufacturer-minimum 0.5 mm acoustic NPTH missing")

    board_keepouts = [z for z in children(root, "zone") if child(z, "keepout")]
    mic_large = mic_small = False
    for zone in board_keepouts:
        polygon = child(zone, "polygon")
        serialized = json.dumps(polygon) if polygon else ""
        mic_large |= all(value in serialized for value in ["38.8", "44.01", "41.2", "46.41"])
        mic_small |= all(value in serialized for value in ["39.5", "44.71", "40.5", "45.71"])
    if not (mic_large and mic_small):
        errors.append("microphone acoustic front/back keepouts missing")

    silk_text = {str(item[1]) for item in children(root, "gr_text") if len(item) > 1}
    for required in [
        "QUIET TRACE  REV A",
        "ANTENNA KEEPOUT",
        "MIC PORT - KEEP CLEAR",
        "USB 5V SELV",
        "NOT A CERTIFIED SLM",
    ]:
        if required not in silk_text:
            errors.append(f"silkscreen label missing: {required}")

    if errors:
        print("PCB cross-check failed:")
        for error in errors:
            print(f"- {error}")
        return 1

    print(
        "PCB cross-check passed: "
        f"{len(components)} schematic components, {pin_count} pin/net assignments, "
        f"{len(footprints)} PCB footprints, {len(board_keepouts)} board keepouts, "
        f"{len(u1_keepouts)} antenna keepout, {len(acoustic_holes)} acoustic hole"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
