#!/usr/bin/env python3
"""Export Quiet Trace's enriched BOM from a KiCad XML netlist.

Generate the XML with:
  kicad-cli sch export netlist --format kicadxml \
    -o quiet-trace.xml quiet-trace.kicad_sch

The XML netlist is used because KiCad preserves arbitrary symbol properties there.
"""

from __future__ import annotations

import argparse
import csv
import re
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path

PRICE_RE = re.compile(r"USD\s+([0-9]+(?:\.[0-9]+)?)\s*@1", re.IGNORECASE)
REQUIRED = ("Manufacturer", "MPN", "Supplier", "Supplier PN", "Availability", "BOM Comments")


def natural_ref(ref: str) -> tuple[str, int]:
    match = re.fullmatch(r"([^0-9]+)([0-9]+)", ref)
    return (match.group(1), int(match.group(2))) if match else (ref, 0)


def text_of(node: ET.Element, tag: str) -> str:
    child = node.find(tag)
    return "" if child is None or child.text is None else child.text.strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("xml_netlist", type=Path)
    parser.add_argument("output_csv", type=Path)
    args = parser.parse_args()

    root = ET.parse(args.xml_netlist).getroot()
    groups: dict[tuple[str, ...], list[dict[str, str]]] = defaultdict(list)
    errors: list[str] = []

    for comp in root.findall("./components/comp"):
        ref = comp.attrib["ref"]
        if ref.startswith("#"):
            continue
        fields = {field.attrib["name"]: (field.text or "").strip() for field in comp.findall("./fields/field")}
        record = {
            "Reference": ref,
            "Value": text_of(comp, "value"),
            "Footprint": text_of(comp, "footprint"),
            "Datasheet": text_of(comp, "datasheet"),
            "Manufacturer": fields.get("Manufacturer", ""),
            "MPN": fields.get("MPN", ""),
            "Supplier": fields.get("Supplier", ""),
            "Supplier PN": fields.get("Supplier PN", ""),
            "Availability": fields.get("Availability", ""),
            "Notes": fields.get("BOM Comments", ""),
            "DNP": "yes" if any(p.attrib.get("name") == "dnp" for p in comp.findall("property")) else "no",
        }
        is_fabricated = record["Manufacturer"] == "PCB feature"
        missing = [name for name in REQUIRED if not fields.get(name)]
        if not record["Datasheet"] or record["Datasheet"] == "~":
            missing.append("Datasheet")
        if missing and not is_fabricated:
            errors.append(f"{ref}: missing {', '.join(sorted(set(missing)))}")
        key = tuple(record[name] for name in (
            "Value", "Footprint", "Manufacturer", "MPN", "Supplier", "Supplier PN",
            "Datasheet", "Availability", "DNP",
        ))
        groups[key].append(record)

    if errors:
        raise SystemExit("BOM validation failed:\n" + "\n".join(errors))

    rows: list[dict[str, str]] = []
    for records in groups.values():
        records.sort(key=lambda r: natural_ref(r["Reference"]))
        first = records[0]
        price = PRICE_RE.search(first["Availability"])
        unit_cost = price.group(1) if price else ("N/A" if first["Manufacturer"] == "PCB feature" else "TBD")
        rows.append({
            "Reference": ",".join(r["Reference"] for r in records),
            "Qty": str(len(records)),
            "Value / Description": first["Value"],
            "Footprint / Package": first["Footprint"],
            "Manufacturer": first["Manufacturer"],
            "MPN": first["MPN"],
            "Supplier / Source": first["Supplier"],
            "Supplier PN": first["Supplier PN"],
            "Estimated Unit Cost USD": unit_cost,
            "Availability / Price Check": first["Availability"],
            "Datasheet": first["Datasheet"],
            "DNP": first["DNP"],
            "Notes": "; ".join(r["Notes"] for r in records),
        })
    rows.sort(key=lambda r: natural_ref(r["Reference"].split(",", 1)[0]))

    args.output_csv.parent.mkdir(parents=True, exist_ok=True)
    columns = list(rows[0]) if rows else []
    with args.output_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

    purchased = [r for r in rows if r["Manufacturer"] != "PCB feature"]
    covered = sum(int(r["Qty"]) for r in purchased if r["MPN"] and r["MPN"] != "N/A")
    total = sum(int(r["Qty"]) for r in purchased)
    print(f"wrote {args.output_csv}: {len(rows)} lines, {total} purchased components, MPN coverage {covered}/{total}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
