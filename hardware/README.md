# Quiet Trace hardware

## System block description

```text
USB-C 5 V SELV
      │
      ├─ input protection / regulated module rail / test points
      │
      ▼
ESP32-S3 module ── digital audio ── MEMS microphone + acoustic port
      │
      ├─ setup/mark button
      ├─ status LED with blink-pattern redundancy
      ├─ native USB / serial recovery
      └─ Wi-Fi local dashboard
```

The first revision is a carrier around an Espressif ESP32-S3 module rather than a chip-down RF design. It should remain hand-assemblable, expose recovery and measurement points, and keep the module antenna region outside copper/enclosure obstructions.

## Controller choice

Revision A selects `ESP32-S3-WROOM-1-N8R8` after comparison with `ESP32-S3-MINI-1-N8`. The WROOM variant provides 8 MB flash and 8 MB PSRAM, native USB on GPIO19/GPIO20, official KiCad library support, and a less dense assembly footprint. See [parts-selection.md](parts-selection.md) for datasheet citations, sourcing limits, and the +65 °C normal operating limit of the R8 PSRAM variant.

## Interfaces

- Digital microphone: TDK InvenSense `ICS-43434`, 24-bit I²S; firmware clocks must stay within the selected datasheet operating bands.
- USB: 5 V power plus flashing, logs, setup, recovery, and aggregate export.
- Wi-Fi: local HTTP/JSON/SSE only; internet not required.
- Human input: one setup/mark button with short/long press semantics.
- Human output: LED patterns that remain understandable without color perception.
- Debug/test: ground, input/regulated rail, reset/boot, audio clocks/data, and spare serial/SWD-style module test access as supported.

## Power plan

USB 5 V SELV only. Revision A uses a 500 mA-hold resettable fuse, VBUS TVS, USB data ESD array, and a 600 mA AP2112K 3.3 V LDO. The LDO's current rating is not continuous thermal capacity: [parts-selection.md](parts-selection.md) records the static thermal bounds and required worst-case current/temperature bring-up measurement. No battery, charging circuit, mains interface, PoE, or external actuator is in MVP. Use a certified USB supply and intact cable.

## Enclosure and assembly concept

A small two-part ventilated desktop enclosure supports PCB standoffs, strain-free USB access, button access, visible status, and a datasheet-compliant microphone acoustic port. The port should face away from board/enclosure noise sources. Mechanical source must be editable; initial prototypes may be 3D printed. Design should permit opening with common tools and replacing the controller carrier or microphone board.

## Safety limits

- Indoor dry-location prototype, USB 5 V SELV only.
- No claim of ingress protection, acoustic certification, hearing safety, regulatory compliance, surveillance, or emergency monitoring.
- Keep conductive debris away and maintain the module antenna/microphone keepouts.
- Raw audio must never be persisted or transmitted.

## Expected KiCad deliverables

Current and planned paths:

```text
hardware/kicad/quiet-trace.kicad_pro
hardware/kicad/quiet-trace.kicad_sch
hardware/kicad/quiet-trace.kicad_pcb       # issue #3
hardware/kicad/quiet-trace.kicad_dru       # if custom rules are needed
```

The design must include power/protection, controller/module sockets or pads, microphone interface, controls, status, programming/recovery, test points, mounting, antenna/acoustic keepouts, and labeled connectors. Completion requires actual ERC/DRC output with every exception resolved or documented. Renders and schematic PDFs supplement but do not replace editable KiCad source.

Issue-#3 layout handoff: put the WROOM antenna at a board edge and implement the Espressif copper/component keepout as a KiCad rule area; place the bottom-port microphone over a minimum 0.5 mm sound hole with no copper or contamination beneath the acoustic port; place U3 directly behind J1 with the shortest ESD return; keep C1/C2 at U2 and C5 at U4; give U2 generous copper and preserve access for worst-case temperature measurement; leave J2 DNP by default.

The reproducible issue-#2 ERC, netlist, BOM, analyzer triage, and verification limits are recorded in [`docs/verification/issue-2-schematic.md`](../docs/verification/issue-2-schematic.md). After exporting the KiCad XML netlist, run `python3 hardware/kicad/verify_netlist.py hardware/kicad/quiet-trace.xml` to assert the critical selected-part properties, pin functions, and named-net memberships.

Final Manufacturer/MPN/supplier data belongs in schematic symbol properties and is exported to `bom/bom.csv`; `bom/preliminary-bom.csv` is not the source of truth.
