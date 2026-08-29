# Reproducible toolchains

## Dashboard

- Node.js `22.23.1` (`app/.nvmrc` and `package.json` engines)
- npm `10.9.8`
- All JavaScript development dependencies are exact versions in `app/package.json` and integrity-locked by `app/package-lock.json`.

```bash
cd app
npm ci
npm run format:check
npm run lint
npm run typecheck
npm run test:run
npm run build
```

Fixture mode is opt-in at `/?fixture=1` and visibly labels synthetic data. The default app does not fabricate a device connection or sensor values.

## Firmware target

- ESP-IDF `v6.1` (`firmware/.idf-version`)
- Target: generic `esp32s3` until issue #2 selects and verifies the exact controller/module MPN
- CI image: `espressif/idf:v6.1`, pinned in the workflow to repository digest `sha256:81893c71bb5e570088901f21def8684c25cd2a9020281bd01b843a7655edb18c`

### Ubuntu 24.04 native prerequisites

```bash
sudo apt-get update
sudo apt-get install -y \
  bison ccache cmake dfu-util flex g++ gcc git gperf libffi-dev \
  libssl-dev libusb-1.0-0 ninja-build python3 python3-pip \
  python3-setuptools python3-venv wget
```

Install the `v6.1` tag with Espressif’s version-specific Linux instructions:
<https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/get-started/linux-macos-setup.html>.
Do not install from the moving `master` branch. After the documented `install.sh esp32s3` step, each shell must source the checkout’s `export.sh`; verify `idf.py --version` prints `ESP-IDF v6.1` before building.

With that environment exported:

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
```

Flash/monitor commands are intentionally not evidence until exact hardware is selected and attached:

```bash
idf.py -p "$ESPPORT" flash monitor
```

## Firmware host tests

CI runs host checks in `silkeh/clang@sha256:10854c9a1b4b8fa550b7c2261d298d9d078f5b89aebd50e7576d8599419cbbfd`, which currently provides Clang/clang-format 20.1.8 and CMake 3.25.1. The immutable image digest, not the moving tag, is authoritative. Local development requires CMake 3.24+ and a C++20 compiler. The host tests use the real domain source but no microphone hardware or synthetic source in the production target.

```bash
cmake -S firmware/host_tests -B /tmp/quiet-trace-host-tests \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build /tmp/quiet-trace-host-tests --parallel
ctest --test-dir /tmp/quiet-trace-host-tests --output-on-failure
```

## Evidence limit

A successful host, app, or ESP-IDF build is static/synthetic evidence. It is not proof of a selected microphone, fabricated PCB, audio framing, RF/acoustic behavior, calibration, USB electrical behavior, flash endurance, or physical bring-up.
