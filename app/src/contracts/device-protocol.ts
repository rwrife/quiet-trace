import {
  AggregateRecordSchemaError,
  parseAggregateRecord,
  type AggregateRecord,
} from "./aggregate-record";

const SAFE_MAX = Number.MAX_SAFE_INTEGER;
const ISO_UTC_REGEX = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})Z$/;
const QUALITY_FLAG_REGEX = /^[a-z][a-z0-9_]{0,47}$/;

const FORBIDDEN_FIELD_NAMES = new Set([
  "audio",
  "audio_buffer",
  "audio_frame",
  "audio_samples",
  "audio_stream",
  "browser_token",
  "credential",
  "credentials",
  "firmware_secret",
  "mic",
  "microphone",
  "pcm",
  "raw",
  "raw_audio",
  "sample",
  "samples",
  "secret",
  "speech",
  "token",
  "transcript",
  "voice",
  "waveform",
  "wifi_password",
]);

const KNOWN_CLOCK_QUALITY = [
  "unknown",
  "monotonic_only",
  "host_set",
  "synced",
] as const;

const KNOWN_CALIBRATION_STATES = [
  "uncalibrated",
  "reference_adjusted",
  "invalid",
] as const;

export type ClockQuality = AggregateRecord["clock_quality"];
export type CalibrationState = AggregateRecord["calibration"]["state"];

export class ProtocolSchemaError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "ProtocolSchemaError";
  }
}

export interface StatusCounters {
  readonly clipping_samples: number;
  readonly framing_error_samples: number;
  readonly acquisition_overruns: number;
  readonly dropped_intervals: number;
  readonly clock_uncertain_intervals: number;
  readonly valid_samples: number;
  readonly dropped_samples: number;
}

export interface DeviceStatusV1 {
  readonly schema: "quiet-trace/status/v1";
  readonly records: number;
  readonly generation: number;
  readonly setup_active: boolean;
  readonly calibration_state: CalibrationState;
  readonly clock_quality: ClockQuality;
  readonly fixture_mode: boolean;
  readonly counters: StatusCounters;
  readonly extensions: Readonly<Record<string, unknown>>;
}

export interface PagingCursor {
  readonly after: number | null;
  readonly next_after: number | null;
  readonly limit: number;
  readonly has_more: boolean;
}

export interface RecordsPageV1 {
  readonly schema: "quiet-trace/records-page/v1";
  readonly records: readonly Readonly<AggregateRecord>[];
  readonly paging: PagingCursor;
  readonly generated_at: string | null;
  readonly clock_quality: ClockQuality;
  readonly quality_flags: readonly string[];
  readonly extensions: Readonly<Record<string, unknown>>;
}

export interface SessionSummaryV1 {
  readonly session_id: string;
  readonly label: string;
  readonly records: number;
  readonly interval_start: string | null;
  readonly interval_end: string | null;
  readonly level_eq_avg_dbfs: number;
  readonly peak_dbfs: number;
  readonly calibration_state: CalibrationState;
  readonly clock_quality: ClockQuality;
  readonly quality_flags: readonly string[];
}

export interface SessionsResponseV1 {
  readonly schema: "quiet-trace/sessions/v1";
  readonly sessions: readonly SessionSummaryV1[];
  readonly extensions: Readonly<Record<string, unknown>>;
}

export interface AnnotationAckV1 {
  readonly schema: "quiet-trace/annotation-ack/v1";
  readonly annotation_id: string;
  readonly accepted_at: string;
  readonly session_id: string | null;
  readonly sequence: number | null;
  readonly note: string;
}

export interface BackupPayloadV1 {
  readonly schema: "quiet-trace/export/v1";
  readonly exported_at: string;
  readonly source: "fixture" | "device";
  readonly records: readonly Readonly<AggregateRecord>[];
  readonly sessions: readonly SessionSummaryV1[];
  readonly annotations: readonly AnnotationAckV1[];
}

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function expectObject(value: unknown, path: string): Record<string, unknown> {
  if (!isObject(value)) {
    throw new ProtocolSchemaError(`${path} must be an object`);
  }

  return value;
}

function expectString(
  value: unknown,
  path: string,
  options: { minLength?: number; maxLength?: number; pattern?: RegExp } = {},
): string {
  if (typeof value !== "string") {
    throw new ProtocolSchemaError(`${path} must be a string`);
  }

  const scalarLength = [...value].length;
  if (options.minLength !== undefined && scalarLength < options.minLength) {
    throw new ProtocolSchemaError(
      `${path} must be at least ${options.minLength} characters`,
    );
  }

  if (options.maxLength !== undefined && scalarLength > options.maxLength) {
    throw new ProtocolSchemaError(
      `${path} must be at most ${options.maxLength} characters`,
    );
  }

  if (options.pattern && !options.pattern.test(value)) {
    throw new ProtocolSchemaError(`${path} has invalid format`);
  }

  return value;
}

function expectInteger(
  value: unknown,
  path: string,
  min = 0,
  max = SAFE_MAX,
): number {
  if (typeof value !== "number" || !Number.isInteger(value)) {
    throw new ProtocolSchemaError(`${path} must be an integer`);
  }

  if (value < min || value > max) {
    throw new ProtocolSchemaError(`${path} must be between ${min} and ${max}`);
  }

  return value;
}

function expectFiniteNumber(
  value: unknown,
  path: string,
  min = -1_000,
  max = 1_000,
): number {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    throw new ProtocolSchemaError(`${path} must be a finite number`);
  }

  if (value < min || value > max) {
    throw new ProtocolSchemaError(`${path} must be between ${min} and ${max}`);
  }

  return value;
}

function expectBoolean(value: unknown, path: string): boolean {
  if (typeof value !== "boolean") {
    throw new ProtocolSchemaError(`${path} must be a boolean`);
  }

  return value;
}

function expectIsoUtc(value: unknown, path: string): string {
  const text = expectString(value, path);

  if (!ISO_UTC_REGEX.test(text)) {
    throw new ProtocolSchemaError(`${path} must match YYYY-MM-DDTHH:MM:SSZ`);
  }

  const parsed = Date.parse(text);
  if (!Number.isFinite(parsed)) {
    throw new ProtocolSchemaError(`${path} is not a valid UTC instant`);
  }

  return text;
}

function expectNullableIsoUtc(value: unknown, path: string): string | null {
  if (value === null) {
    return null;
  }

  return expectIsoUtc(value, path);
}

function expectClockQuality(value: unknown, path: string): ClockQuality {
  const quality = expectString(value, path);
  if (!KNOWN_CLOCK_QUALITY.includes(quality as ClockQuality)) {
    throw new ProtocolSchemaError(
      `${path} must be a known clock_quality value`,
    );
  }

  return quality as ClockQuality;
}

function expectCalibrationState(
  value: unknown,
  path: string,
): CalibrationState {
  const calibration = expectString(value, path);
  if (!KNOWN_CALIBRATION_STATES.includes(calibration as CalibrationState)) {
    throw new ProtocolSchemaError(
      `${path} must be a known calibration_state value`,
    );
  }

  return calibration as CalibrationState;
}

function expectQualityFlags(value: unknown, path: string): readonly string[] {
  if (!Array.isArray(value)) {
    throw new ProtocolSchemaError(`${path} must be an array`);
  }

  if (value.length > 16) {
    throw new ProtocolSchemaError(`${path} must contain at most 16 flags`);
  }

  const seen = new Set<string>();
  return value.map((flag, index) => {
    const parsed = expectString(flag, `${path}[${index}]`, {
      pattern: QUALITY_FLAG_REGEX,
    });
    if (seen.has(parsed)) {
      throw new ProtocolSchemaError(`${path} cannot contain duplicate flags`);
    }

    seen.add(parsed);
    return parsed;
  });
}

function expectIntOrNull(value: unknown, path: string): number | null {
  if (value === null) {
    return null;
  }

  return expectInteger(value, path);
}

function rejectForbiddenFields(node: unknown, path: string): void {
  if (Array.isArray(node)) {
    node.forEach((value, index) =>
      rejectForbiddenFields(value, `${path}[${index}]`),
    );
    return;
  }

  if (!isObject(node)) {
    return;
  }

  for (const [key, value] of Object.entries(node)) {
    const normalized = key.toLowerCase();
    if (FORBIDDEN_FIELD_NAMES.has(normalized)) {
      throw new ProtocolSchemaError(`Forbidden field '${key}' at ${path}`);
    }

    rejectForbiddenFields(value, `${path}.${key}`);
  }
}

function collectExtensions(
  source: Record<string, unknown>,
  knownKeys: readonly string[],
): Readonly<Record<string, unknown>> {
  const known = new Set(knownKeys);
  return Object.freeze(
    Object.fromEntries(
      Object.entries(source).filter(([key]) => !known.has(key)),
    ),
  );
}

export function parseDeviceStatus(payload: unknown): DeviceStatusV1 {
  rejectForbiddenFields(payload, "$status");
  const root = expectObject(payload, "$status");

  const schema = expectString(root.schema, "$status.schema");
  if (schema !== "quiet-trace/status/v1") {
    throw new ProtocolSchemaError(
      "$status.schema must be quiet-trace/status/v1",
    );
  }

  const counters = expectObject(root.counters, "$status.counters");

  return Object.freeze({
    schema,
    records: expectInteger(root.records, "$status.records"),
    generation: expectInteger(root.generation, "$status.generation"),
    setup_active: expectBoolean(root.setup_active, "$status.setup_active"),
    calibration_state: expectCalibrationState(
      root.calibration_state,
      "$status.calibration_state",
    ),
    clock_quality: expectClockQuality(
      root.clock_quality,
      "$status.clock_quality",
    ),
    fixture_mode: expectBoolean(root.fixture_mode, "$status.fixture_mode"),
    counters: Object.freeze({
      clipping_samples: expectInteger(
        counters.clipping_samples,
        "$status.counters.clipping_samples",
      ),
      framing_error_samples: expectInteger(
        counters.framing_error_samples,
        "$status.counters.framing_error_samples",
      ),
      acquisition_overruns: expectInteger(
        counters.acquisition_overruns,
        "$status.counters.acquisition_overruns",
      ),
      dropped_intervals: expectInteger(
        counters.dropped_intervals,
        "$status.counters.dropped_intervals",
      ),
      clock_uncertain_intervals: expectInteger(
        counters.clock_uncertain_intervals,
        "$status.counters.clock_uncertain_intervals",
      ),
      valid_samples: expectInteger(
        counters.valid_samples,
        "$status.counters.valid_samples",
      ),
      dropped_samples: expectInteger(
        counters.dropped_samples,
        "$status.counters.dropped_samples",
      ),
    }),
    extensions: collectExtensions(root, [
      "schema",
      "records",
      "generation",
      "setup_active",
      "calibration_state",
      "clock_quality",
      "fixture_mode",
      "counters",
    ]),
  });
}

export function parseRecordsPage(payload: unknown): RecordsPageV1 {
  rejectForbiddenFields(payload, "$records_page");
  const root = expectObject(payload, "$records_page");

  const schema = expectString(root.schema, "$records_page.schema");
  if (schema !== "quiet-trace/records-page/v1") {
    throw new ProtocolSchemaError(
      "$records_page.schema must be quiet-trace/records-page/v1",
    );
  }

  if (!Array.isArray(root.records)) {
    throw new ProtocolSchemaError("$records_page.records must be an array");
  }

  const parsedRecords = root.records.map((record, index) => {
    try {
      return parseAggregateRecord(record);
    } catch (error) {
      if (error instanceof AggregateRecordSchemaError) {
        throw new ProtocolSchemaError(
          `$records_page.records[${index}] invalid: ${error.message}`,
        );
      }

      throw error;
    }
  });

  const paging = expectObject(root.paging, "$records_page.paging");
  const hasMore = expectBoolean(
    paging.has_more,
    "$records_page.paging.has_more",
  );
  const nextAfter = expectIntOrNull(
    paging.next_after,
    "$records_page.paging.next_after",
  );

  if (hasMore && nextAfter === null) {
    throw new ProtocolSchemaError(
      "$records_page.paging.next_after must be integer when has_more=true",
    );
  }

  if (!hasMore && nextAfter !== null) {
    throw new ProtocolSchemaError(
      "$records_page.paging.next_after must be null when has_more=false",
    );
  }

  return Object.freeze({
    schema,
    records: Object.freeze(parsedRecords),
    paging: Object.freeze({
      after: expectIntOrNull(paging.after, "$records_page.paging.after"),
      next_after: nextAfter,
      limit: expectInteger(paging.limit, "$records_page.paging.limit", 1, 200),
      has_more: hasMore,
    }),
    generated_at: expectNullableIsoUtc(
      root.generated_at,
      "$records_page.generated_at",
    ),
    clock_quality: expectClockQuality(
      root.clock_quality,
      "$records_page.clock_quality",
    ),
    quality_flags: Object.freeze(
      expectQualityFlags(root.quality_flags, "$records_page.quality_flags"),
    ),
    extensions: collectExtensions(root, [
      "schema",
      "records",
      "paging",
      "generated_at",
      "clock_quality",
      "quality_flags",
    ]),
  });
}

function parseSession(value: unknown, path: string): SessionSummaryV1 {
  const session = expectObject(value, path);

  return Object.freeze({
    session_id: expectString(session.session_id, `${path}.session_id`, {
      minLength: 1,
      maxLength: 64,
      pattern: /^[a-z0-9_-]+$/,
    }),
    label: expectString(session.label, `${path}.label`, {
      minLength: 1,
      maxLength: 80,
    }),
    records: expectInteger(session.records, `${path}.records`),
    interval_start: expectNullableIsoUtc(
      session.interval_start,
      `${path}.interval_start`,
    ),
    interval_end: expectNullableIsoUtc(
      session.interval_end,
      `${path}.interval_end`,
    ),
    level_eq_avg_dbfs: expectFiniteNumber(
      session.level_eq_avg_dbfs,
      `${path}.level_eq_avg_dbfs`,
      -200,
      10,
    ),
    peak_dbfs: expectFiniteNumber(
      session.peak_dbfs,
      `${path}.peak_dbfs`,
      -200,
      10,
    ),
    calibration_state: expectCalibrationState(
      session.calibration_state,
      `${path}.calibration_state`,
    ),
    clock_quality: expectClockQuality(
      session.clock_quality,
      `${path}.clock_quality`,
    ),
    quality_flags: expectQualityFlags(
      session.quality_flags,
      `${path}.quality_flags`,
    ),
  });
}

export function parseSessionsResponse(payload: unknown): SessionsResponseV1 {
  rejectForbiddenFields(payload, "$sessions");
  const root = expectObject(payload, "$sessions");
  const schema = expectString(root.schema, "$sessions.schema");
  if (schema !== "quiet-trace/sessions/v1") {
    throw new ProtocolSchemaError(
      "$sessions.schema must be quiet-trace/sessions/v1",
    );
  }

  if (!Array.isArray(root.sessions)) {
    throw new ProtocolSchemaError("$sessions.sessions must be an array");
  }

  return Object.freeze({
    schema,
    sessions: Object.freeze(
      root.sessions.map((session, index) =>
        parseSession(session, `$sessions.sessions[${index}]`),
      ),
    ),
    extensions: collectExtensions(root, ["schema", "sessions"]),
  });
}

function parseAnnotationAck(value: unknown, path: string): AnnotationAckV1 {
  const root = expectObject(value, path);
  const schema = expectString(root.schema, `${path}.schema`);
  if (schema !== "quiet-trace/annotation-ack/v1") {
    throw new ProtocolSchemaError(
      `${path}.schema must be quiet-trace/annotation-ack/v1`,
    );
  }

  return Object.freeze({
    schema,
    annotation_id: expectString(root.annotation_id, `${path}.annotation_id`, {
      minLength: 8,
      maxLength: 128,
      pattern: /^[a-zA-Z0-9_-]+$/,
    }),
    accepted_at: expectIsoUtc(root.accepted_at, `${path}.accepted_at`),
    session_id:
      root.session_id === null
        ? null
        : expectString(root.session_id, `${path}.session_id`, {
            minLength: 1,
            maxLength: 64,
            pattern: /^[a-z0-9_-]+$/,
          }),
    sequence: expectIntOrNull(root.sequence, `${path}.sequence`),
    note: expectString(root.note, `${path}.note`, {
      minLength: 1,
      maxLength: 240,
    }),
  });
}

export function parseBackupPayload(payload: unknown): BackupPayloadV1 {
  rejectForbiddenFields(payload, "$backup");
  const root = expectObject(payload, "$backup");

  const schema = expectString(root.schema, "$backup.schema");
  if (schema !== "quiet-trace/export/v1") {
    throw new ProtocolSchemaError(
      "$backup.schema must be quiet-trace/export/v1",
    );
  }

  const source = expectString(root.source, "$backup.source");
  if (source !== "fixture" && source !== "device") {
    throw new ProtocolSchemaError("$backup.source must be fixture or device");
  }

  if (!Array.isArray(root.records)) {
    throw new ProtocolSchemaError("$backup.records must be an array");
  }
  if (!Array.isArray(root.sessions)) {
    throw new ProtocolSchemaError("$backup.sessions must be an array");
  }
  if (!Array.isArray(root.annotations)) {
    throw new ProtocolSchemaError("$backup.annotations must be an array");
  }

  return Object.freeze({
    schema,
    exported_at: expectIsoUtc(root.exported_at, "$backup.exported_at"),
    source,
    records: Object.freeze(
      root.records.map((record, index) => {
        try {
          return parseAggregateRecord(record);
        } catch (error) {
          if (error instanceof AggregateRecordSchemaError) {
            throw new ProtocolSchemaError(
              `$backup.records[${index}] invalid: ${error.message}`,
            );
          }

          throw error;
        }
      }),
    ),
    sessions: Object.freeze(
      root.sessions.map((session, index) =>
        parseSession(session, `$backup.sessions[${index}]`),
      ),
    ),
    annotations: Object.freeze(
      root.annotations.map((entry, index) =>
        parseAnnotationAck(entry, `$backup.annotations[${index}]`),
      ),
    ),
  });
}
