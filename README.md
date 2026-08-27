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

The provisional architecture uses a Seeed Studio XIAO ESP32S3-class module, a datasheet-validated digital MEMS microphone or breakout, non-color-only LED status, a physical setup/mark button, and USB-C 5 V power. A custom carrier PCB will provide mounting, signal integrity, microphone acoustic clearance, protection, test access, and optional expansion. Exact manufacturer part numbers, package choices, costs, lifecycle, and availability are **not yet validated**.

Planned editable sources are:

```text
hardware/kicad/quiet-trace.kicad_pro
hardware/kicad/quiet-trace.kicad_sch
hardware/kicad/quiet-trace.kicad_pcb
```

PDFs and renders may supplement these files but will never replace them. Final BOM data belongs in KiCad schematic symbol properties (`Manufacturer`, `MPN`, supplier fields, and notes) and will be exported to tracked `bom/bom.csv`. The current `bom/preliminary-bom.csv` is planning input only.

Prototype planning target: **USD 35–60**, including electronics, simple enclosure, cable, and ordinary fasteners but excluding a phone/computer, tools, and a reference sound meter. This is a target rather than a live quote; every price and availability entry remains TBD until validated.

## Safety limits

- USB 5 V SELV only; no mains wiring, battery charging, or control of safety-critical equipment.
- Use a certified USB supply and intact cable.
- Indoor, dry-location prototype; not weatherproof.
- Enclosure must prevent conductive debris contact while preserving the microphone’s specified acoustic path.
- Results are informational and unfit for hearing-safety, workplace compliance, tenancy disputes, law enforcement, or emergency decisions.

## Current status and milestones

**Status: documentation and backlog scaffold only.** No application, firmware, schematic, PCB, validated BOM, ERC/DRC result, calibration result, fabricated board, or physical test is claimed yet.

1. Freeze measurable requirements and privacy threat model.
2. Validate controller/microphone/protection choices from manufacturer datasheets.
3. Create the real KiCad schematic and schematic-backed BOM; pass ERC.
4. Build the aggregation firmware and local dashboard against synthetic fixtures.
5. Lay out the PCB; pass DRC and KiCad analysis.
6. Integrate and bring up a prototype with documented measurements and calibration limits.
7. Publish assembly instructions and inspected fabrication/release outputs.

See [PLAN.md](PLAN.md), [hardware/requirements.md](hardware/requirements.md), and the GitHub issue backlog.

## Development quickstart

The implementation has not been created yet. The intended toolchain is:

- KiCad 9+ for editable hardware sources and ERC/DRC.
- ESP-IDF (pinned version to be selected) for ESP32-S3 firmware.
- TypeScript + Vite for the static dashboard bundled into firmware.
- Python/pytest or host-native C++ tests for deterministic DSP fixtures where practical.

Once issue #1 establishes the repository skeleton, expected commands will resemble:

```bash
idf.py build
idf.py flash monitor
npm ci --prefix app
npm test --prefix app
npm run build --prefix app
```

These are planned interfaces, not successful build evidence. Exact setup, versions, and commands will be committed with the implementation.

## Licensing plan

The initial documentation/software scaffold is MIT licensed. Before hardware source release, the project will add an explicit open-hardware license (planned CERN-OHL-S-2.0) and document boundaries for firmware, web app, documentation, and third-party assets.
