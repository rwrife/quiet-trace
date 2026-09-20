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

Flash/monitor commands (still not bench evidence until issue #6):

```bash
idf.py -p "$ESPPORT" flash monitor
```

Factory erase and recovery entry points:

```bash
# wipe flash for recovery or ownership transfer
idf.py -p "$ESPPORT" erase-flash

# recovery flash: hold BOOT, tap RESET, then run
idf.py -p "$ESPPORT" flash monitor
```

USB serial command surface for setup/export/erase/recovery (line-oriented, aggregate-only):

```text
setup begin
time set <token> 2026-09-06T12:00:00Z
wifi set <token> <ssid> <password>
export json|csv
erase records <token>
erase factory <token>
recover store <token>
reboot recovery <token>
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

## Hardware (KiCad)

CI runs the hardware static gate in the official KiCad image pinned by digest:
`kicad/kicad@sha256:e638b79b0321f29395a5b783e94bb9f3c73303e8da15da27b8f5cb4b67a37729`
(`kicad/kicad:9.0.9`, amd64). The immutable digest, not the moving tag, is
authoritative. The job generates a project-local `fp-lib-table` in
`hardware/kicad/` with absolute URIs into the image's packaged footprints via
`hardware/kicad/make_system_lib_tables.py` (gitignored scratch — headless
`kicad-cli` does not receive the GUI-defined `${KICAD*_DIR}` variables the
packaged template tables rely on). Symbol resolution is **not** generated in
CI: the schematic's exact symbol content is vendored in-repo under
`hardware/kicad/libs/symbols/` and referenced by the tracked
`hardware/kicad/sym-lib-table` via `${KIPRJMOD}` relative URIs.

Why symbols are vendored: two distributions of the *same* KiCad 9.0.9 engine
ship different content in the same system libraries. The design was authored
and ERC-verified against the Debian (PPA) build, whose
`RF_Module:ESP32-S3-WROOM-1` names module pads 13/14 `IO19`/`IO20` (the
netlist verifier asserts these pin functions) and whose `Device:LED` has no
`Sim.Pins` property. The official `kicad/kicad` image snapshot renames those
pads `USB_D-`/`USB_D+` and adds `Sim.Pins`, so resolving against image
snapshot libraries emits two spurious `lib_symbol_mismatch` ERC warnings and
would also export different pin names. Vendoring makes the ERC cache check
and the netlist pin names independent of image library snapshots; regeneration
is a reviewed library-uplift action via
`hardware/kicad/vendor_baseline_symbols.py`, never a CI step.

The job then runs:

1. `kicad-cli sch erc` — must report 0 errors and 0 warnings.
2. `kicad-cli sch export netlist` (kicadxml and kicadsexpr) into `ci-scratch/`.
3. `python3 hardware/kicad/verify_netlist.py` and `verify_pcb.py` (stdlib-only pad/net cross-checks).
4. `kicad-cli pcb drc` — expected to exit non-zero while the documented revision-A exceptions are open; `python3 hardware/kicad/verify_drc_baseline.py` then compares the report signature against `hardware/kicad/drc-exceptions-baseline.json`. Any new, removed, or moved violation fails the gate.
5. `python3 hardware/kicad/export_bom.py` and a byte comparison of the export against the tracked `bom/bom.csv`.

The DRC baseline JSON is an exception ledger for the USB-C fanout layout pass
documented in `docs/reports/issue-6-drc-geometry-analysis-2026-09-12.md`; it may
only be regenerated as part of that layout pass with review, never simply to
make CI green.

Local runs require Docker (or Podman) with the `kicad-cli` entry point, or a
native KiCad 9 install; the repo-local `kicad-cli` helper wrapper used during
early issue work is not a CI dependency. KiCad 9.0.9 is the baseline engine:
9.0.2 emits two additional `lib_symbol_mismatch` ERC warnings and 9.0.6 emits
four additional `hole_clearance` DRC violations, so pinning matters.

## Evidence limit

A successful host, app, or ESP-IDF build is static/synthetic evidence. It is not proof of a selected microphone, fabricated PCB, audio framing, RF/acoustic behavior, calibration, USB electrical behavior, flash endurance, or physical bring-up.
