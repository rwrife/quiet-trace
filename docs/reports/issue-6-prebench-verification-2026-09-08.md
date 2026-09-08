# Issue #6 pre-bench integration packet verification (2026-09-08)

Related issue: <https://github.com/rwrife/quiet-trace/issues/6>

## Scope and evidence class

This report captures **static analysis** and **simulation/fixture** checks that can be executed without physical hardware, plus publication of revision-A assembly/bring-up/calibration/troubleshooting documentation.

- `STATIC_ANALYSIS`: app formatting/lint/type/build checks, firmware host tests, ESP-IDF target compile, KiCad ERC/netlist/PCB cross-check, BOM export reproducibility.
- `SIMULATION_FIXTURE`: existing Vitest fixture/contract tests.
- `BENCH_MEASUREMENT`: **not performed in this report**.
- `FIELD_OBSERVATION`: not performed.

No bench readings are fabricated in this document.

## Commands executed and outcomes

All commands were run from repository root `/home/rwrife/repos/.worktrees/quiet-trace-issue-6` unless noted.

| Command | Result |
|---|---|
| `cd app && node --version && npm --version` | ✅ `v22.23.1`, `10.9.8` |
| `cd app && npm ci` | ✅ added 156 packages, 0 vulnerabilities |
| `cd app && npm run format:check` | ✅ `All matched files use Prettier code style!` |
| `cd app && npm run lint` | ✅ pass |
| `cd app && npm run typecheck` | ✅ pass |
| `cd app && npm run test:run` | ✅ `8 passed` files, `74 passed` tests |
| `cd app && npm run test:accessibility` | ✅ `1 passed` file, `2 passed` tests |
| `cd app && npm run build` | ✅ Vite build; JS gzip `12.50 kB`, CSS gzip `1.16 kB` |
| `cd app && npm run check:bundle-size` | ✅ `TOTAL gzip=13591 bytes` |
| `cd app && npm run check:firmware-contract` | ✅ `Firmware control-plane contract check passed.` |
| `cmake -S firmware/host_tests -B /tmp/quiet-trace-host-tests-issue6 -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/quiet-trace-host-tests-issue6 --parallel && ctest --test-dir /tmp/quiet-trace-host-tests-issue6 --output-on-failure` | ✅ `100% tests passed, 0 tests failed out of 5` |
| `docker run ... espressif/idf@sha256:81893c71... idf.py --version && idf.py set-target esp32s3 && idf.py build` | ✅ `ESP-IDF v6.1`; `quiet_trace.bin` built, size `0x29820`, partition free `84%` |
| `kicad-cli sch erc --exit-code-violations --severity-error --severity-warning --format report -o analysis/issue-6-prebench/quiet-trace-erc.rpt hardware/kicad/quiet-trace.kicad_sch` | ✅ `Found 0 violations` |
| `kicad-cli sch export netlist --format kicadxml -o hardware/kicad/quiet-trace.xml hardware/kicad/quiet-trace.kicad_sch` | ✅ pass |
| `kicad-cli sch export netlist --format kicadsexpr -o hardware/kicad/quiet-trace.net hardware/kicad/quiet-trace.kicad_sch` | ✅ pass |
| `python3 hardware/kicad/verify_netlist.py hardware/kicad/quiet-trace.xml` | ✅ `11 components, 21 nets, 41 pin functions, 30 explicit no-connects` |
| `python3 hardware/kicad/verify_pcb.py hardware/kicad/quiet-trace.xml hardware/kicad/quiet-trace.kicad_pcb` | ✅ `34 schematic components, 97 pin/net assignments, 38 PCB footprints, 6 board keepouts` |
| `python3 hardware/kicad/export_bom.py hardware/kicad/quiet-trace.xml bom/bom.csv` | ✅ `29 lines, 24 purchased components, MPN coverage 24/24` |
| `python3 hardware/kicad/export_bom.py ... /tmp/bom-repro.csv && cmp -s bom/bom.csv /tmp/bom-repro.csv` | ✅ `bom_reproducible=yes` |
| `kicad-cli pcb drc --exit-code-violations --severity-error --severity-warning --format report -o analysis/issue-6-prebench/quiet-trace-drc.rpt hardware/kicad/quiet-trace.kicad_pcb` | ⚠️ exit code `5` (4 violations + 1 unconnected item) |

## DRC exceptions captured (not resolved here)

From `docs/reports/issue-6-prebench-drc-2026-09-08.rpt`:

- `solder_mask_bridge` between `VBUS_RAW` track and `J1` shield pad `S1 [GND]`
- `courtyards_overlap` between `R5` and `U2`
- `shorting_items` between `GND` and `VBUS_RAW` at J1/track location
- `clearance` violation between `VBUS_RAW` track and `GND` zone
- one `unconnected_items` entry for F.Cu GND zone

Because this PR only adds issue-#6 integration documentation and verification capture, no PCB geometry/routing was modified here. These DRC findings are preserved as explicit open hardware blockers.

## Published issue-#6 integration docs in this change

- `docs/integration/assembly-guide.md`
- `docs/integration/bring-up-checklist.md`
- `docs/integration/calibration-reference-comparison.md`
- `docs/integration/troubleshooting.md`
- `docs/integration/bench-log-template.md`

These documents provide:

- BOM-linked assembly sequence, ESD/safety checks, orientation checks, and enclosure steps
- test-point map with board coordinates
- first-power/current/rail checks and flashing/recovery flow
- aggregate-only USB/export/privacy leak inspection checklist
- explicit reference-meter comparison method and repeatability expectations
- required evidence class labeling and `NOT_MEASURED` handling

## Not performed in this report (required future bench work)

All of the below remain mandatory for issue #6 closure and are intentionally not simulated:

- real resistance/continuity/rail/current/temperature measurements on assembled hardware
- end-to-end physical microphone → aggregate record → dashboard/export/delete run
- flash/network/USB inspection on physical device for forbidden raw/reconstructable audio leakage and secret leakage
- reference-meter calibration run with logged instrument metadata and date
- enclosure/port A/B comparison and repeatability measurements

## Notes

- An initial host-test attempt reused a build cache from another worktree and failed CMake source-dir validation. Re-run with a unique build directory `/tmp/quiet-trace-host-tests-issue6` passed cleanly.
- Versioned report artifacts committed with this change:
  - `docs/reports/issue-6-prebench-erc-2026-09-08.rpt`
  - `docs/reports/issue-6-prebench-drc-2026-09-08.rpt`
  - `docs/reports/issue-6-prebench-command-status-2026-09-08.tsv`
