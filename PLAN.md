# Quiet Trace implementation plan

## Scope

Quiet Trace is a USB-powered indoor ambient sound-level logger. It computes and stores only non-reconstructable loudness summaries, presents them through a local responsive dashboard, and supports explicit export/deletion. The MVP ends at one reproducible assembled prototype design plus firmware/app builds and static/integration evidence; it does not imply certified acoustic performance or completed physical testing until measured artifacts are published.

## Architecture

```text
room sound
   │
   ▼
digital MEMS microphone ─I2S/PDM─> ESP32-S3
                                      ├─ fixed-size sample window
                                      ├─ DC removal / weighting / RMS
                                      ├─ aggregate + discard samples
                                      ├─ flash ring buffer
                                      ├─ LED + setup/mark button
                                      ├─ USB serial configuration/export
                                      └─ local HTTP API + SSE
                                                   │
                                                   ▼
                                      device-hosted TypeScript dashboard
```

### Trust boundaries

- **Sensor/DSP boundary:** PCM samples may exist only inside a fixed-size volatile processing buffer.
- **Persistence boundary:** the record writer accepts aggregates, never sample arrays or encoded audio.
- **Network boundary:** only authenticated aggregate/status/config endpoints are exposed; physical presence opens setup.
- **Export boundary:** CSV/JSON excludes Wi-Fi credentials, setup secrets, and browser-local preferences.

### Proposed repository structure

```text
app/                    device-hosted responsive dashboard
bom/                    preliminary and schematic-exported BOM data
docs/                   protocol, calibration, bring-up, assembly
firmware/               ESP-IDF application and host-testable DSP/domain code
hardware/kicad/          real editable KiCad project/schematic/PCB
hardware/mechanical/     enclosure source and drawings
```

## Technology choices

- **ESP32-S3 / ESP-IDF:** sufficient digital-audio/DSP headroom, native USB options, Wi-Fi, mature local networking, and testable component model. Exact module variant remains subject to datasheet and availability review.
- **Digital MEMS microphone:** avoids an uncharacterized analog preamplifier in the first revision. Selection must be based on manufacturer sensitivity tolerance, frequency response, acoustic overload point, supply/interface compatibility, lifecycle, and package/assembly path.
- **USB 5 V SELV power:** avoids battery charging, runtime claims, and mains design while keeping a stationary logger useful.
- **Flash ring buffer:** aggregate records are small enough to avoid removable storage in MVP; wear and power-loss behavior must be measured.
- **TypeScript + Vite static dashboard:** produces a compact device-served UI usable from modern phone and desktop browsers without an installed account-bearing app.
- **Versioned HTTP/JSON + SSE API:** understandable, inspectable, and functional over local networking; USB serial is the offline recovery/export path.
- **KiCad 9+:** editable, reviewable open-hardware source with schematic properties as BOM source of truth.

## Milestones and dependency order

### M0 — Requirements and proof strategy

- Freeze metric definitions, privacy invariants, retention limits, clock-quality states, safety language, and cost target.
- Define synthetic DSP vectors and an optional reference-meter comparison procedure.
- Decide which claims can be supported by static analysis, simulation, bench measurement, or field observation.

### M1 — Parts and electrical architecture

- Compare controller/module and microphone candidates using manufacturer datasheets.
- Validate voltage/current/pinout/package, microphone acoustic constraints, USB protection, reset/programming, test points, lifecycle, and availability.
- Record Manufacturer and MPN in KiCad symbol properties after selection; no guessed identifiers.

### M2 — Schematic, source-of-truth BOM, and ERC

- Create `quiet-trace.kicad_pro` and `quiet-trace.kicad_sch` with power/protection, controller, microphone, controls, status, debug, and named interfaces.
- Run ERC; resolve or explicitly justify each exception.
- Export and validate `bom/bom.csv`, with enclosure/cable/fasteners tracked separately.

### M3 — Firmware and dashboard vertical slice

- Implement sample acquisition, deterministic aggregate pipeline, raw-buffer erasure/overwrite, ring-buffer records, setup/mark controls, and USB serial.
- Implement versioned API, live SSE, history, annotations, export, retention, erase, and responsive accessible dashboard.
- Test DSP fixtures, record migration, power-loss behavior, API validation, and privacy invariants on host/simulated boundaries.

### M4 — PCB and mechanical design

- Freeze board outline and microphone port/enclosure relationship.
- Place and route power, USB, digital audio, antenna keepout, controls, mounting, silkscreen, and test access.
- Run DRC and analyzers; inspect outputs and document exceptions.
- Create editable enclosure source that preserves acoustic clearance and USB/button access.

### M5 — Integration and bring-up

- Assemble only after design and sourcing gates are met.
- Measure rails, current, clock/acquisition behavior, storage recovery, network/recovery paths, and thermal behavior.
- Compare calibrated/uncalibrated behavior against a documented reference setup; never infer certification.
- Record photos only after hardware exists and label renders/placeholders honestly.

### M6 — Reproducible release

- Finish assembly, pinout, calibration, troubleshooting, privacy verification, and recovery docs.
- Export and inspect Gerbers, drill, BOM, CPL when applicable, schematic PDF, board renders, licenses, checksums, firmware/web artifacts, and release archive.

## Testing strategy

- **Static:** formatting, TypeScript checks, firmware compiler warnings, KiCad ERC/DRC, schematic/PCB analyzers, BOM field/MPN coverage.
- **Unit:** DSP math against generated vectors, histogram/percentile logic, calibration-state propagation, ring-buffer wrap/migration, API schemas, CSV/JSON round trips.
- **Property/fuzz:** malformed configuration/import/protocol frames; ensure no persistence endpoint can accept sample arrays or encoded media.
- **Integration with simulation/mocks:** synthetic I2S provider through aggregation, power-loss injection around commits, local API/SSE/dashboard contract, USB recovery flows.
- **Bench:** rail/current measurements, microphone data framing, button/LED, Wi-Fi/USB recovery, flash retention, reference-meter comparison. Bench evidence is not certification.
- **Field observation:** optional room surveys to evaluate workflow and annotation usefulness, clearly separated from calibrated/regulated claims.

## Packaging and distribution

- Reproducible ESP-IDF firmware binaries and source with flashing/recovery instructions.
- Dashboard bundled into firmware and also buildable as a static preview against fixture data.
- Fabrication ZIP with inspected Gerbers/drill and BOM/CPL only after DRC and source review.
- Versioned release archive containing source snapshot, checksums, schematic PDF, board render, firmware, dashboard, protocol, assembly, and licenses.
- GitHub Releases; no app store, cloud backend, telemetry, or mandatory account.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Uncalibrated microphone produces misleading values | Default to an explicit uncalibrated state; validate sensitivity tolerance and require documented offset procedure. |
| Enclosure/port changes frequency response | Treat microphone placement and acoustic path as requirements; compare assembled configurations before claims. |
| Aggregates accidentally become reconstructable | Store bounded statistics only; prohibit spectra/audio; schema and tests reject sample arrays and media fields. |
| ESP32 task load drops samples | Bounded queues, overrun counters, synthetic stress tests, and visible data-quality flags. |
| Flash wear or corruption | Batched append-only ring records, checksums/versioning, measured write rate, power-loss tests, export/restore. |
| Setup endpoint leaks credentials or data | Physical-presence setup window, randomized secret, local-only API, credential exclusion tests, erase path. |
| Wi-Fi unavailable | Autonomous logging plus USB serial setup/export/recovery. |
| Parts are unavailable or package is unsuitable | Datasheet- and lifecycle-backed selection before schematic freeze; second-source strategy where feasible. |
| Scope expands into surveillance or compliance | Enforce no-audio/non-medical/non-regulatory non-goals in architecture, API schema, UI wording, and reviews. |

## Explicit non-goals

- Raw audio, clips, streaming, spectral archives, voice/speaker/event recognition, or transcription.
- Certified sound-level meter, occupational dose, hearing-health advice, medical or legal use.
- Cloud service, remote monitoring, accounts, analytics, or subscription.
- Battery charger, mains interface, outdoor enclosure, actuator, alarm, or safety-critical control.
- Manufacturing-scale optimization before one documented prototype is brought up.
