# Issue #4 firmware aggregate vertical slice verification

Date: 2026-09-06

Evidence class: **static analysis + synthetic fixture**.

This report does **not** claim bench or field evidence. No physical microphone, enclosure, or browser-on-device checks were run here.

## Scope covered

- Aggregate-domain firmware slice implementation (acquisition math, privacy boundaries, control plane, ring persistence model).
- Toolchain/build reproducibility checks.
- Host tests for deterministic vectors, fault injection, migration/recovery, and command/auth boundaries.

## Commands and observed results

### 1) GitHub auth and write-scope preflight

```bash
cd /home/rwrife/repos/quiet-trace
gh api user --jq .login
```
Observed: `rwrife`

```bash
cd /home/rwrife/repos/quiet-trace
SHA=$(gh api repos/rwrife/quiet-trace/commits/main --jq .sha)
PROBE=hermes-write-probe-$(date +%s)
gh api -X POST repos/rwrife/quiet-trace/git/refs -f ref="refs/heads/$PROBE" -f sha="$SHA"
gh api -X DELETE repos/rwrife/quiet-trace/git/refs/heads/$PROBE
```
Observed: create + delete both succeeded (effective write permission confirmed).

### 2) C/C++ formatting gate (pinned clang image)

```bash
cd /home/rwrife/repos/quiet-trace-issue-4
docker run --rm -v "$PWD:/workspace:ro" -w /workspace \
  silkeh/clang@sha256:10854c9a1b4b8fa550b7c2261d298d9d078f5b89aebd50e7576d8599419cbbfd \
  bash -euxo pipefail -c '
    clang-format --version | grep -F "version 20.1.8"
    clang-format --dry-run --Werror \
      firmware/components/audio_source/include/quiet_trace/audio_source.hpp \
      firmware/components/domain/aggregate_contract.cpp \
      firmware/components/domain/acquisition_pipeline.cpp \
      firmware/components/domain/record_ring.cpp \
      firmware/components/domain/control_plane.cpp \
      firmware/components/domain/include/quiet_trace/aggregate_contract.hpp \
      firmware/components/domain/include/quiet_trace/acquisition_pipeline.hpp \
      firmware/components/domain/include/quiet_trace/record_ring.hpp \
      firmware/components/domain/include/quiet_trace/control_plane.hpp \
      firmware/host_tests/aggregate_contract_test.cpp \
      firmware/host_tests/acquisition_pipeline_test.cpp \
      firmware/host_tests/record_ring_test.cpp \
      firmware/host_tests/control_plane_test.cpp \
      firmware/host_tests/support/synthetic_sample_source.hpp \
      firmware/host_tests/synthetic_source_test.cpp \
      firmware/main/quiet_trace_main.cpp
  '
```
Observed:
- `Debian clang-format version 20.1.8 ...`
- command exited `0` (no format violations).

### 3) Host firmware tests

```bash
cd /home/rwrife/repos/quiet-trace-issue-4
cmake -S firmware/host_tests -B /tmp/quiet-trace-host-tests-issue4 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/g++
cmake --build /tmp/quiet-trace-host-tests-issue4 --parallel
ctest --test-dir /tmp/quiet-trace-host-tests-issue4 --output-on-failure
```
Observed final status:
- `100% tests passed, 0 tests failed out of 5`
- test targets passed: `aggregate_contract`, `synthetic_source`, `acquisition_pipeline`, `record_ring`, `control_plane`.

### 4) Dashboard static + unit + build checks (pinned Node image)

```bash
cd /home/rwrife/repos/quiet-trace-issue-4
docker run --rm -v "$PWD:/workspace" -w /workspace/app node:22.23.1-bookworm-slim bash -euxo pipefail -c '
  node --version | grep -F "v22.23.1"
  npm --version | grep -F "10.9.8"
  npm ci
  npm run format:check
  npm run lint
  npm run test:run
  npm run typecheck
  npm run build
'
```
Observed:
- `v22.23.1`, `10.9.8` matched.
- Vitest: `3 passed`, `58 passed` tests.
- Vite build completed successfully.

### 5) ESP-IDF build check (release-v6.1 image)

First attempt using the historical pinned digest from workflow failed to resolve (`manifest unknown`).
Fallback used the maintained tag `espressif/idf:release-v6.1`.

```bash
cd /home/rwrife/repos/quiet-trace-issue-4
docker run --rm -v "$PWD:/workspace" -w /workspace/firmware espressif/idf:release-v6.1 bash -euxo pipefail -c '
  idf.py --version
  idf.py build
'
```
Observed:
- `ESP-IDF v6.1-520-g629db272933`
- build completed successfully
- artifact: `Generated /workspace/firmware/build/quiet_trace.bin`
- size check: `quiet_trace.bin binary size 0x296f0 bytes ... 84% free`.

## Known evidence limits (explicit)

- No physical board flashing or monitor transcript was captured in this run.
- No hardware microphone framing capture, SPL calibration comparison, or retention soak test was executed.
- No browser-to-device live session was executed.

These remain for issue #6 (bench verification) and later milestones.