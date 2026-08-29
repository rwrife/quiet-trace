# Aggregate metric and record contract

Status: **foundation contract v1 proposed for review** in issue #1. Algorithm implementation and physical validation belong to later issues.

## Hard boundary

A `sample` is a signed microphone amplitude value held only in a bounded volatile acquisition/DSP buffer. Samples, sample windows, encoded audio, waveform fragments, ordered spectral coefficients, speech/transcripts, speaker identity, and source labels are forbidden outside that boundary. Intermediate per-window energies are volatile and discarded after the minute record is finalized.

## Terminology

- **Processing block:** a bounded implementation-sized group of samples. Its exact size and microphone framing follow the selected MPN datasheets.
- **Valid sample:** a correctly framed sample not discarded for an acquisition overrun or invalid clock interval.
- **125 ms window:** a contiguous, non-overlapping set of valid weighted samples. Partial windows are excluded and flagged.
- **Minute interval:** exactly 60,000 monotonic milliseconds. It may have no trustworthy wall time.
- **Relative level (`dBFS`):** energy ratio to digital full scale. It is not sound pressure level.
- **Reference-adjusted level:** relative level plus a documented single-point offset. It is still not a certified or standards-compliant measurement.

## DSP definition

Let normalized full-scale samples after DC blocking and the selected weighting profile be `x[n]`, bounded to `[-1, 1]`. Let `epsilon = 1e-20` prevent logarithm of zero.

For a set of `N` valid samples, mean-square energy is:

`E = (sum(x[n]^2) / N)`

and relative level is:

`L_dbfs = 10 * log10(max(E, epsilon))`.

The persisted fields are:

- `level_eq_dbfs`: energy average over every valid sample in the 60-second interval, not the arithmetic mean of decibel values.
- `peak_125ms_dbfs`: maximum `L_dbfs` among complete non-overlapping 125 ms windows in the interval.
- `histogram_counts`: counts of complete 125 ms windows in ten half-open dBFS bins with edges `[-100, -90, -80, -70, -60, -50, -40, -30, -20, -10, 0]`; values below -100 enter the first bin, exactly 0 enters the final bin, and values above 0 are invalid/clipped.

The histogram discards temporal order and is level-distribution metadata, not a spectrum. No one-second or 125 ms time series is persisted.

## Weighting profile

Revision A targets `mvp_a_approx_v1`: a digital approximation generated from the public nominal A-weighting transfer curve after the sample rate is fixed. This is an engineering approximation, not an IEC/ANSI conformance claim.

Before firmware may label a record with this profile, deterministic frequency fixtures must show its implemented response is within:

- ±0.5 dB of the generated digital reference from 31.5 Hz through 8 kHz; and
- ±1.0 dB of that reference from 20 Hz through 12.5 kHz.

Those limits compare code to its digital design target only. They do not include microphone, enclosure, calibration, free-field/diffuse-field, time-weighting, or reference-instrument error. If the filter or sample rate is not validated, firmware must fail closed to a clearly labeled unweighted relative profile; it must not emit `dBA`, `LAeq`, or certified-meter wording.

## Calibration states

- `uncalibrated`: default. `offset_db`, date, and method are null. UI/export says “uncalibrated relative level.”
- `reference_adjusted`: a documented reference-meter comparison supplies a finite offset, timestamp, method, reference instrument, placement, source, duration, and firmware/hardware revision. UI/export says “reference-adjusted estimate,” not “calibrated meter.”
- `invalid`: stored calibration metadata is incomplete, outside configured sanity bounds, incompatible with the active weighting/sample profile, or fails integrity checks. No adjusted value is shown.

A single-point offset changes level alignment only. It does not establish frequency weighting accuracy, linearity, overload behavior, directionality, or standards compliance.

## Clock quality

- `unknown`: no trusted wall time and monotonic continuity is not established.
- `monotonic_only`: sequence and monotonic interval order are valid; wall time is null.
- `host_set`: wall time was set locally but is not currently synchronized.
- `synced`: time was synchronized to the configured local/approved source within the implementation’s freshness limit.

Wall time may be absent. `sequence` is the authoritative ordering key and never decreases within a store generation. Factory erase creates a new store generation/device epoch rather than pretending continuity.

## Quality flags

The v1 vocabulary is additive and includes: `uncalibrated`, `calibration_invalid`, `clipped`, `acquisition_overrun`, `dropped_interval`, `insufficient_samples`, `wall_time_unknown`, `clock_adjusted`, `reset_recovery`, and `weighting_unvalidated`. Unknown flags must be preserved by exports and tolerated by clients.

An interval with no valid samples is not represented by fabricated numeric values; it is recorded as a missing interval/status event with reason. Partial intervals may be stored only with `insufficient_samples` and explicit valid-sample coverage metadata added in a compatible schema revision.

## Retention and bounds

- Default persistence cadence: at most one record per 60 seconds.
- Minimum capacity target: 7 days = 10,080 records.
- Hard planning size ceiling: 256 serialized bytes per record, yielding 2,580,480 bytes (2.461 MiB) before storage overhead.
- Annotation text is stored separately, UTF-8, user-entered, and bounded to 120 Unicode scalar values per annotation and 16 annotations per minute interval.
- Live display may receive a bounded aggregate more frequently, but never raw samples or an ordered high-resolution level series suitable for audio reconstruction. The exact live cadence is fixed with the API implementation and threat-tested before release.

The canonical machine-readable record shape is [`schemas/aggregate-record-v1.schema.json`](schemas/aggregate-record-v1.schema.json). Draft 2020-12 alone cannot express the histogram-total or peak-versus-equivalent cross-field rules, so consumers must also run a declared semantic validator; see [`schemas/README.md`](schemas/README.md).
