# Quiet Trace firmware scaffold

This directory contains an ESP-IDF v6.1 ESP32-S3 target scaffold and host-testable aggregate/audio-source contracts. A deterministic synthetic sample source exists only under `host_tests/support`; it is not registered as an ESP-IDF component and cannot feed the production target. The scaffold does **not** contain a selected microphone driver, persisted records, networking, USB protocol, or physical test evidence yet.

## Boundaries

- Production target code never synthesizes microphone values.
- Raw samples will be confined to a bounded volatile acquisition/DSP owner and overwritten after aggregation.
- Persistence, API, USB, log, crash, and export code may accept only typed aggregate/configuration contracts.
- The current `AggregateRecordV1` host test is structural/synthetic evidence, not hardware evidence.

## Target build

Install/export ESP-IDF `v6.1`, then:

```bash
idf.py set-target esp32s3
idf.py build
```

No flash command should be treated as verified until issue #2 selects the controller/module and a target is physically attached.

## Host test

```bash
cmake -S host_tests -B /tmp/quiet-trace-host-tests \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/quiet-trace-host-tests --parallel
ctest --test-dir /tmp/quiet-trace-host-tests --output-on-failure
```

See [`docs/toolchains.md`](../docs/toolchains.md), [`docs/metrics.md`](../docs/metrics.md), and [`docs/privacy-threat-model.md`](../docs/privacy-threat-model.md).
