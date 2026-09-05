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

## Revision-A PCB and enclosure

Revision A uses an 80 × 52 mm two-layer carrier with four M3 holes. The USB-C receptacle is on the left edge, the WROOM module antenna overhangs the top edge, the bottom-port microphone is aligned to its footprint's 0.5 mm NPTH, and reset/setup/status controls remain accessible from the top. The board has labeled test points for protected 5 V, 3V3, GND, USB D+/D−, microphone clocks/data, EN, and BOOT0.

`hardware/mechanical/quiet-trace-enclosure.scad` is an editable 88 × 60 × 24 mm two-part enclosure model. It aligns standoffs, USB access, button holes, status window, and microphone opening to PCB coordinates and opens with common M3 hardware. The model is dimensional/static evidence only: it has not been printed, assembled, RF-tested, thermally tested, or acoustically characterized.

## Safety limits

- Indoor dry-location prototype, USB 5 V SELV only.
- No claim of ingress protection, acoustic certification, hearing safety, regulatory compliance, surveillance, or emergency monitoring.
- Keep conductive debris away and maintain the module antenna/microphone keepouts.
- Raw audio must never be persisted or transmitted.

## Expected KiCad deliverables

Current editable paths:

```text
hardware/kicad/quiet-trace.kicad_pro
hardware/kicad/quiet-trace.kicad_sch
hardware/kicad/quiet-trace.kicad_pcb
hardware/kicad/quiet-trace.kicad_dru
hardware/mechanical/quiet-trace-enclosure.scad
```

The design must include power/protection, controller/module sockets or pads, microphone interface, controls, status, programming/recovery, test points, mounting, antenna/acoustic keepouts, and labeled connectors. Completion requires actual ERC/DRC output with every exception resolved or documented. Renders and schematic PDFs supplement but do not replace editable KiCad source.

Layout implementation: the official ESP32-S3-WROOM-1 footprint's embedded F.Cu/B.Cu antenna rule area spans the radiating edge and forbids tracks, vias, pads, pours, and footprints. U4 retains the footprint's 0.5 mm acoustic NPTH; board-level front/back acoustic rule areas prevent pours/vias beneath it and prevent back-side tracks. U3 sits directly behind J1; R3/R4 terminate near U1; C1/C2 flank U2; C3/C4 sit at the module rail entry; C5 is adjacent to U4. J2 remains DNP by default.

The continuous-ground strategy uses filled GND zones on F.Cu and B.Cu, clipped by antenna, mounting, and microphone rule areas. Power nets are widened after routing (`VBUS_RAW`/`USB_5V` 0.4 mm, `+3V3` 0.5 mm); USB and I²S are 0.25 mm. The custom rule file enforces these widths/clearances. These dimensions support a conservative two-layer prototype; they do not constitute controlled-impedance or EMC-compliance evidence.

### Assembly/fabrication constraints

- Two-layer 1.6 mm FR-4, 1 oz copper planning basis; minimum track/clearance 0.2 mm, minimum copper-to-edge 0.5 mm.
- Keep all copper, components, metal hardware, conductive coating, and cabling out of the WROOM antenna rule area.
- Do not apply solder paste, wash fluid, mesh, foam, adhesive, or debris to the ICS-43434 sound hole. Align any enclosure port before assembly.
- Fit U3 directly behind J1 and inspect USB-C/ESD orientation before power. D2 is bidirectional; D1 polarity and all IC/module pin-1 marks are present in the library footprints/silkscreen.
- J2 is DNP unless needed for development. Preserve access to U2 and TP1/TP2/TP3 for issue-#6 rail/current/temperature measurements.
- No Gerber/CPL/release package is checked in at this stage; issue #7 owns final fabrication export and inspection.

The reproducible issue-#2 ERC/netlist/BOM evidence is recorded in [`docs/verification/issue-2-schematic.md`](../docs/verification/issue-2-schematic.md). Issue-#3 DRC, routing, cross-analysis, EMC/thermal triage, and mechanical evidence is recorded in [`docs/verification/issue-3-pcb-mechanical.md`](../docs/verification/issue-3-pcb-mechanical.md). After exporting the KiCad XML netlist, run `python3 hardware/kicad/verify_netlist.py hardware/kicad/quiet-trace.xml` and `python3 hardware/kicad/verify_pcb.py hardware/kicad/quiet-trace.xml hardware/kicad/quiet-trace.kicad_pcb`.

Final Manufacturer/MPN/supplier data belongs in schematic symbol properties and is exported to `bom/bom.csv`; `bom/preliminary-bom.csv` is not the source of truth.
