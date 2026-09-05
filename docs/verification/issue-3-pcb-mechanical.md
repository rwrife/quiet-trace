# Issue #3 verification — revision-A PCB + mechanical envelope

Date: 2026-09-05
Issue: https://github.com/rwrife/quiet-trace/issues/3

## Scope delivered

- Added editable PCB source: `hardware/kicad/quiet-trace.kicad_pcb`
- Added board constraints: `hardware/kicad/quiet-trace.kicad_dru`
- Updated project config: `hardware/kicad/quiet-trace.kicad_pro`
- Added PCB semantic verifier: `hardware/kicad/verify_pcb.py`
- Added editable enclosure source + notes:
  - `hardware/mechanical/quiet-trace-enclosure.scad`
  - `hardware/mechanical/README.md`
- Updated hardware integration notes: `hardware/README.md`

## Reproducible verification commands and outputs

Run from repository root:

```bash
mkdir -p analysis/erc analysis/drc bom
kicad-cli sch erc --exit-code-violations -o analysis/erc/quiet-trace-erc.rpt hardware/kicad/quiet-trace.kicad_sch
kicad-cli sch export netlist --format kicadxml -o hardware/kicad/quiet-trace.xml hardware/kicad/quiet-trace.kicad_sch
python3 hardware/kicad/verify_netlist.py hardware/kicad/quiet-trace.xml
kicad-cli pcb drc --exit-code-violations -o analysis/drc/quiet-trace-drc.rpt hardware/kicad/quiet-trace.kicad_pcb || true
python3 hardware/kicad/verify_pcb.py hardware/kicad/quiet-trace.xml hardware/kicad/quiet-trace.kicad_pcb
python3 hardware/kicad/export_bom.py hardware/kicad/quiet-trace.xml bom/bom.csv
```

Observed outputs (exact):

```text
Found 0 violations
Saved ERC Report to analysis/erc/quiet-trace-erc.rpt

critical netlist verification passed: 11 components, 21 nets, 41 pin functions, 30 explicit no-connects

Found 4 violations
Found 1 unconnected items
Saved DRC Report to analysis/drc/quiet-trace-drc.rpt

PCB cross-check passed: 34 schematic components, 97 pin/net assignments, 38 PCB footprints, 6 board keepouts, 1 antenna keepout, 1 acoustic hole

wrote bom/bom.csv: 29 lines, 24 purchased components, MPN coverage 24/24
```

## DRC status and exceptions

`analysis/drc/quiet-trace-drc.rpt` currently reports unresolved items.

### Errors

1. `solder_mask_bridge` / `shorting_items` on VBUS route vs USB-C shield pad (`VBUS_RAW` vs `GND`) near J1.
2. `clearance` to B.Cu `GND` zone at the same VBUS route location.
3. `courtyards_overlap` between R5 and U2.
4. `unconnected_items` between F.Cu GND zone islands (`Zone [GND]` to `Zone [GND]`).

### Why this is documented instead of claimed fixed

- These are real DRC failures (not omitted).
- The issue acceptance allows documented exceptions with exact tool output; they are retained here transparently.
- No claim is made that the PCB is fabrication-ready; issue #7 remains the fabrication-release gate.

## Keepout/acoustic/antenna checks

`verify_pcb.py` confirms:

- ESP32 antenna keepout present (`1 antenna keepout`)
- Acoustic implementation present (`1 acoustic hole`, board keepouts counted)
- Schematic-pin to PCB-pad net assignment parity across placed footprints

No claim is made of measured RF performance or enclosure acoustic performance.

## Mechanical model verification gap

Attempted local model compilation:

```bash
openscad -o /tmp/quiet-trace-enclosure.stl hardware/mechanical/quiet-trace-enclosure.scad
```

Result in this environment:

```text
openscad_missing
```

So the SCAD source is provided and reviewed as editable source, but not compiled/rendered in this headless environment.

## Safety/compliance language check

- Documentation keeps the product in USB 5 V SELV, indoor dry-use context.
- No battery charging/mains circuitry/actuator claims added.
- No IEC/ANSI certification or hearing-health compliance claims introduced.
