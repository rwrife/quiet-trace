# Revision-A bring-up checklist (issue #6)

Use this checklist on a real board during first power-up and integration.

- Hardware revision: `Rev A`
- Firmware tree: `firmware/`
- Dashboard tree: `app/`
- Bench log template: `docs/integration/bench-log-template.md`

> Do not backfill this checklist with simulated values. If a measurement is unavailable, mark it `NOT_MEASURED` with reason.

## 1) Pre-power continuity and resistance (unpowered)

Record measurements with USB disconnected:

| Check | Probe points | Expected | Tolerance / decision |
|---|---|---|---|
| VBUS short check | J1 VBUS ↔ GND | Not shorted | Fail if near 0 Ω |
| Protected rail short check | TP1 ↔ TP3 | Not shorted | Fail if near 0 Ω |
| 3V3 short check | TP2 ↔ TP3 | Not shorted | Fail if near 0 Ω |
| EN idle pull-up path | TP9 ↔ TP2 | Pull-up present | Confirm no hard short to GND |
| BOOT0 idle pull-up path | TP10 ↔ TP2 | Pull-up present | Confirm no hard short to GND |
| USB data pair isolation | TP7 ↔ TP8 | Not shorted | Fail if short |

## 2) First power-up rail/current checks

Start from current-limited source (or USB power meter + known-good host cable):

| Step | Measurement point | Expected behavior |
|---|---|---|
| Apply 5 V | TP1 vs TP3 | Protected rail present near USB input |
| Regulator output | TP2 vs TP3 | 3.3 V rail present and stable |
| Inrush + steady current | USB input current | Within planned prototype envelope; no runaway heating |
| Thermal spot check | U2 package temp | No unsafe rise during idle/logging |

Record instrument model, range, and measured values in bench log.

## 3) Flash + recovery path

1. Build/flash firmware using pinned ESP-IDF flow.
2. Confirm normal boot and serial logs.
3. Force recovery entry: hold BOOT0, tap RESET, retry flash.
4. Run factory erase flow and reflash to confirm recoverability.

Expected outcomes:

- Flash succeeds without repeated transport faults.
- Recovery sequence works when normal bootloader entry fails.
- Erase + reflash returns to known-good boot.

## 4) USB serial control-plane checks (aggregate-only)

After opening a serial console, verify command surface:

```text
setup begin
time set <token> 2026-09-08T12:00:00Z
wifi set <token> <ssid> <password>
export json
export csv
erase records <token>
erase factory <token>
reboot recovery <token>
```

Checklist:

- setup window requires physical confirmation for privileged actions.
- Secrets/tokens are never echoed in cleartext logs or exports.
- `export json|csv` returns aggregate-only payloads.
- erase actions enforce authentication + physical confirmation constraints.

## 5) Microphone framing + counters

Probe TP4/TP5/TP6 and verify:

- I2S framing observed (`BCLK`, `WS`, data activity on `SD`).
- Status reports include clipping/overrun counters.
- Forced overload/quiet transitions change aggregate metrics without persisting raw frames.

## 6) Button/LED semantics

- `SW1` (RESET): device reset behavior and EN line response at TP9.
- `SW2` (SETUP/MARK/BOOT): setup hold entry + mark behavior after boot.
- LED patterns must remain interpretable by timing/pattern, not color alone.

## 7) Offline + fallback behavior

- With Wi-Fi unavailable, logging continues locally.
- USB serial export remains available.
- Device setup/recovery remains possible without internet.

## 8) Storage and erase behavior

- Fill ring with test aggregates until wrap behavior is observed.
- Validate export-before-erase flow.
- Validate erase records and erase factory behavior.
- Simulate interrupted write/power cycle where feasible and confirm recovery path.

## 9) Privacy/secret leakage inspection

Inspect all observable outputs during end-to-end run:

- Flash content schema
- HTTP JSON/SSE responses
- USB serial output
- Dashboard exports (JSON/CSV)
- Diagnostic logs

Fail criteria:

- Any raw/encoded/reconstructable audio payload appears.
- Wi-Fi credentials, auth secrets, or setup tokens leak in export/log channels.

## 10) Evidence classification requirement

Every recorded result in the issue-#6 report must be tagged:

- `STATIC_ANALYSIS`
- `SIMULATION_FIXTURE`
- `BENCH_MEASUREMENT`
- `FIELD_OBSERVATION` (optional)
