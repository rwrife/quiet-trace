import { describe, expect, it } from "vitest";
import {
  ProtocolSchemaError,
  parseBackupPayload,
  parseDeviceStatus,
  parseRecordsPage,
  parseSessionsResponse,
} from "../src/contracts/device-protocol";

const aggregateFixture = {
  schema: "quiet-trace/aggregate/v1",
  sequence: 10,
  interval_start: null,
  monotonic_start_ms: 120000,
  duration_ms: 60000,
  level_eq_dbfs: -30,
  peak_125ms_dbfs: -20,
  histogram_counts: [0, 1, 2, 3, 4, 5, 6, 7, 8, 444],
  quality_flags: ["uncalibrated"],
  calibration: {
    state: "uncalibrated",
    offset_db: null,
    calibrated_at: null,
    method: null,
    reference_instrument: null,
    reference_placement: null,
    reference_source: null,
    reference_duration_s: null,
    firmware_version: null,
    hardware_revision: null,
  },
  clock_quality: "monotonic_only",
  session_id: "fixture-office",
} as const;

describe("device protocol contracts", () => {
  it("accepts status payloads with additive fields", () => {
    const parsed = parseDeviceStatus({
      schema: "quiet-trace/status/v1",
      records: 42,
      generation: 8,
      setup_active: false,
      calibration_state: "reference_adjusted",
      clock_quality: "host_set",
      fixture_mode: false,
      counters: {
        clipping_samples: 1,
        framing_error_samples: 2,
        acquisition_overruns: 0,
        dropped_intervals: 0,
        clock_uncertain_intervals: 1,
        valid_samples: 1234,
        dropped_samples: 5,
      },
      firmware_rev: "v0.7.1",
    });

    expect(parsed.extensions).toEqual({ firmware_rev: "v0.7.1" });
    expect(parsed.records).toBe(42);
  });

  it("rejects status schema mismatch", () => {
    expect(() =>
      parseDeviceStatus({
        schema: "quiet-trace/status/v2",
        records: 1,
        generation: 1,
        setup_active: false,
        calibration_state: "uncalibrated",
        clock_quality: "unknown",
        fixture_mode: true,
        counters: {
          clipping_samples: 0,
          framing_error_samples: 0,
          acquisition_overruns: 0,
          dropped_intervals: 0,
          clock_uncertain_intervals: 0,
          valid_samples: 1,
          dropped_samples: 0,
        },
      }),
    ).toThrow(ProtocolSchemaError);
  });

  it("enforces paging invariants", () => {
    expect(() =>
      parseRecordsPage({
        schema: "quiet-trace/records-page/v1",
        records: [],
        paging: {
          after: null,
          next_after: null,
          limit: 20,
          has_more: true,
        },
        generated_at: null,
        clock_quality: "unknown",
        quality_flags: [],
      }),
    ).toThrow(/next_after must be integer/);
  });

  it("parses records pages with additive fields", () => {
    const parsed = parseRecordsPage({
      schema: "quiet-trace/records-page/v1",
      records: [aggregateFixture],
      paging: {
        after: null,
        next_after: null,
        limit: 1,
        has_more: false,
      },
      generated_at: null,
      clock_quality: "monotonic_only",
      quality_flags: ["wall_time_partial"],
      trace_id: "abc123",
    });

    expect(parsed.extensions).toEqual({ trace_id: "abc123" });
    expect(parsed.records).toHaveLength(1);
  });

  it("rejects forbidden backup fields", () => {
    expect(() =>
      parseBackupPayload({
        schema: "quiet-trace/export/v1",
        exported_at: "2026-09-07T12:00:00Z",
        source: "fixture",
        records: [aggregateFixture],
        sessions: [],
        annotations: [],
        wifi_password: "supersecret",
      }),
    ).toThrow(/Forbidden field/);
  });

  it("parses sessions response", () => {
    const parsed = parseSessionsResponse({
      schema: "quiet-trace/sessions/v1",
      sessions: [
        {
          session_id: "session_alpha",
          label: "Session alpha",
          records: 2,
          interval_start: "2026-09-07T12:00:00Z",
          interval_end: "2026-09-07T12:02:00Z",
          level_eq_avg_dbfs: -31.2,
          peak_dbfs: -14.2,
          calibration_state: "uncalibrated",
          clock_quality: "host_set",
          quality_flags: ["uncalibrated"],
        },
      ],
    });

    expect(parsed.sessions[0]?.session_id).toBe("session_alpha");
  });
});
