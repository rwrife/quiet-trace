# Revision-A mechanical envelope

`quiet-trace-enclosure.scad` is the editable OpenSCAD source for the first serviceable desktop enclosure concept. It is dimensioned from the 80 × 52 mm KiCad PCB and renders an 88 × 60 × 24 mm two-part enclosure, below the 90 × 65 × 35 mm requirement.

## Coordinate contract

The OpenSCAD PCB coordinate system matches `hardware/kicad/quiet-trace.kicad_pcb` after a `[4, 4]` enclosure offset:

- four M3 board holes: `(3.475, 3.475)`, `(76.525, 3.475)`, `(76.525, 48.525)`, `(3.475, 48.525)`;
- USB-C receptacle centerline: left edge, PCB `y = 24.0`;
- reset and setup/mark top access: `(60, 32)` and `(68, 32)`;
- status window: `(69, 42)`;
- microphone sound path: board NPTH and enclosure opening aligned at `(40.0, 45.21)`.

Set `part` at the top of the source to `"base"`, `"lid"`, or `"assembled"`. Example export:

```sh
openscad -D 'part="base"' -o quiet-trace-base.stl quiet-trace-enclosure.scad
openscad -D 'part="lid"' -o quiet-trace-lid.stl quiet-trace-enclosure.scad
```

## Assembly and service notes

1. Print the base and lid in a non-conductive material suitable for indoor dry use.
2. Clear all holes and remove debris before fitting electronics. Do not wash or blow debris through the MEMS microphone port.
3. Mount the PCB on four 3 mm standoffs with M3 hardware. Confirm the USB plug inserts without loading the PCB.
4. Keep metal fasteners, conductive coatings, cables, and enclosure features outside the PCB antenna rule area.
5. Align the microphone board hole with the base opening; do not add mesh, gasket, foam, or adhesive over the port without a new acoustic comparison.
6. Verify button travel and status visibility before closing the lid. Status meaning is conveyed by patterns/timing, not color alone.
7. Disconnect USB before opening. The enclosure uses common M3 fasteners and is intended to remain serviceable.

## Evidence boundary

The source establishes dimensions, access, mounting, and nominal clearances only. The enclosure has not been printed or assembled. Its microphone port, ventilation, RF behavior, temperatures, fit, strength, and acoustic response are unmeasured and must be checked during issue #6 bring-up. It has no ingress-protection rating and is for USB 5 V SELV, indoor dry use only.
