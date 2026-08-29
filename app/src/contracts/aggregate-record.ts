export type CalibrationState =
  "uncalibrated" | "reference_adjusted" | "invalid";

export interface CalibrationMetadata {
  readonly state: CalibrationState;
  readonly offset_db: number | null;
  readonly calibrated_at: string | null;
  readonly method: string | null;
  readonly reference_instrument: string | null;
  readonly reference_placement: string | null;
  readonly reference_source: string | null;
  readonly reference_duration_s: number | null;
  readonly firmware_version: string | null;
  readonly hardware_revision: string | null;
}

export interface AggregateRecord {
  readonly schema: "quiet-trace/aggregate/v1";
  readonly sequence: number;
  readonly interval_start: string | null;
  readonly monotonic_start_ms: number;
  readonly duration_ms: 60_000;
  readonly level_eq_dbfs: number;
  readonly peak_125ms_dbfs: number;
  readonly histogram_counts: readonly number[];
  readonly calibration: CalibrationMetadata;
  readonly clock_quality: "unknown" | "monotonic_only" | "host_set" | "synced";
  readonly quality_flags: readonly string[];
  readonly session_id: string | null;
}

const aggregateFields = new Set([
  "schema",
  "sequence",
  "interval_start",
  "monotonic_start_ms",
  "duration_ms",
  "level_eq_dbfs",
  "peak_125ms_dbfs",
  "histogram_counts",
  "calibration",
  "clock_quality",
  "quality_flags",
  "session_id",
]);

const calibrationFields = new Set([
  "state",
  "offset_db",
  "calibrated_at",
  "method",
  "reference_instrument",
  "reference_placement",
  "reference_source",
  "reference_duration_s",
  "firmware_version",
  "hardware_revision",
]);

const forbiddenKeys = new Set([
  "audio",
  "authtoken",
  "credential",
  "credentials",
  "encodedaudio",
  "encodedmedia",
  "eventlabel",
  "fftbins",
  "pcm",
  "rawsample",
  "rawsamples",
  "recording",
  "samples",
  "secret",
  "speakerid",
  "sourceid",
  "sourcelabel",
  "spectra",
  "spectrum",
  "speech",
  "transcript",
  "voiceactivity",
  "waveform",
  "wifipassword",
]);

const clockQualities = new Set([
  "unknown",
  "monotonic_only",
  "host_set",
  "synced",
]);

const isoUtcPattern =
  /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.\d{1,9})?Z$/;
const qualityFlagPattern = /^[a-z][a-z0-9_]{0,47}$/;

export class AggregateRecordSchemaError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "AggregateRecordSchemaError";
  }
}

function rejectForbiddenFields(value: unknown, path = "$"): void {
  if (Array.isArray(value)) {
    value.forEach((entry, index) =>
      rejectForbiddenFields(entry, `${path}[${index}]`),
    );
    return;
  }

  if (value === null || typeof value !== "object") {
    return;
  }

  for (const [key, child] of Object.entries(value)) {
    const normalizedKey = key.toLowerCase().replaceAll(/[^a-z0-9]/g, "");
    if (forbiddenKeys.has(normalizedKey)) {
      throw new AggregateRecordSchemaError(`forbidden field at ${path}.${key}`);
    }
    rejectForbiddenFields(child, `${path}.${key}`);
  }
}

function requireObject(value: unknown, name: string): Record<string, unknown> {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    throw new AggregateRecordSchemaError(`${name} must be an object`);
  }
  return value as Record<string, unknown>;
}

function rejectUnknownFields(
  value: Record<string, unknown>,
  allowed: ReadonlySet<string>,
  name: string,
): void {
  const unknown = Object.keys(value).find((key) => !allowed.has(key));
  if (unknown !== undefined) {
    throw new AggregateRecordSchemaError(`${name}.${unknown} is not allowed`);
  }
}

function requireFiniteRange(
  value: unknown,
  name: string,
  minimum: number,
  maximum: number,
): number {
  if (
    typeof value !== "number" ||
    !Number.isFinite(value) ||
    value < minimum ||
    value > maximum
  ) {
    throw new AggregateRecordSchemaError(
      `${name} must be finite and between ${minimum} and ${maximum}`,
    );
  }
  return value;
}

function isIsoUtc(value: unknown): value is string {
  if (typeof value !== "string") {
    return false;
  }
  const match = isoUtcPattern.exec(value);
  if (match === null) {
    return false;
  }

  const [, yearText, monthText, dayText, hourText, minuteText, secondText] =
    match;
  const year = Number(yearText);
  const month = Number(monthText);
  const day = Number(dayText);
  const hour = Number(hourText);
  const minute = Number(minuteText);
  const second = Number(secondText);
  const leapYear = year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0);
  const daysInMonth = [
    31,
    leapYear ? 29 : 28,
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31,
  ];

  const maxDay = daysInMonth[month - 1];
  return (
    year >= 1 &&
    month >= 1 &&
    month <= 12 &&
    maxDay !== undefined &&
    day >= 1 &&
    day <= maxDay &&
    hour <= 23 &&
    minute <= 59 &&
    second <= 59
  );
}

function unicodeScalarLength(value: string): number | null {
  let count = 0;
  for (let index = 0; index < value.length; index += 1) {
    const codeUnit = value.charCodeAt(index);
    if (codeUnit >= 0xd800 && codeUnit <= 0xdbff) {
      const next = value.charCodeAt(index + 1);
      if (!(next >= 0xdc00 && next <= 0xdfff)) {
        return null;
      }
      index += 1;
    } else if (codeUnit >= 0xdc00 && codeUnit <= 0xdfff) {
      return null;
    }
    count += 1;
  }
  return count;
}

function isBoundedText(
  value: unknown,
  minimum: number,
  maximum: number,
): value is string {
  if (typeof value !== "string") {
    return false;
  }
  const length = unicodeScalarLength(value);
  return length !== null && length >= minimum && length <= maximum;
}

function validateCalibration(value: unknown): void {
  const calibration = requireObject(value, "calibration");
  rejectUnknownFields(calibration, calibrationFields, "calibration");

  if (
    calibration.state !== "uncalibrated" &&
    calibration.state !== "reference_adjusted" &&
    calibration.state !== "invalid"
  ) {
    throw new AggregateRecordSchemaError("invalid calibration state");
  }

  const hasOffset = calibration.offset_db !== null;
  if (
    hasOffset &&
    (typeof calibration.offset_db !== "number" ||
      !Number.isFinite(calibration.offset_db) ||
      calibration.offset_db < -80 ||
      calibration.offset_db > 80)
  ) {
    throw new AggregateRecordSchemaError(
      "offset_db must be null or finite within -80..80",
    );
  }

  const validReferenceText =
    isBoundedText(calibration.reference_instrument, 1, 160) &&
    isBoundedText(calibration.reference_placement, 1, 240) &&
    isBoundedText(calibration.reference_source, 1, 240) &&
    Number.isSafeInteger(calibration.reference_duration_s) &&
    (calibration.reference_duration_s as number) >= 1 &&
    (calibration.reference_duration_s as number) <= 86_400 &&
    isBoundedText(calibration.firmware_version, 1, 64) &&
    isBoundedText(calibration.hardware_revision, 1, 64);

  if (calibration.state === "reference_adjusted") {
    if (
      !hasOffset ||
      !isIsoUtc(calibration.calibrated_at) ||
      !isBoundedText(calibration.method, 1, 240) ||
      !validReferenceText
    ) {
      throw new AggregateRecordSchemaError(
        "reference_adjusted calibration requires bounded offset, UTC date, method, instrument, placement, reference source, duration, firmware, and hardware provenance",
      );
    }
    return;
  }

  if (
    calibration.offset_db !== null ||
    calibration.calibrated_at !== null ||
    calibration.method !== null ||
    calibration.reference_instrument !== null ||
    calibration.reference_placement !== null ||
    calibration.reference_source !== null ||
    calibration.reference_duration_s !== null ||
    calibration.firmware_version !== null ||
    calibration.hardware_revision !== null
  ) {
    throw new AggregateRecordSchemaError(
      "uncalibrated/invalid records must not carry calibration provenance",
    );
  }
}

function validateAggregateRecord(
  value: unknown,
): asserts value is AggregateRecord {
  rejectForbiddenFields(value);
  const record = requireObject(value, "record");
  rejectUnknownFields(record, aggregateFields, "record");

  if (record.schema !== "quiet-trace/aggregate/v1") {
    throw new AggregateRecordSchemaError("unsupported aggregate schema");
  }
  if (
    !Number.isSafeInteger(record.sequence) ||
    (record.sequence as number) < 0
  ) {
    throw new AggregateRecordSchemaError(
      "sequence must be a non-negative integer",
    );
  }
  if (record.interval_start !== null && !isIsoUtc(record.interval_start)) {
    throw new AggregateRecordSchemaError(
      "interval_start must be a UTC date-time or null",
    );
  }
  if (
    !Number.isSafeInteger(record.monotonic_start_ms) ||
    (record.monotonic_start_ms as number) < 0
  ) {
    throw new AggregateRecordSchemaError(
      "monotonic_start_ms must be a non-negative integer",
    );
  }
  if (record.duration_ms !== 60_000) {
    throw new AggregateRecordSchemaError("duration_ms must be 60000");
  }

  const equivalent = requireFiniteRange(
    record.level_eq_dbfs,
    "level_eq_dbfs",
    -200,
    0,
  );
  const peak = requireFiniteRange(
    record.peak_125ms_dbfs,
    "peak_125ms_dbfs",
    -200,
    0,
  );
  if (peak < equivalent) {
    throw new AggregateRecordSchemaError(
      "peak_125ms_dbfs cannot be below level_eq_dbfs",
    );
  }

  if (
    !Array.isArray(record.histogram_counts) ||
    record.histogram_counts.length !== 10 ||
    record.histogram_counts.some(
      (count) => !Number.isSafeInteger(count) || count < 0,
    ) ||
    record.histogram_counts.reduce((sum, count) => sum + count, 0) !== 480
  ) {
    throw new AggregateRecordSchemaError(
      "histogram_counts must contain ten non-negative integers totaling 480",
    );
  }

  validateCalibration(record.calibration);

  if (!clockQualities.has(record.clock_quality as string)) {
    throw new AggregateRecordSchemaError("invalid clock_quality");
  }
  const requiresWallTime =
    record.clock_quality === "host_set" || record.clock_quality === "synced";
  if ((record.interval_start !== null) !== requiresWallTime) {
    throw new AggregateRecordSchemaError(
      "clock_quality and interval_start must describe the same wall-time state",
    );
  }

  if (
    !Array.isArray(record.quality_flags) ||
    record.quality_flags.length > 8 ||
    record.quality_flags.some(
      (flag) => typeof flag !== "string" || !qualityFlagPattern.test(flag),
    ) ||
    new Set(record.quality_flags).size !== record.quality_flags.length
  ) {
    throw new AggregateRecordSchemaError(
      "quality_flags must be unique bounded snake-case strings",
    );
  }

  if (record.session_id !== null && !isBoundedText(record.session_id, 1, 64)) {
    throw new AggregateRecordSchemaError(
      "session_id must be null or a non-empty string of at most 64 characters",
    );
  }
}

function deepFreeze<T>(value: T): Readonly<T> {
  if (value !== null && typeof value === "object") {
    Object.freeze(value);
    for (const child of Object.values(value)) {
      deepFreeze(child);
    }
  }
  return value;
}

export function parseAggregateRecord(
  value: unknown,
): Readonly<AggregateRecord> {
  validateAggregateRecord(value);
  return deepFreeze(structuredClone(value));
}
