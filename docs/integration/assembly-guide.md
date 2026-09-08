# Revision-A assembly guide (issue #6)

This guide is the build-time checklist for **Quiet Trace revision A**:

- PCB: `hardware/kicad/quiet-trace.kicad_pcb` (80 × 52 mm)
- Mechanical: `hardware/mechanical/quiet-trace-enclosure.scad` (88 × 60 × 24 mm enclosure concept)
- Source of truth for parts: `bom/bom.csv`

> Evidence boundary: this document defines the process and inspection points. It does **not** itself claim a successful physical build. Fill measured results in `docs/integration/bench-log-template.md` during bench execution.

## 1) Safety + handling prerequisites

1. USB 5 V SELV only. No battery charging, mains wiring, or actuator/safety-control use.
2. Indoor dry-use prototype only.
3. ESD handling required for U1/U4 and any unpopulated module pads.
4. Keep microphone port and antenna keepout free of debris, adhesive, conformal coat, mesh, or metal.

Recommended tools:

- Temperature-controlled soldering iron + fine tip
- Hot-air rework station (optional but recommended for U1/U4/J1)
- Flux, solder wick, leaded or SAC solder, no-clean cleanup tools
- Stereo microscope or inspection camera
- DMM (continuity + DC current), optional bench PSU with current limit
- USB logic analyzer (for USB/I2S framing checks)
- Known-good USB-C cable + host
- Reference sound-level meter for calibration comparison

## 2) BOM references by subsystem

Use `bom/bom.csv` references exactly; do not substitute without recording variant details in the bench log.

- USB/power entry: `J1`, `F1`, `D2`, `C1`
- 3.3 V regulation: `U2`, `C2`, `C3`, `C4`
- Controller + controls: `U1`, `SW1`, `SW2`, `D1`, `R5`, `R6`, `R7`
- USB data protection/routing: `U3`, `R3`, `R4`, `TP7`, `TP8`
- Microphone front-end: `U4`, `C5`, `R8`, `TP4`, `TP5`, `TP6`
- Debug/recovery header (DNP by default): `J2`
- Measurement pads: `TP1`..`TP10`

## 3) Placement and orientation inspection before soldering

Inspect polarity/pin-1 and footprint orientation before heat:

- `J1` USB-C receptacle on left edge; shell tabs seated
- `U1` ESP32-S3-WROOM-1 at top edge, antenna overhang keepout unobstructed
- `U4` microphone aligned over board acoustic hole; no paste/contamination in port path
- `D1` LED polarity and visibility through enclosure window
- `D2` orientation per footprint; confirm protected `USB_5V` rail connection
- `SW1`/`SW2` orientation and plunger accessibility

## 4) Post-solder optical inspection checklist

- No bridges on fine-pitch pads (`U1`, `U4`, `J1`, `U3`)
- No tombstoned 0603/0805 passives
- All shell/ground joints complete on `J1`
- No solder balls or conductive debris in antenna/microphone keepouts
- `TP1`..`TP10` exposed and probe-accessible

Record pass/fail per step in bench log.

## 5) Connector, pinout, and test-point map

### J2 debug/recovery header (DNP default)

| Pin | Net | Function |
|---|---|---|
| 1 | +3V3 | Regulated rail out/in for debug only |
| 2 | GND | Ground reference |
| 3 | UART_TX | ESP32 TXD0 |
| 4 | UART_RX | ESP32 RXD0 |
| 5 | EN | Reset/enable control |
| 6 | BOOT0 | Boot strap / setup-mark line |

### Test-point locations (PCB mm coordinates)

Coordinates from `hardware/kicad/quiet-trace.kicad_pcb`:

| Ref | Net | X (mm) | Y (mm) | Bring-up use |
|---|---|---:|---:|---|
| TP1 | USB_5V | 17.00 | 13.00 | Protected VBUS check |
| TP2 | +3V3 | 23.00 | 12.00 | Regulated rail check |
| TP3 | GND | 28.00 | 47.50 | Probe return |
| TP4 | MIC_BCLK | 33.00 | 47.50 | I2S bit-clock framing |
| TP5 | MIC_WS | 47.00 | 47.50 | I2S word-select framing |
| TP6 | MIC_SD | 52.00 | 47.50 | I2S data activity |
| TP7 | USB_D+ | 21.00 | 24.00 | USB signal check |
| TP8 | USB_D- | 21.00 | 27.50 | USB signal check |
| TP9 | EN | 58.00 | 37.50 | Reset behavior |
| TP10 | BOOT0 | 68.00 | 37.50 | Boot strap/setup behavior |

## 6) Enclosure assembly steps

1. Print `quiet-trace-enclosure.scad` base/lid (`part="base"`, `part="lid"`).
2. Deburr holes; remove residue without forcing debris through the microphone opening.
3. Install board on four M3 standoffs/fasteners; verify USB port alignment and zero side-load.
4. Verify SW1/SW2 top access and LED window visibility before closing lid.
5. Keep any metallic hardware/cable strain relief out of antenna keepout zone.
6. Confirm microphone opening remains unobstructed and aligned to board hole.

## 7) Photo evidence policy (required during physical run)

When real hardware is assembled, include photo set in the bench report with explicit labels:

- `REAL_PHOTO`: populated PCB top
- `REAL_PHOTO`: populated PCB bottom
- `REAL_PHOTO`: USB connector close-up
- `REAL_PHOTO`: microphone port area close-up
- `REAL_PHOTO`: enclosure assembled/unassembled

If placeholders/renders are used, mark them `RENDER_PLACEHOLDER` and do not treat them as bench evidence.