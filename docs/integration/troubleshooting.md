# Revision-A troubleshooting guide (issue #6)

Use this with `docs/integration/bring-up-checklist.md` and test-point map in `docs/integration/assembly-guide.md`.

## No power / no boot

Symptoms:

- No LED behavior
- No USB enumeration
- No 3.3 V rail

Checks:

1. Verify USB cable/supply and connector seating.
2. Measure TP1 (USB_5V) to TP3 (GND).
3. Measure TP2 (+3V3) to TP3.
4. Inspect F1, D2, U2 orientation/solder joints.
5. Verify no short between TP2 and TP3.

## Flashing fails

Symptoms:

- `idf.py flash` transport errors
- repeated connection timeout

Checks:

1. Hold BOOT0 (`SW2`), tap RESET (`SW1`), retry flash.
2. Confirm USB D+/D− continuity at TP7/TP8 (no short).
3. Try known-good cable/port.
4. Check J1 solder shell and data pins for bridges.

## Microphone framing missing/invalid

Symptoms:

- no aggregate updates
- framing errors / overruns

Checks:

1. Probe TP4/TP5/TP6 for BCLK/WS/SD activity.
2. Inspect U4 orientation and solder joints.
3. Confirm C5 and R8 placement/value.
4. Check that microphone port is unobstructed.

## Implausible or uncalibrated values

Symptoms:

- values clearly inconsistent with environment
- calibration flag missing/invalid

Checks:

1. Verify device still marked uncalibrated unless valid calibration metadata exists.
2. Re-run reference comparison procedure (`calibration-reference-comparison.md`).
3. Compare clipping/overrun counters for overload conditions.
4. Confirm enclosure/port condition matches calibration setup.

## Network setup unavailable

Symptoms:

- cannot join/setup via Wi-Fi
- setup API unauthorized

Checks:

1. Ensure setup hold window was physically initiated.
2. Retry `setup begin` over USB serial and apply credentials.
3. Confirm credentials are not echoed/logged in plain text.
4. Use USB fallback for export/config while Wi-Fi is unavailable.

## Clock uncertainty / timestamp quality issues

Symptoms:

- records missing trusted wall-clock quality
- jumpy timestamps after reset

Checks:

1. Apply `time set <token> <ISO-8601>` through authenticated path.
2. Confirm status quality fields reflect sync state.
3. Power cycle and verify sequence monotonic behavior.

## Storage corruption / retention anomalies

Symptoms:

- missing records
- checksum or migration warnings

Checks:

1. Run export and inspect record continuity.
2. Validate ring-wrap behavior with controlled fill test.
3. Use `recover store <token>` / recovery path if available.
4. If unresolved, capture logs and execute controlled `erase records` then retest.

## USB recovery path unavailable

Symptoms:

- device unreachable on network and app

Checks:

1. Attach USB serial directly and run control-plane commands.
2. Attempt bootloader recovery (BOOT hold + RESET tap).
3. Reflash known-good firmware and verify boot.

## Factory erase issues

Symptoms:

- erase command rejected unexpectedly
- data persists after erase request

Checks:

1. Confirm valid auth token/session and required physical confirmation path.
2. Retry `erase factory <token>` from USB serial.
3. Reboot and verify state reset + data cleared.
4. Confirm no secret retention in ordinary exports after reset.
