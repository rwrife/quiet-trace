# Quiet Trace

> USB-powered ESP32-S3 ambient sound-level logger for home offices and shared rooms that keeps only privacy-preserving loudness summaries and serves a local history dashboard—never raw audio.

## Overview

Quiet Trace is an open-hardware instrument for understanding *when* a room becomes noisy without installing a recorder or sending microphone data to a cloud service. An ESP32-S3 samples a digital MEMS microphone, calculates bounded-window loudness statistics in memory, discards each audio buffer, and stores only aggregate records such as one-minute equivalent level, peak window, and histogram buckets. A responsive dashboard served on the local network shows live level, timelines, annotations, and portable exports.

Quiet Trace is a practical survey and awareness tool, **not** a certified sound-level meter, occupational exposure instrument, medical device, security monitor, or legal evidence recorder. Displayed units must be labeled uncalibrated until the device has completed a documented reference-meter calibration.

## Motivation

Phone sound-meter apps are convenient but tie up a phone, vary by device, and can create understandable recording concerns. Professional logging meters are expensive and often export through proprietary software. Quiet Trace aims for a small, repairable, visibly non-recording device that can remain on a desk, compare rooms or time periods, and keep the data owner in control.

## Target users

- Remote workers comparing distracting periods before changing their schedule or workspace.
- Households investigating recurring appliance, traffic, or shared-room noise patterns.
- Makers learning digital audio measurement, calibration, embedded aggregation, and privacy-by-design.
- Community rooms and workshops that want a simple local trend display without storing conversations.

## Concrete use cases

1. Put Quiet Trace on a desk, select an existing room label, and watch the current aggregate level.
2. Add a local annotation such as “window opened” or “dishwasher started” and compare the before/after timeline.
3. Move the device to another room and run the same timed survey profile.
4. Export versioned CSV/JSON summaries for a spreadsheet, then delete selected sessions from the device.
5. Demonstrate privacy by inspecting the firmware/data schema: no waveform, encoded audio, transcript, or frequency reconstruction data is persisted or sent.

## Intended workflow

1. Connect Quiet Trace to a certified USB 5 V supply or computer.
2. On first setup, hold the physical setup button to open a time-limited local setup session.
3. Join the temporary device network or use USB serial, set a device name/time source, and optionally provide local Wi-Fi credentials.
4. Open the device-hosted dashboard from a phone or computer on the same network.
5. Choose a room/session label and start logging. The LED communicates recording-*statistics* state without relying on color alone.
6. Review live and historical aggregates; annotate, export, back up, or erase them locally.
7. If calibrated readings are needed, follow the reference-meter procedure and retain the calibration date and offset with every export.

If Wi-Fi is unavailable, logging and physical status remain functional; USB serial supports configuration and data export. No internet service or account is required.

## MVP features

- ESP32-S3 controller, digital I2S/PDM MEMS microphone, status LED, setup/mark button, USB power/data, and test points on an editable KiCad carrier board.
- Fixed-size in-memory sample buffers, documented weighting/aggregation math, and irreversible raw-sample discard.
- One-minute summary records with monotonic sequence, wall-clock quality flag, average/equivalent level, bounded peak, and histogram buckets.
- Explicit calibration state: uncalibrated by default; single-point offset procedure against a reference meter.
- Local flash ring buffer with wear-aware commits and export-before-erase controls.
- Device-hosted responsive web dashboard for setup, live status, history, annotations, CSV/JSON export, backup/restore, retention, and deletion.
- Local-only authenticated API and time-limited physical-presence setup mode.
- Repeatable firmware/web builds, simulation or hardware-abstraction tests, ERC/DRC evidence, bring-up measurements, and assembly/fabrication documentation.

## Non-goals

- Persisting or streaming raw audio, audio clips, spectra, voice activity, speech, transcripts, or speaker identity.
- Certified IEC/ANSI sound-level measurements, occupational dose, hearing-risk advice, diagnosis, treatment, emergency alerts, or regulatory compliance.
- Remote surveillance, cloud dashboards, accounts, analytics, advertisements, or subscription services.
- Automatic identification of a sound source.
- Battery charging in the MVP; operation is USB 5 V SELV only.

## Privacy, permissions, and data storage

Raw microphone samples exist only in bounded RAM buffers required for calculation and are overwritten after each window. The device must never write or transmit samples, recordings, transcripts, or reconstructable spectral data. Stored records contain aggregates, device/session metadata, user-entered annotations, calibration metadata, and timestamps. Data stays in on-device flash and the browser’s local storage unless the user explicitly downloads an export.

The dashboard requires local-network access. Wi-Fi credentials are stored only on the device, must be erasable by a physical reset flow, and are never included in ordinary exports. Setup credentials/tokens must not be hard-coded. No microphone permission is requested from the phone or computer because the device owns the sensor. Optional browser file access is user-initiated for backup restore only.

## Hardware direction

Revision A selects an Espressif `ESP32-S3-WROOM-1-N8R8` module and TDK InvenSense `ICS-43434` I²S microphone, with USB-C 5 V power/protection, non-color-only LED status, physical RESET and SETUP/MARK controls, recovery header, and test access. Exact selected parts, pin/package checks, acoustic constraints, the LDO thermal bound, and point-in-time sourcing evidence are documented in [hardware/parts-selection.md](hardware/parts-selection.md). Stock, prices, and formal lifecycle status still require a pre-order procurement check.

Current editable sources are:

```text
hardware/kicad/quiet-trace.kicad_pro
hardware/kicad/quiet-trace.kicad_sch
hardware/kicad/quiet-trace.kicad_pcb
```

PDFs and renders may supplement these files but will never replace them. Final BOM data lives in KiCad schematic symbol properties (`Manufacturer`, `MPN`, supplier fields, and notes) and is exported to tracked [`bom/bom.csv`](bom/bom.csv). `bom/preliminary-bom.csv` remains planning input only.

Prototype planning target: **USD 35–60**, including electronics, simple enclosure, cable, and ordinary fasteners but excluding a phone/computer, tools, and a reference sound meter. This is a target rather than a live quote; the tracked BOM preserves `TBD` wherever current unit pricing was not actually observed, and all availability must be rechecked before ordering.

## Safety limits

- USB 5 V SELV only; no mains wiring, battery charging, or control of safety-critical equipment.
- Use a certified USB supply and intact cable.
- Indoor, dry-location prototype; not weatherproof.
- Enclosure must prevent conductive debris contact while preserving the microphone’s specified acoustic path.
- Results are informational and unfit for hearing-safety, workplace compliance, tenancy disputes, law enforcement, or emergency decisions.

## Current status and milestones

**Status: issue-5 dashboard/protocol slice is merged; issue-6 physical integration evidence is in progress.** The repository now contains architecture/privacy/metric contracts, selected schematic/BOM, PCB/mechanical draft, host-tested firmware domain contracts, and the merged local dashboard implementation with typed aggregate protocol fixtures. Issue #6 assembly/bring-up/calibration/troubleshooting guides are published, but real bench measurements and end-to-end physical evidence are still required before any calibrated claim. No certification, safety, legal, or bench-calibrated claim is made.

1. Freeze measurable requirements and privacy threat model.
2. Validate controller/microphone/protection choices from manufacturer datasheets.
3. Create the real KiCad schematic and schematic-backed BOM; pass ERC.
4. Build the aggregation firmware and local dashboard against synthetic fixtures.
5. Lay out the PCB; pass DRC and KiCad analysis.
6. Integrate and bring up a prototype with documented measurements and calibration limits.
7. Publish assembly instructions and inspected fabrication/release outputs.

See [PLAN.md](PLAN.md), [hardware/requirements.md](hardware/requirements.md), and the GitHub issue backlog.

Issue-#6 integration documents:
- [Assembly guide](docs/integration/assembly-guide.md)
- [Bring-up checklist](docs/integration/bring-up-checklist.md)
- [Calibration/reference comparison procedure](docs/integration/calibration-reference-comparison.md)
- [Troubleshooting guide](docs/integration/troubleshooting.md)
- [Bench evidence log template](docs/integration/bench-log-template.md)

## Development quickstart

Pinned versions and setup details are in [docs/toolchains.md](docs/toolchains.md).

```bash
# Dashboard: exact lockfile, static checks, tests, and production bundle
cd app
npm ci
npm run format:check
npm run lint
npm run typecheck
npm run test:run
npm run build

# Host-testable firmware domain contract
cd ..
cmake -S firmware/host_tests -B /tmp/quiet-trace-host-tests \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/quiet-trace-host-tests --parallel
ctest --test-dir /tmp/quiet-trace-host-tests --output-on-failure

# ESP-IDF v6.1 target scaffold (after exporting the pinned IDF environment)
cd firmware
idf.py set-target esp32s3
idf.py build
```

The dashboard’s synthetic view is opt-in at `/?fixture=1` and visibly marked as fixture-only. The default route does not fabricate sensor data. Target flashing, microphone behavior, calibration, and bench results remain unverified until the exact hardware is selected and present.

## Licensing plan

The initial documentation/software scaffold is MIT licensed. Before hardware source release, the project will add an explicit open-hardware license (planned CERN-OHL-S-2.0) and document boundaries for firmware, web app, documentation, and third-party assets.
