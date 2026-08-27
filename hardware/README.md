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

The first revision is a carrier around a sourceable ESP32-S3 module rather than a bare-module RF design. It should remain hand-assemblable, expose recovery and measurement points, and keep the module antenna region outside copper/enclosure obstructions.

## Controller choice

A Seeed Studio XIAO ESP32S3-class module is the provisional controller because it offers ESP32-S3 DSP/networking resources in a compact, replaceable module with USB. The exact variant is not selected until its manufacturer documentation, pinout, power behavior, lifecycle, availability, antenna requirements, and native-USB constraints are checked. A standard Espressif development module remains a bring-up fallback.

## Interfaces

- Digital microphone: I2S or PDM, exact bus and clock rates selected from the chosen microphone datasheet.
- USB: 5 V power plus flashing, logs, setup, recovery, and aggregate export.
- Wi-Fi: local HTTP/JSON/SSE only; internet not required.
- Human input: one setup/mark button with short/long press semantics.
- Human output: LED patterns that remain understandable without color perception.
- Debug/test: ground, input/regulated rail, reset/boot, audio clocks/data, and spare serial/SWD-style module test access as supported.

## Power plan

USB 5 V SELV only. The carrier must document input protection, module current transients, decoupling, connector rating, and rail measurements. No battery, charging circuit, mains interface, PoE, or external actuator is in MVP. Use a certified USB supply and intact cable.

## Enclosure and assembly concept

A small two-part ventilated desktop enclosure supports PCB standoffs, strain-free USB access, button access, visible status, and a datasheet-compliant microphone acoustic port. The port should face away from board/enclosure noise sources. Mechanical source must be editable; initial prototypes may be 3D printed. Design should permit opening with common tools and replacing the controller carrier or microphone board.

## Safety limits

- Indoor dry-location prototype, USB 5 V SELV only.
- No claim of ingress protection, acoustic certification, hearing safety, regulatory compliance, surveillance, or emergency monitoring.
- Keep conductive debris away and maintain the module antenna/microphone keepouts.
- Raw audio must never be persisted or transmitted.

## Expected KiCad deliverables

Planned paths:

```text
hardware/kicad/quiet-trace.kicad_pro
hardware/kicad/quiet-trace.kicad_sch
hardware/kicad/quiet-trace.kicad_pcb
hardware/kicad/quiet-trace.kicad_dru       # if custom rules are needed
```

The design must include power/protection, controller/module sockets or pads, microphone interface, controls, status, programming/recovery, test points, mounting, antenna/acoustic keepouts, and labeled connectors. Completion requires actual ERC/DRC output with every exception resolved or documented. Renders and schematic PDFs supplement but do not replace editable KiCad source.

Final Manufacturer/MPN/supplier data belongs in schematic symbol properties and is exported to `bom/bom.csv`; `bom/preliminary-bom.csv` is not the source of truth.
