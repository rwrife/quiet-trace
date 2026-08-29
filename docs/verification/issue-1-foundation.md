# Issue #1 foundation verification

Date: 2026-08-29
Revision: feature worktree before commit/PR
Evidence class: local static analysis and deterministic synthetic fixtures only

## Dashboard

Command:

```bash
cd app
npm ci
npm audit --audit-level=high
npm run format:check
npm run lint
npm run typecheck
npm run test:run
npm run build
```

Observed result:

- npm installed/audited 157 packages and reported 0 vulnerabilities.
- Prettier, ESLint, and TypeScript completed with exit code 0.
- Vitest: 3 test files passed, 58 tests passed, 0 failed.
- Vite 8.2.2 production build completed; output included a 0.63 kB HTML file, 1.24 kB CSS asset, and 2.25 kB JavaScript asset before gzip values shown by Vite.
- Tests cover explicit fixture gating, the closed aggregate DTO, forbidden/unknown fields, complete calibration provenance, calibration and clock/value bounds, clock-quality/wall-time consistency, 480-window histogram coverage, executable JSON Schema/TypeScript UTC parity vectors, Unicode-scalar text bounds, and the required semantic-validation boundary for invariants standard Draft 2020-12 cannot express.

## Firmware host contract

Pinned CI/local container:

- `silkeh/clang@sha256:10854c9a1b4b8fa550b7c2261d298d9d078f5b89aebd50e7576d8599419cbbfd`
- CMake 3.25.1
- Clang/clang-format 20.1.8

Command:

```bash
docker run --rm \
  -v "$PWD:/workspace:ro" \
  -w /workspace \
  silkeh/clang@sha256:10854c9a1b4b8fa550b7c2261d298d9d078f5b89aebd50e7576d8599419cbbfd \
  bash -euxo pipefail -c '
    clang-format --version | grep -F "version 20.1.8"
    cmake --version | grep -F "cmake version 3.25.1"
    clang++ --version | grep -F "clang version 20.1.8"
    clang-format --dry-run --Werror \
      firmware/components/audio_source/include/quiet_trace/audio_source.hpp \
      firmware/components/domain/aggregate_contract.cpp \
      firmware/components/domain/include/quiet_trace/aggregate_contract.hpp \
      firmware/host_tests/aggregate_contract_test.cpp \
      firmware/host_tests/support/synthetic_sample_source.hpp \
      firmware/host_tests/synthetic_source_test.cpp \
      firmware/main/quiet_trace_main.cpp
    cmake -S firmware/host_tests -B /tmp/quiet-trace-host-tests \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++
    cmake --build /tmp/quiet-trace-host-tests --parallel
    ctest --test-dir /tmp/quiet-trace-host-tests --output-on-failure
  '
```

Observed result:

- Formatting check completed with exit code 0.
- Release build completed with warnings treated as errors.
- CTest: 2/2 tests passed (`aggregate_contract`, `synthetic_source`).
- The deterministic sample source is compiled only by host tests and is not registered with ESP-IDF.

## ESP-IDF target scaffold

Command (source mounted read-only; build performed inside container `/tmp`):

```bash
docker run --rm \
  -v "$PWD/firmware:/src:ro" \
  espressif/idf@sha256:81893c71bb5e570088901f21def8684c25cd2a9020281bd01b843a7655edb18c \
  bash -lc 'cp -a /src /tmp/firmware && cd /tmp/firmware && \
    idf.py --version && idf.py set-target esp32s3 && idf.py build'
```

Observed result:

- Toolchain reported `ESP-IDF v6.1`.
- ESP32-S3 image generation succeeded.
- `quiet_trace.bin` size: `0x29810` bytes.
- Smallest app partition: `0x100000` bytes; ESP-IDF reported `0xd67f0` bytes (84%) free.
- ESP-IDF emitted upstream Kconfig notes about invalid boolean defaults and duplicate rename mappings plus component-validation warnings about include dependencies inside ESP-IDF itself. They did not fail the build, and no warning/error pointed to Quiet Trace project source.

## Workflow and source hygiene

- `.github/workflows/ci.yml` parsed as YAML and contains three jobs.
- Checkout/setup actions and both firmware containers are pinned to full immutable SHAs/digests; CI asserts the host Clang/CMake versions.
- Dashboard CI includes `npm audit --audit-level=high` before format/lint/type/test/build gates.
- `git diff --check` passed.
- The staged generated-artifact guard found no `node_modules`, `dist`, `coverage`, or build outputs.
- Added-line scan found no hardcoded secret assignment, dynamic eval/exec, shell execution, or unsafe HTML assignment.

## Limits and pending evidence

- GitHub Actions results are pending until the PR is pushed. Local success is not represented as hosted CI success.
- No controller or microphone MPN is selected; no microphone driver or production synthetic source exists.
- No KiCad project, schematic, PCB, BOM source of truth, datasheet verification, ERC/DRC, or fabrication output is part of issue #1. Those are dependency-ordered work beginning with issue #2.
- No device was flashed, assembled, calibrated, bench tested, field observed, or certified.
- No microphone framing, RF, acoustic enclosure, flash endurance/power-loss, USB electrical, Wi-Fi/browser-platform, or reference-meter claim is supported by this evidence.
