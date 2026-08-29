import { readFileSync } from "node:fs";

import Ajv2020 from "ajv/dist/2020";
import addFormats from "ajv-formats";
import { describe, expect, it } from "vitest";

import {
  AggregateRecordSchemaError,
  parseAggregateRecord,
} from "../src/contracts/aggregate-record";
import { aggregateFixture } from "../src/fixtures/aggregate";

const schema = JSON.parse(
  readFileSync(
    new URL(
      "../../docs/schemas/aggregate-record-v1.schema.json",
      import.meta.url,
    ),
    "utf8",
  ),
) as object;
const canonicalFixture = JSON.parse(
  readFileSync(
    new URL(
      "../../docs/schemas/fixtures/aggregate-record-v1.json",
      import.meta.url,
    ),
    "utf8",
  ),
) as unknown;

const structuralAjv = new Ajv2020({ allErrors: true, strict: false });
addFormats(structuralAjv);
const validateStructureOnly = structuralAjv.compile(schema);

const ajv = new Ajv2020({ allErrors: true, strict: true });
ajv.addKeyword({
  keyword: "x-quiet-trace-semantic-validation",
  schemaType: "object",
  validate: () => true,
});
ajv.addKeyword({
  keyword: "x-quiet-trace-total",
  type: "array",
  schemaType: "number",
  validate: (expected: number, data: unknown[]) =>
    data.every((entry) => typeof entry === "number") &&
    data.reduce((sum, entry) => sum + (entry as number), 0) === expected,
});
ajv.addKeyword({
  keyword: "x-quiet-trace-peak-gte-equivalent",
  type: "object",
  schemaType: "boolean",
  validate: (enabled: boolean, data: Record<string, unknown>) =>
    !enabled ||
    (typeof data.level_eq_dbfs === "number" &&
      typeof data.peak_125ms_dbfs === "number" &&
      data.peak_125ms_dbfs >= data.level_eq_dbfs),
});
addFormats(ajv);
const validate = ajv.compile(schema);

describe("canonical aggregate JSON Schema", () => {
  it("matches the app fixture and validates it", () => {
    expect(aggregateFixture).toEqual(canonicalFixture);
    expect(validate(canonicalFixture), JSON.stringify(validate.errors)).toBe(
      true,
    );
  });

  it("declares the required semantic validators for non-standard invariants", () => {
    const semanticValidation = (
      schema as {
        "x-quiet-trace-semantic-validation"?: {
          required?: boolean;
          rules?: string[];
          implementations?: Record<string, string>;
        };
      }
    )["x-quiet-trace-semantic-validation"];
    const invalidHistogram = {
      ...aggregateFixture,
      histogram_counts: [0, 0, 1, 8, 24, 19, 7, 1, 0, 0],
    };
    const invalidPeak = { ...aggregateFixture, peak_125ms_dbfs: -40 };

    expect(validateStructureOnly(invalidHistogram)).toBe(true);
    expect(validateStructureOnly(invalidPeak)).toBe(true);
    expect(semanticValidation).toEqual({
      required: true,
      rules: [
        "histogram_total_480",
        "peak_gte_equivalent",
        "valid_unicode_scalar_text",
        "clock_quality_wall_time_consistency",
      ],
      implementations: {
        typescript:
          "app/src/contracts/aggregate-record.ts#parseAggregateRecord",
        cpp: "firmware/components/domain/aggregate_contract.cpp#is_valid",
      },
    });
    expect(() => parseAggregateRecord(invalidHistogram)).toThrow(
      AggregateRecordSchemaError,
    );
    expect(() => parseAggregateRecord(invalidPeak)).toThrow(
      AggregateRecordSchemaError,
    );
  });

  it("rejects unknown top-level fields", () => {
    expect(validate({ ...aggregateFixture, microphone_buffer: [1, 2] })).toBe(
      false,
    );
  });

  it("rejects a histogram that does not total 480 windows", () => {
    expect(
      validate({
        ...aggregateFixture,
        histogram_counts: [0, 0, 1, 8, 24, 19, 7, 1, 0, 0],
      }),
    ).toBe(false);
  });

  it("rejects a peak below the equivalent level", () => {
    expect(validate({ ...aggregateFixture, peak_125ms_dbfs: -40 })).toBe(false);
  });

  it.each([
    "2026-02-29T12:00:00Z",
    "2016-12-31T23:59:60Z",
    "2026-08-28T12:00:00.Z",
    "2026-08-28 12:00:00Z",
    "2026-08-28t12:00:00Z",
    "2026-08-28T12:00:00.1234567890Z",
    "2026-08-28T12:00:00+00:00",
    "0000-01-01T00:00:00Z",
  ])(
    "rejects invalid UTC timestamp %s in both app and schema",
    (interval_start) => {
      const candidate = {
        ...aggregateFixture,
        interval_start,
        clock_quality: "host_set",
      };
      expect(validate(candidate)).toBe(false);
      expect(() => parseAggregateRecord(candidate)).toThrow(
        AggregateRecordSchemaError,
      );
    },
  );

  it.each(["2016-12-31T23:59:60Z", "2026-02-29T12:00:00Z"])(
    "rejects invalid calibration timestamp %s in both app and schema",
    (calibrated_at) => {
      const candidate = {
        ...aggregateFixture,
        calibration: {
          state: "reference_adjusted",
          offset_db: 42,
          calibrated_at,
          method: "side-by-side comparison",
          reference_instrument: "traceable reference meter asset QT-REF-01",
          reference_placement: "capsules adjacent at desk position",
          reference_source: "steady broadband calibration signal",
          reference_duration_s: 300,
          firmware_version: "0.1.0",
          hardware_revision: "unbuilt-fixture",
        },
      };

      expect(validate(candidate)).toBe(false);
      expect(() => parseAggregateRecord(candidate)).toThrow(
        AggregateRecordSchemaError,
      );
    },
  );

  it("counts bounded text in Unicode scalar values across schema and app", () => {
    const validSession = { ...aggregateFixture, session_id: "😀".repeat(64) };
    const invalidSession = { ...aggregateFixture, session_id: "😀".repeat(65) };
    const validMethod = {
      ...aggregateFixture,
      calibration: {
        state: "reference_adjusted",
        offset_db: 42,
        calibrated_at: "2026-08-28T12:00:00Z",
        method: "😀".repeat(240),
        reference_instrument: "reference meter",
        reference_placement: "adjacent capsules",
        reference_source: "broadband source",
        reference_duration_s: 300,
        firmware_version: "0.1.0",
        hardware_revision: "fixture",
      },
    };

    expect(validate(validSession), JSON.stringify(validate.errors)).toBe(true);
    expect(parseAggregateRecord(validSession).session_id).toBe(
      validSession.session_id,
    );
    expect(validate(invalidSession)).toBe(false);
    expect(() => parseAggregateRecord(invalidSession)).toThrow(
      AggregateRecordSchemaError,
    );
    expect(validate(validMethod), JSON.stringify(validate.errors)).toBe(true);
    expect(parseAggregateRecord(validMethod).calibration.method).toBe(
      validMethod.calibration.method,
    );
  });

  it("rejects text containing an unpaired UTF-16 surrogate", () => {
    const invalidUnicode = { ...aggregateFixture, session_id: "\ud800" };

    expect(validate(invalidUnicode)).toBe(true);
    expect(() => parseAggregateRecord(invalidUnicode)).toThrow(
      AggregateRecordSchemaError,
    );
  });

  it.each([
    ["unknown", "2026-08-28T12:00:00Z"],
    ["monotonic_only", "2026-08-28T12:00:00Z"],
    ["host_set", null],
    ["synced", null],
  ])(
    "rejects clock quality %s with inconsistent wall time",
    (clock_quality, interval_start) => {
      const candidate = {
        ...aggregateFixture,
        clock_quality,
        interval_start,
      };

      expect(validate(candidate)).toBe(false);
      expect(() => parseAggregateRecord(candidate)).toThrow(
        AggregateRecordSchemaError,
      );
    },
  );

  it("rejects integers above the JSON safe-integer wire limit", () => {
    expect(
      validate({ ...aggregateFixture, sequence: 9_007_199_254_740_992 }),
    ).toBe(false);
    expect(
      validate({
        ...aggregateFixture,
        monotonic_start_ms: 9_007_199_254_740_992,
      }),
    ).toBe(false);
  });

  it("accepts complete reference-adjusted provenance", () => {
    expect(
      validate({
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
      }),
      JSON.stringify(validate.errors),
    ).toBe(true);
  });

  it("rejects incomplete reference adjustment", () => {
    expect(
      validate({
        ...aggregateFixture,
        calibration: {
          state: "reference_adjusted",
          offset_db: 42,
          calibrated_at: null,
          method: null,
        },
      }),
    ).toBe(false);
  });
});
