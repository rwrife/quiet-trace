# Reference-meter comparison and calibration procedure (issue #6)

This procedure defines how to compare Quiet Trace aggregate output against a reference sound-level meter and how to store a single-point calibration offset.

> Quiet Trace remains **non-certified**. Calibration offset alignment is not the same as weighting/network compliance. Do not claim IEC/ANSI certification, occupational dose fitness, or legal/medical suitability.

## 1) Required artifacts per run

Record all of the following in `docs/integration/bench-log-template.md`:

- Board revision and serial/identifier
- Firmware commit hash and build command
- Dashboard/app commit hash and build command
- Reference meter make/model/serial, calibration date, weighting/time settings
- Microphone/reference placement geometry and distance
- Enclosure state (open board vs enclosure variant)
- Ambient conditions and test stimuli summary
- Exact timestamps and timezone

## 2) Test setup

1. Place Quiet Trace and reference meter microphones in repeatable geometry.
2. Select one weighting/time behavior on the reference meter and record it.
3. Ensure Quiet Trace is marked uncalibrated before first comparison.
4. Run at least two conditions:
   - steady mid-level source
   - transient or changing source to exercise peak/histogram behavior

## 3) Data capture sequence

For each condition:

1. Start logging.
2. Capture:
   - Quiet Trace aggregate snapshot (`level_eq_dbfs`, peak, histogram, counters)
   - Reference meter reading and settings
3. Repeat at least three times for repeatability.
4. Record clipping/overrun counters each pass.

## 4) Offset derivation and application

1. Compute a single offset between Quiet Trace aggregate baseline and reference meter reading under the chosen setup.
2. Store offset as metadata with:
   - method
   - date/time
   - reference meter identity
   - firmware + hardware revision
3. Mark status as calibrated only when that metadata is present.

## 5) Repeatability + enclosure/port comparison

Run the same capture sequence across:

- bare board
- enclosure variant A
- enclosure variant B (if applicable)

Document differences and whether microphone port changes alter results beyond expected tolerance.

## 6) Required limitations statement

Every report using this calibration flow must state:

- calibration is setup-specific and revision-specific
- no certified-meter claim
- no hearing-safety, occupational, legal, medical, or surveillance claim
- unavailable bench data remains explicitly `NOT_MEASURED`
