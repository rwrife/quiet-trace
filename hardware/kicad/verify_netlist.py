#!/usr/bin/env python3
"""Verify Quiet Trace's critical KiCad XML netlist invariants.

Generate the input with KiCad before running this check:

    kicad-cli sch export netlist --format kicadxml \
      -o quiet-trace.xml quiet-trace.kicad_sch

This check intentionally validates the generated netlist, not the hand-authored
schematic text. It catches changed symbol pin numbers, changed footprint
assignments, broken protection paths, and missing BOM source properties.
Manufacturer datasheets remain the authority for the expected values encoded
below; see hardware/parts-selection.md for citations and package notes.
"""

from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


EXPECTED_COMPONENTS = {
    "J1": (
        "GCT",
        "USB4105-GF-A",
        "Connector_USB:USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal",
    ),
    "F1": ("BHFUSE", "BSMD1206-050-16V", "Fuse:Fuse_1206_3216Metric"),
    "D2": ("Nexperia", "PESD5V0S1BA,115", "Diode_SMD:D_SOD-323"),
    "U1": (
        "Espressif Systems",
        "ESP32-S3-WROOM-1-N8R8",
        "RF_Module:ESP32-S3-WROOM-1",
    ),
    "U2": (
        "Diodes Incorporated",
        "AP2112K-3.3TRG1",
        "Package_TO_SOT_SMD:SOT-23-5",
    ),
    "U3": (
        "STMicroelectronics",
        "USBLC6-2SC6",
        "Package_TO_SOT_SMD:SOT-23-6",
    ),
    "U4": (
        "TDK InvenSense",
        "ICS-43434",
        "Sensor_Audio:InvenSense_ICS-43434-6_3.5x2.65mm",
    ),
    "SW1": ("C&K", "KMR221GLFS", "Button_Switch_SMD:SW_Push_1P1T_NO_CK_KMR2"),
    "SW2": ("C&K", "KMR221GLFS", "Button_Switch_SMD:SW_Push_1P1T_NO_CK_KMR2"),
    "D1": ("Lite-On", "LTST-C170KGKT", "LED_SMD:LED_0805_2012Metric"),
    "J2": (
        "Harwin",
        "M20-9990645",
        "Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical",
    ),
}

# Exact (reference, symbol-pin/footprint-pad) membership for named interfaces.
EXPECTED_NETS = {
    "+3V3": {
        ("C2", "1"), ("C3", "1"), ("C4", "1"), ("C5", "1"),
        ("J2", "1"), ("R5", "1"), ("R6", "1"), ("TP2", "1"),
        ("U1", "2"), ("U2", "5"), ("U4", "5"),
    },
    "BOOT0": {("J2", "6"), ("R6", "2"), ("SW2", "1"), ("TP10", "1"), ("U1", "27")},
    "CC1": {("J1", "A5"), ("R1", "1")},
    "CC2": {("J1", "B5"), ("R2", "1")},
    "EN": {("J2", "5"), ("R5", "2"), ("SW1", "1"), ("TP9", "1"), ("U1", "3")},
    "GND": {
        ("C1", "2"), ("C2", "2"), ("C3", "2"), ("C4", "2"), ("C5", "2"),
        ("D1", "1"), ("D2", "2"), ("J1", "A1"), ("J1", "A12"),
        ("J1", "B1"), ("J1", "B12"), ("J1", "S1"), ("J2", "2"),
        ("R1", "2"), ("R2", "2"), ("R8", "2"), ("SW1", "2"),
        ("SW2", "2"), ("TP3", "1"), ("U1", "1"), ("U1", "40"),
        ("U1", "41"), ("U2", "2"), ("U3", "2"), ("U4", "2"), ("U4", "3"),
    },
    "MIC_BCLK": {("TP4", "1"), ("U1", "6"), ("U4", "4")},
    "MIC_SD": {("R8", "1"), ("TP6", "1"), ("U1", "4"), ("U4", "6")},
    "MIC_WS": {("TP5", "1"), ("U1", "5"), ("U4", "1")},
    "STATUS_LED": {("R7", "1"), ("U1", "25")},
    "STATUS_LED_A": {("D1", "2"), ("R7", "2")},
    "UART_RX": {("J2", "4"), ("U1", "36")},
    "UART_TX": {("J2", "3"), ("U1", "37")},
    "USB_5V": {
        ("C1", "1"), ("D2", "1"), ("F1", "2"), ("TP1", "1"),
        ("U2", "1"), ("U2", "3"), ("U3", "5"),
    },
    "USB_D_N": {("R3", "2"), ("TP8", "1"), ("U1", "13")},
    "USB_D_N_CONN": {("J1", "A7"), ("J1", "B7"), ("U3", "3")},
    "USB_D_N_ESD": {("R3", "1"), ("U3", "4")},
    "USB_D_P": {("R4", "2"), ("TP7", "1"), ("U1", "14")},
    "USB_D_P_CONN": {("J1", "A6"), ("J1", "B6"), ("U3", "1")},
    "USB_D_P_ESD": {("R4", "1"), ("U3", "6")},
    "VBUS_RAW": {
        ("F1", "1"), ("J1", "A4"), ("J1", "A9"),
        ("J1", "B4"), ("J1", "B9"),
    },
}

EXPECTED_PIN_FUNCTIONS = {
    ("J1", "A4"): "VBUS", ("J1", "A9"): "VBUS",
    ("J1", "B4"): "VBUS", ("J1", "B9"): "VBUS",
    ("J1", "A5"): "CC1", ("J1", "B5"): "CC2",
    ("J1", "A6"): "D+", ("J1", "B6"): "D+",
    ("J1", "A7"): "D-", ("J1", "B7"): "D-",
    ("U1", "1"): "GND", ("U1", "2"): "3V3", ("U1", "3"): "EN",
    ("U1", "4"): "IO4", ("U1", "5"): "IO5", ("U1", "6"): "IO6",
    ("U1", "13"): "IO19", ("U1", "14"): "IO20", ("U1", "25"): "IO48",
    ("U1", "27"): "IO0", ("U1", "36"): "RXD0", ("U1", "37"): "TXD0",
    ("U1", "40"): "GND", ("U1", "41"): "GND",
    ("U2", "1"): "VIN", ("U2", "2"): "GND", ("U2", "3"): "EN",
    ("U2", "4"): "NC", ("U2", "5"): "VOUT",
    ("U3", "1"): "I/O1", ("U3", "2"): "GND", ("U3", "3"): "I/O2",
    ("U3", "4"): "I/O2", ("U3", "5"): "VBUS", ("U3", "6"): "I/O1",
    ("U4", "1"): "WS", ("U4", "2"): "LR", ("U4", "3"): "GND",
    ("U4", "4"): "SCK", ("U4", "5"): "VDD", ("U4", "6"): "SD",
}

EXPECTED_NO_CONNECTS = {
    ("J1", "A8"), ("J1", "B8"),
    ("U1", "7"), ("U1", "8"), ("U1", "9"), ("U1", "10"),
    ("U1", "11"), ("U1", "12"), ("U1", "15"), ("U1", "16"),
    ("U1", "17"), ("U1", "18"), ("U1", "19"), ("U1", "20"),
    ("U1", "21"), ("U1", "22"), ("U1", "23"), ("U1", "24"),
    ("U1", "26"), ("U1", "28"), ("U1", "29"), ("U1", "30"),
    ("U1", "31"), ("U1", "32"), ("U1", "33"), ("U1", "34"),
    ("U1", "35"), ("U1", "38"), ("U1", "39"), ("U2", "4"),
}

REQUIRED_FIELDS = ("Manufacturer", "MPN", "Supplier", "Supplier PN", "Availability", "BOM Comments")


def component_fields(component: ET.Element) -> dict[str, str]:
    return {
        field.attrib["name"]: (field.text or "").strip()
        for field in component.findall("./fields/field")
    }


def format_nodes(nodes: set[tuple[str, str]]) -> str:
    return ", ".join(f"{ref}.{pin}" for ref, pin in sorted(nodes))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("xml_netlist", type=Path)
    args = parser.parse_args()

    root = ET.parse(args.xml_netlist).getroot()
    components = {node.attrib["ref"]: node for node in root.findall("./components/comp")}
    nets = {
        node.attrib["name"].removeprefix("/"): node
        for node in root.findall("./nets/net")
    }
    failures: list[str] = []

    for ref, (manufacturer, mpn, footprint) in EXPECTED_COMPONENTS.items():
        component = components.get(ref)
        if component is None:
            failures.append(f"missing component {ref}")
            continue
        fields = component_fields(component)
        for field in REQUIRED_FIELDS:
            if not fields.get(field):
                failures.append(f"{ref}: missing required field {field}")
        actual = (
            fields.get("Manufacturer", ""),
            fields.get("MPN", ""),
            (component.findtext("footprint") or "").strip(),
        )
        expected = (manufacturer, mpn, footprint)
        if actual != expected:
            failures.append(f"{ref}: expected manufacturer/MPN/footprint {expected!r}, got {actual!r}")
        datasheet = (component.findtext("datasheet") or "").strip()
        if not datasheet or datasheet == "~":
            failures.append(f"{ref}: missing datasheet URL")

    j2 = components.get("J2")
    if j2 is None or not any(p.attrib.get("name") == "dnp" for p in j2.findall("property")):
        failures.append("J2: expected KiCad DNP property")

    for name, expected in EXPECTED_NETS.items():
        net = nets.get(name)
        if net is None:
            failures.append(f"missing net /{name}")
            continue
        actual = {(node.attrib["ref"], node.attrib["pin"]) for node in net.findall("node")}
        if actual != expected:
            failures.append(
                f"/{name}: expected [{format_nodes(expected)}], got [{format_nodes(actual)}]"
            )

    all_nodes = {
        (node.attrib["ref"], node.attrib["pin"]): node.attrib.get("pinfunction", "")
        for net in root.findall("./nets/net")
        for node in net.findall("node")
    }
    for key, expected in EXPECTED_PIN_FUNCTIONS.items():
        actual = all_nodes.get(key)
        if actual != expected:
            failures.append(f"{key[0]}.{key[1]}: expected pin function {expected!r}, got {actual!r}")

    actual_no_connects = {
        (node.attrib["ref"], node.attrib["pin"])
        for net in root.findall("./nets/net")
        for node in net.findall("node")
        if "no_connect" in node.attrib.get("pintype", "")
    }
    if actual_no_connects != EXPECTED_NO_CONNECTS:
        failures.append(
            "explicit no-connects: expected "
            f"[{format_nodes(EXPECTED_NO_CONNECTS)}], got [{format_nodes(actual_no_connects)}]"
        )

    if failures:
        print("critical netlist verification FAILED", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print(
        "critical netlist verification passed: "
        f"{len(EXPECTED_COMPONENTS)} components, {len(EXPECTED_NETS)} nets, "
        f"{len(EXPECTED_PIN_FUNCTIONS)} pin functions, "
        f"{len(EXPECTED_NO_CONNECTS)} explicit no-connects"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
