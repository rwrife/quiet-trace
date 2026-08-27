# Quiet Trace firmware plan

Status: planned responsibilities and boundaries; no firmware project or successful build exists yet.

## Responsibilities

- Configure and sample the selected digital microphone at its datasheet-supported timing.
- Process fixed-size volatile buffers through DC removal, documented weighting/normalization, RMS/equivalent-level calculation, peak-window tracking, and bounded histogram aggregation.
- Overwrite/discard each raw buffer after aggregation. Never persist, encode, stream, log, or expose raw samples, audio clips, spectra, speech, transcripts, or speaker/event identity.
- Attach quality flags for calibration, clipping, overruns, clock uncertainty, missing records, and reset recovery.
- Append versioned one-minute aggregates to a wear-aware checksummed flash ring buffer.
- Handle physical setup/mark button and status LED patterns.
- Serve the bundled dashboard and versioned local HTTP/JSON/SSE API.
- Provide USB serial setup, diagnostic counters, aggregate export, factory erase, and recovery.

## Proposed components

- `audio_source`: hardware driver behind a synthetic source interface.
- `aggregate_dsp`: host-testable fixed-point/float pipeline with generated fixtures.
- `record_store`: versioned append/ring semantics, checksums, migration, retention, and power-loss recovery.
- `clock`: monotonic sequence plus wall-clock and quality/source metadata.
- `device_state`: setup/calibration/session/annotation model.
- `api`: typed aggregate-only contracts and validation.
- `usb_console`: recovery-safe commands that never expose samples or secrets.
- `web_assets`: reproducibly bundled dashboard files.

## Interfaces and protocol

The initial contract is in [`docs/protocol.md`](../docs/protocol.md). Network transport is local HTTP with SSE for live aggregate updates. USB serial mirrors essential setup/export/recovery operations. Every persistent/network schema is aggregate-only and versioned.

## Provisioning and updates

First setup requires a physical long press, opens a time-limited local setup session, and creates a randomized device secret. Local Wi-Fi credentials remain device-only and are erasable. MVP firmware updates use signed/release-checksummed USB flashing with a documented recovery path; network OTA is deferred until authenticity, rollback, and failure recovery are implemented and tested.

## Test strategy

- Host unit tests over deterministic PCM fixtures for level math, clipping, histogram, calibration flags, and overrun propagation.
- Boundary tests proving record/API serializers reject sample arrays, binary media, spectra, credential fields, and oversized annotations.
- Ring-buffer wrap, migration, corruption, retention, and power-loss injection tests.
- ESP-IDF component tests for driver framing, queues, button state machine, LED patterns, API authentication, and USB commands.
- Integration tests from synthetic source to stored/exported aggregates and dashboard fixtures.
- Bench checks for rails/current, microphone framing, overrun counters, Wi-Fi loss, USB recovery, and reference-meter comparison.

Static analysis, simulation, and bench/field observations must be reported separately. An unbuilt prototype is never called tested.
