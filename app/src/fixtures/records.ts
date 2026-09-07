import { aggregateFixture } from "./aggregate";
import {
  parseRecordsPage,
  type RecordsPageV1,
} from "../contracts/device-protocol";

function plusMinutes(timestamp: string, minutes: number): string {
  const base = new Date(timestamp).getTime();
  const shifted = new Date(base + minutes * 60_000);
  return shifted.toISOString().replace(".000", "");
}

function makeRecord(sequence: number, minuteOffset: number) {
  const start = plusMinutes("2026-09-07T12:00:00Z", minuteOffset);

  return {
    ...aggregateFixture,
    sequence,
    interval_start: start,
    monotonic_start_ms:
      aggregateFixture.monotonic_start_ms + minuteOffset * 60_000,
    level_eq_dbfs: Number(
      (aggregateFixture.level_eq_dbfs + minuteOffset * 0.15).toFixed(2),
    ),
    peak_125ms_dbfs: Number(
      (aggregateFixture.peak_125ms_dbfs + minuteOffset * 0.1).toFixed(2),
    ),
    quality_flags:
      minuteOffset % 5 === 0
        ? ["uncalibrated", "clock_drift_suspected"]
        : ["calibration_pending"],
    clock_quality: minuteOffset % 2 === 0 ? "host_set" : "synced",
    calibration: {
      ...aggregateFixture.calibration,
      state: minuteOffset % 3 === 0 ? "reference_adjusted" : "uncalibrated",
      offset_db: minuteOffset % 3 === 0 ? 1.25 : null,
      calibrated_at: minuteOffset % 3 === 0 ? "2026-09-01T11:00:00Z" : null,
      method: minuteOffset % 3 === 0 ? "field_reference" : null,
      reference_instrument: minuteOffset % 3 === 0 ? "Class 2 SLM" : null,
      reference_placement: minuteOffset % 3 === 0 ? "desktop tripod" : null,
      reference_source: minuteOffset % 3 === 0 ? "pink_noise_1kHz" : null,
      reference_duration_s: minuteOffset % 3 === 0 ? 60 : null,
      firmware_version: minuteOffset % 3 === 0 ? "0.7.1" : null,
      hardware_revision: minuteOffset % 3 === 0 ? "revA" : null,
    },
  } as const;
}

export const fixtureRecordHistory = Object.freeze(
  Array.from({ length: 24 }, (_, index) => makeRecord(101 + index, index)),
);

export function fixturePage(
  after: number | null,
  limit: number,
): Readonly<RecordsPageV1> {
  const pageSize = Math.max(1, Math.min(limit, 50));
  const filtered =
    after === null
      ? fixtureRecordHistory
      : fixtureRecordHistory.filter((record) => record.sequence > after);

  const records = filtered.slice(0, pageSize);
  const hasMore = filtered.length > pageSize;
  const nextAfter =
    hasMore && records.length > 0 ? (records.at(-1)?.sequence ?? null) : null;

  return parseRecordsPage({
    schema: "quiet-trace/records-page/v1",
    records,
    paging: {
      after,
      next_after: nextAfter,
      limit: pageSize,
      has_more: hasMore,
    },
    generated_at: records.at(-1)?.interval_start ?? null,
    clock_quality: records.at(-1)?.clock_quality ?? "unknown",
    quality_flags: [],
  });
}
