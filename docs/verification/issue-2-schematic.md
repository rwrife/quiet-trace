# Issue #2 schematic and BOM verification

Date: 2026-09-01

## Scope and verdict

Revision A now has editable KiCad 9 project and schematic sources plus a schematic-exported BOM. KiCad ERC passes with zero errors and zero warnings. The source covers USB-C 5 V SELV input/protection, 3.3 V regulation, ESP32-S3 module, digital microphone, native USB, reset/boot controls, status LED, optional recovery header, and test points.

This is static design evidence only. No PCB, enclosure, fabricated assembly, current/temperature measurement, acoustic calibration, EMC result, or physical test exists yet.

## Authoritative ERC

The host has no native `kicad-cli`, so the check used the locally available `parts-tally-kicad:9-arm64` container (`KiCad 9.0.9`). The command creates an ephemeral HOME and copies KiCad's packaged default symbol/footprint tables into it before ERC; it does not depend on mutable container state:

```sh
docker run --rm --user "$(id -u):$(id -g)" \
  -e HOME=/tmp/kicad-home -v "$PWD:/work" -w /work \
  parts-tally-kicad:9-arm64 sh -lc '
    mkdir -p "$HOME/.config/kicad/9.0"
    cp /usr/share/kicad/template/sym-lib-table "$HOME/.config/kicad/9.0/"
    cp /usr/share/kicad/template/fp-lib-table "$HOME/.config/kicad/9.0/"
    kicad-cli sch erc --exit-code-violations \
      -o analysis/erc/quiet-trace-erc.rpt \
      hardware/kicad/quiet-trace.kicad_sch
  '
```

Result:

```text
Found 0 violations
Saved ERC Report to analysis/erc/quiet-trace-erc.rpt
```

The report records `0 Errors 0 Warnings`.

## Generated netlist and connectivity checks

The same KiCad 9.0.9 environment exported `hardware/kicad/quiet-trace.xml` and `hardware/kicad/quiet-trace.net`. The generated files are ignored working artifacts. `hardware/kicad/verify_netlist.py` makes the critical checks repeatable:

```sh
python3 hardware/kicad/verify_netlist.py hardware/kicad/quiet-trace.xml
```

Result:

```text
critical netlist verification passed: 11 components, 21 nets, 41 pin functions, 30 explicit no-connects
```

The verifier checks the selected components' manufacturer/MPN/footprint/datasheet/supplier properties, J2's KiCad DNP flag, exact named-net membership, and datasheet-derived pin functions for J1/U1/U2/U3/U4. Critical generated-netlist checks include:

- `VBUS_RAW`: J1 VBUS pins A4/A9/B4/B9 and F1.1.
- `USB_5V`: F1.2, C1.1, D2.1, U2 VIN/EN, U3 VBUS, and TP1.
- `+3V3`: U2 VOUT, C2/C3/C4/C5, U1 3V3, U4 VDD, and TP2.
- USB D−: J1 A7/B7 → U3 I/O2 pair → R3 → U1 GPIO19/module pin 13.
- USB D+: J1 A6/B6 → U3 I/O1 pair → R4 → U1 GPIO20/module pin 14.
- Microphone: U1 GPIO6/5/4 to U4 SCK/WS/SD; U4 LR is tied low; U4 VDD is decoupled by C5; R8 is the 100 kΩ SD pull-down.
- Controls: EN has a 10 kΩ pull-up and active-low RESET; GPIO0/BOOT0 has a 10 kΩ pull-up and active-low SETUP/MARK.
- J2 exposes 3V3, GND, UART TX/RX, EN, and BOOT0 and is formally marked DNP in KiCad.

## BOM checks

The tracked `bom/bom.csv` is regenerated from the KiCad XML netlist:

```sh
python3 hardware/kicad/export_bom.py \
  hardware/kicad/quiet-trace.xml bom/bom.csv
```

Re-export to a temporary file and byte comparison passed (`bom_reproducible=yes`). The canonical BOM manager independently reported 18 active lines / 23 active component instances, with all 18 active lines carrying MPN and datasheet values. It recognizes the generic `Supplier` / `Supplier PN` fields as project-specific fields; `export_bom.py` preserves those fields in the tracked CSV. The BOM contains:

- 18 active purchased line items / 23 active purchased component instances.
- One DNP line: J2.
- Zero active lines missing Manufacturer, MPN, footprint, or datasheet.
- Test points represented as PCB features and excluded from purchased quantity.
- `TBD` retained wherever a current unit cost was not observed.

A sourcing cross-check found that the previously stored U3 code `C2687116` resolves to a UMW implementation, not the selected STMicroelectronics part. U3 now uses exact ST LCSC code `C7519`; live stock/price was not available from the accessible endpoint and therefore remains `TBD` with a mandatory pre-order recheck.

## Datasheet basis

Local manufacturer PDFs/text were reviewed for the critical parts and candidates, including ESP32-S3-WROOM-1, ESP32-S3-MINI-1, ICS-43434, IM69D130, AP2112, USBLC6-2SC6, USB4105, PESD5V0S1BA, and BSMD1206. `hardware/parts-selection.md` cites the relevant tables/sections and records exact pin, package, supply, timing, acoustic, and environmental constraints.

The structured `datasheets/extracted/` cache was not populated, so the schematic analyzer reports mixed trust and no datasheet-backed machine findings. Datasheet claims in the parts-selection document are manual PDF/text checks, not structured-extraction results.

## LDO static thermal bound

The AP2112 datasheet lists 184 °C/W junction-to-ambient for SOT-25 without a heatsink. For 5.0 V to 3.3 V:

```text
Ta=25C I=100mA P=0.170W Tj_static=56.3C
Ta=25C I=200mA P=0.340W Tj_static=87.6C
Ta=25C I=300mA P=0.510W Tj_static=118.8C
Ta=25C I=500mA P=0.850W Tj_static=181.4C
Ta=40C I=100mA P=0.170W Tj_static=71.3C
Ta=40C I=200mA P=0.340W Tj_static=102.6C
Ta=40C I=300mA P=0.510W Tj_static=133.8C
Ta=40C I=500mA P=0.850W Tj_static=196.4C
Ta=40C theoretical_current_at_Tj150=352mA
```

These are conservative static calculations, not measurements or recommended operating points. Issue #3 must provide generous U2 copper; bring-up must measure worst-case 3V3 current and U2 temperature. Sustained load outside safe margin requires a qualified buck-regulator redesign before fabrication release.

## Supplemental analyzer and triage

`analyze_schematic.py` reports 34 functional components, 51 nets, one DNP part, no missing MPNs, and no missing footprints. It emits one error and three warnings, all triaged against the KiCad-generated netlist and zero-violation ERC:

| Finding | Triage |
|---|---|
| PP-001: U2 VIN/`USB_5V` has no DC path to a power rail | Analyzer does not traverse F1. KiCad netlist shows J1 VBUS on `VBUS_RAW`, F1.1 on `VBUS_RAW`, and F1.2/U2.1 on `USB_5V`. |
| RS-001: `VBUS_RAW` has no declared source | `VBUS_RAW` is intentionally externally sourced by USB-C J1 and has a PWR_FLAG for ERC. |
| UC-001: no VBUS decoupling at J1 | C1 is deliberately on protected `USB_5V` after F1, at the LDO input. |
| UC-002: no VBUS TVS at J1 | D2 is deliberately on protected `USB_5V` after F1; U3 protects the USB data pair and references the protected VBUS rail. |

These are documented analyzer topology limitations, not waived KiCad ERC violations.

## Not performed / limits

- PCB/DRC, schematic-PCB pad/net cross-check, antenna/acoustic rule areas, EMC analysis, PCB thermal analysis, Gerber/drill/CPL inspection: not applicable until issue #3 creates the PCB.
- SPICE: no `ngspice`, `xyce`, or `ltspice` executable is installed; no analog transfer-function claim depends on a simulation here.
- Lifecycle: no formal manufacturer/distributor lifecycle API audit was available. Datasheet currency and orderable listings are point-in-time evidence only.
- Bench/field/calibration/certification: not performed. The board is not fabricated and no calibrated or compliance claim is made.

## Issue #3 handoff

- Place the WROOM antenna at the board edge and implement the Espressif copper/component keepout as a KiCad rule area.
- Put U4 over a minimum 0.5 mm sound hole; enforce copper/contamination clearance under its bottom port and keep enclosure turbulence/noise sources away.
- Place U3 directly behind J1 with straight-through differential routing and the shortest ESD ground return.
- Keep C1/C2 at U2 and C5 at U4; provide U2 copper for heat spreading and preserve temperature-probe access.
- Route USB as a controlled differential pair with matched topology; keep R3/R4 near U1.
- Preserve all test points and keep J2 DNP unless a development build explicitly populates it.
