# Quiet Trace firmware

This directory contains the ESP-IDF `v6.1` scaffold for the ESP32-S3 target and an aggregate-only firmware vertical slice that is fully host-testable.

## Implemented in this slice

- Injectable acquisition pipeline (`AcquisitionPipeline`) with:
  - 24-bit framed sample validation (ICS-43434-compatible signed range)
  - clipping/overrun/dropped-interval/clock-uncertainty counters
  - deterministic one-minute aggregation (`level_eq_dbfs`, `peak_125ms_dbfs`, histogram)
  - explicit calibration offset/state propagation
  - raw input block zeroization after ingest
- Checksummed/versioned ring store (`AggregateRingStore`) with:
  - wrap retention
  - corruption recovery and power-loss simulation handling
  - legacy-version migration path
  - export-before-erase gating and generation tracking
- Control plane contracts (`control_plane`) with:
  - physical-presence setup token window
  - button action classification and accessible LED pattern contracts
  - aggregate/status JSON renderers and SSE event envelope
  - USB serial commands for setup, time set, Wi-Fi set, export, erase, recovery
- Aggregate schema semantic validator in C++ (`is_valid`) for cross-field constraints.

## Hard privacy boundary

- Production target code never synthesizes microphone values.
- Raw samples are accepted only in bounded volatile ingest blocks and are overwritten after aggregation.
- Persistence/API/USB layers accept only aggregate/control DTOs and reject forbidden raw/encoded/credential field names.
- Host tests are fixture evidence only; they are not bench hardware proof.

## Target build

Install/export ESP-IDF `v6.1`, then:

```bash
idf.py set-target esp32s3
idf.py build
```

Flash/monitor (requires attached hardware and selected serial port):

```bash
idf.py -p "$ESPPORT" flash monitor
```

Factory erase and recovery commands:

```bash
# Erase all flash content
idf.py -p "$ESPPORT" erase-flash

# If normal flashing fails, hold BOOT, tap RESET, then retry flash+monitor
idf.py -p "$ESPPORT" flash monitor
```

USB serial recovery flow (line commands) after monitor/serial attach:

```text
setup begin
export json
erase records <token>
erase factory <token>
reboot recovery <token>
```

No flash or serial command should be treated as verified hardware behavior until issue #6 integration/bring-up captures physical evidence.

## Host tests

```bash
cmake -S host_tests -B /tmp/quiet-trace-host-tests \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/quiet-trace-host-tests --parallel
ctest --test-dir /tmp/quiet-trace-host-tests --output-on-failure
```

See [`docs/toolchains.md`](../docs/toolchains.md), [`docs/metrics.md`](../docs/metrics.md), and [`docs/privacy-threat-model.md`](../docs/privacy-threat-model.md).
