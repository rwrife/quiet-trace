# Issue #6 bench evidence log template

Use one copy of this template per physical run. Do not enter simulated values.

## Run metadata

- Date/time (local + UTC):
- Operator:
- Board revision / identifier:
- Firmware commit + build ID:
- Dashboard commit + build ID:
- Enclosure state (bare / variant name):
- USB supply / cable:

## Instruments

| Instrument | Make/model | Serial | Last calibration date | Usage |
|---|---|---|---|---|
| DMM |  |  |  | Rail/continuity |
| USB power meter |  |  |  | Input current/voltage |
| Logic analyzer/scope |  |  |  | I2S/USB framing |
| Reference SLM |  |  |  | Comparison/calibration |

## Pre-power checks (`BENCH_MEASUREMENT`)

| Check | Value | Unit | Tolerance | Pass/fail | Notes |
|---|---:|---|---|---|---|
| VBUS↔GND resistance |  | Ω | not short |  |  |
| TP1↔TP3 resistance |  | Ω | not short |  |  |
| TP2↔TP3 resistance |  | Ω | not short |  |  |
| TP7↔TP8 resistance |  | Ω | not short |  |  |

## Rail/current/thermal checks (`BENCH_MEASUREMENT`)

| Check | Value | Unit | Tolerance | Pass/fail | Notes |
|---|---:|---|---|---|---|
| TP1 voltage |  | V | expected protected VBUS |  |  |
| TP2 voltage |  | V | expected 3.3 V rail |  |  |
| Idle current |  | mA | planned envelope |  |  |
| Logging current |  | mA | planned envelope |  |  |
| U2 surface temp |  | °C | no unsafe rise |  |  |

## Flash/recovery checks (`BENCH_MEASUREMENT`)

Commands executed:

```text
idf.py set-target esp32s3
idf.py build
idf.py -p <port> flash monitor
idf.py -p <port> erase-flash
```

Results:

-

## USB control-plane checks (`BENCH_MEASUREMENT`)

Command/result table:

| Command | Result | Secret redaction verified? | Notes |
|---|---|---|---|
| setup begin |  |  |  |
| time set <token> <iso8601> |  |  |  |
| wifi set <token> <ssid> <password> |  | yes/no |  |
| export json |  | n/a |  |
| export csv |  | n/a |  |
| erase records <token> |  | n/a |  |
| erase factory <token> |  | n/a |  |
| reboot recovery <token> |  | n/a |  |

## End-to-end privacy inspection (`BENCH_MEASUREMENT`)

Observed channels:

- flash store
- HTTP JSON/SSE
- USB serial
- exported CSV/JSON
- logs

Forbidden-field scan outcome:

- raw/encoded/reconstructable audio present? yes/no
- secret leakage present? yes/no
- details:

## Calibration/reference comparison (`BENCH_MEASUREMENT`)

Reference meter settings:

- Weighting:
- Time:
- Placement geometry:

| Trial | QT aggregate reading | Reference reading | Offset | Clipping/overrun counters | Notes |
|---|---:|---:|---:|---|---|
| 1 |  |  |  |  |  |
| 2 |  |  |  |  |  |
| 3 |  |  |  |  |  |

Derived offset metadata stored? yes/no

## Evidence class summary

- STATIC_ANALYSIS:
- SIMULATION_FIXTURE:
- BENCH_MEASUREMENT:
- FIELD_OBSERVATION (optional):

## Unavailable evidence

List any `NOT_MEASURED` items and exact blocker.
