# Issue #5 dashboard + protocol verification (2026-09-07)

This report captures evidence for **issue #5** (`Build the accessible local dashboard and typed aggregate protocol`).

## Evidence classification

- **Static analysis:** formatter/lint/type-check/schema checks, bundle-size check, firmware contract alignment check.
- **Simulation / fixture:** Vitest contract tests, fixture API behavior, reconnect logic tests.
- **Target compile (no bench hardware):** ESP-IDF v6.1 container build succeeds for ESP32-S3.

No physical-microphone, enclosure, RF, or browser-on-device bench evidence is included here.

## Commands executed and results

### Dashboard app (TypeScript/Vite)

From `app/`:

1. `npm run format:check` ✅
2. `npm run lint` ✅
3. `npm run typecheck` ✅
4. `npm run test:run` ✅ (8 files, 74 tests passed)
5. `npm run test:accessibility` ✅ (2 accessibility contract tests)
6. `npm run build` ✅
   - `dist/assets/index-*.js` raw 45,564 bytes; gzip 12,435 bytes
   - `dist/assets/index-*.css` raw 2,747 bytes; gzip 1,156 bytes
7. `npm run check:bundle-size` ✅ (total gzip 13,591 bytes; below 128 KiB limit)
8. `npm run check:firmware-contract` ✅

### Firmware host checks (pinned clang container)

From repository root:

- `docker run ... silkeh/clang@sha256:10854... bash -euxo pipefail -c 'clang-format --dry-run ...; cmake ...; ctest ...'` ✅
  - Verified clang-format `20.1.8`, cmake `3.25.1`, clang++ `20.1.8`
  - Host tests: 5/5 passed (`aggregate_contract`, `synthetic_source`, `acquisition_pipeline`, `record_ring`, `control_plane`)

### Firmware target build (ESP-IDF v6.1 container)

From repository root:

- `docker run ... espressif/idf@sha256:81893... /opt/esp/entrypoint.sh bash -lc 'idf.py --version; idf.py set-target esp32s3; idf.py build'` ✅
  - Verified `ESP-IDF v6.1`
  - Build completed successfully
  - Output binary size check: `quiet_trace.bin` 0x29820 bytes, app partition 0x100000 bytes (84% free)

## Scope covered by this evidence

- Dashboard sections implemented: setup/help, live aggregate status, calibration + quality labels, history table, session comparison, bounded annotations, privacy/data screen, permissions notes, retention/delete/export/restore/factory-erase controls.
- Protocol envelopes and fixtures added:
  - `docs/schemas/status-v1.schema.json`
  - `docs/schemas/records-page-v1.schema.json`
  - fixture payloads under `docs/schemas/fixtures/`
- Contract and behavior tests include:
  - unknown additive fields
  - schema/version mismatch
  - missing wall time handling
  - clipping/overrun counters in status
  - paging behavior
  - reconnect logic
  - export secret-field exclusions

## Explicit limits / not yet verified

- Browser/platform matrix is not covered (no Safari/Firefox/Chrome manual runs captured).
- No physical-device dashboard session evidence (this is fixture/static + compile evidence).
- No claims of calibration accuracy, occupational/legal/medical advice, or certified-meter behavior.