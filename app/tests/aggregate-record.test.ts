import { describe, expect, it } from "vitest";

import {
  AggregateRecordSchemaError,
  parseAggregateRecord,
} from "../src/contracts/aggregate-record";
import { aggregateFixture } from "../src/fixtures/aggregate";

describe("aggregate-only application boundary", () => {
  it("accepts the deterministic aggregate fixture", () => {
    expect(parseAggregateRecord(aggregateFixture)).toEqual(aggregateFixture);
  });

  it.each([
    ["samples", [1, 2, 3]],
    ["pcm", "AAAA"],
    ["audio", "encoded"],
    ["encoded_media", "encoded"],
    ["recording", "encoded"],
    ["waveform", [0.1, 0.2]],
    ["spectrum", [0.1, 0.2]],
    ["fft_bins", [0.1, 0.2]],
    ["transcript", "private speech"],
    ["speech", "private speech"],
    ["voice_activity", true],
    ["speaker_id", "person-1"],
    ["source_label", "appliance"],
    ["event_label", "door"],
    ["wifi_password", "not-for-export"],
    ["auth_token", "not-for-export"],
  ])("rejects forbidden field %s", (field, value) => {
    expect(() =>
      parseAggregateRecord({ ...aggregateFixture, [field]: value }),
    ).toThrow(AggregateRecordSchemaError);
  });

  it("rejects unknown fields even when their names look harmless", () => {
    expect(() =>
      parseAggregateRecord({ ...aggregateFixture, future_value: 1 }),
    ).toThrow(AggregateRecordSchemaError);
  });

  it("rejects unknown nested calibration fields", () => {
    expect(() =>
      parseAggregateRecord({
        ...aggregateFixture,
        calibration: { ...aggregateFixture.calibration, comment: "extra" },
      }),
    ).toThrow(AggregateRecordSchemaError);
  });

  it("requires 480 valid 125 ms windows in a complete minute", () => {
    expect(() =>
      parseAggregateRecord({
        ...aggregateFixture,
        histogram_counts: [0, 0, 1, 8, 24, 19, 7, 1, 0, 0],
      }),
    ).toThrow(AggregateRecordSchemaError);
  });

  it("rejects a peak below the interval equivalent level", () => {
    expect(() =>
      parseAggregateRecord({ ...aggregateFixture, peak_125ms_dbfs: -40 }),
    ).toThrow(AggregateRecordSchemaError);
  });

  it("accepts a valid UTC leap-day timestamp with fractional seconds", () => {
    expect(
      parseAggregateRecord({
        ...aggregateFixture,
        interval_start: "2024-02-29T23:59:59.123Z",
        clock_quality: "host_set",
      }).interval_start,
    ).toBe("2024-02-29T23:59:59.123Z");
  });

  it.each([
    ["interval_start", "not-a-date"],
    ["interval_start", "2026-02-29T12:00:00Z"],
    ["interval_start", "2026-08-28T12:00:00.Z"],
    ["interval_start", "0000-01-01T00:00:00Z"],
    ["clock_quality", "gps_magic"],
    ["quality_flags", ["UPPER CASE"]],
    ["session_id", "x".repeat(65)],
  ])("rejects invalid %s", (field, value) => {
    const base =
      field === "interval_start"
        ? { ...aggregateFixture, clock_quality: "host_set" as const }
        : aggregateFixture;
    expect(() => parseAggregateRecord({ ...base, [field]: value })).toThrow(
      AggregateRecordSchemaError,
    );
  });

  it("accepts complete reference-adjusted calibration provenance", () => {
    const parsed = parseAggregateRecord({
      ...aggregateFixture,
      calibration: {
        state: "reference_adjusted",
        offset_db: 42,
        calibrated_at: "2026-08-28T12:00:00Z",
        method: "side-by-side comparison",
        reference_instrument: "traceable reference meter asset QT-REF-01",
        reference_placement: "capsules adjacent at desk position",
        reference_source: "steady broadband calibration signal",
        reference_duration_s: 300,
        firmware_version: "0.1.0",
        hardware_revision: "unbuilt-fixture",
      },
    });

    expect(parsed.calibration.state).toBe("reference_adjusted");
  });

  it("requires complete reference-adjusted calibration metadata", () => {
    expect(() =>
      parseAggregateRecord({
        ...aggregateFixture,
        calibration: {
          state: "reference_adjusted",
          offset_db: 42,
          calibrated_at: null,
          method: null,
        },
      }),
    ).toThrow(AggregateRecordSchemaError);
  });

  it("rejects mutable or out-of-range histogram data", () => {
    expect(() =>
      parseAggregateRecord({
        ...aggregateFixture,
        histogram_counts: [1, -1, 2],
      }),
    ).toThrow(AggregateRecordSchemaError);
  });
});
