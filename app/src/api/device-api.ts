import {
  parseAggregateRecord,
  type AggregateRecord,
} from "../contracts/aggregate-record";
import {
  parseBackupPayload,
  parseDeviceStatus,
  parseRecordsPage,
  parseSessionsResponse,
  type AnnotationAckV1,
  type BackupPayloadV1,
  type DeviceStatusV1,
  type RecordsPageV1,
  type SessionsResponseV1,
} from "../contracts/device-protocol";
import { fixtureRecordHistory } from "../fixtures/records";
import { fixtureSessions } from "../fixtures/sessions";
import { fixtureStatus } from "../fixtures/status";
import {
  startReconnectingLiveStream,
  type EventSourceLike,
  type LiveStreamHandlers,
} from "../live-stream";

export type ExportFormat = "json" | "csv";

export interface ExportPayload {
  readonly format: ExportFormat;
  readonly filename: string;
  readonly contentType: string;
  readonly body: string;
}

export interface AnnotationRequest {
  readonly note: string;
  readonly sessionId: string | null;
  readonly sequence: number | null;
}

export interface RetentionSettings {
  readonly maxRecords: number;
  readonly retentionDays: number;
}

export interface DeleteRequest {
  readonly sessionId: string | null;
  readonly beforeSequence: number | null;
}

export interface FactoryEraseRequest {
  readonly confirmationText: string;
  readonly physicalConfirmed: boolean;
}

export interface OperationResult {
  readonly ok: boolean;
  readonly code: string;
  readonly message: string;
}

export interface BackupValidationResult extends OperationResult {
  readonly ticket: string | null;
  readonly summary: {
    records: number;
    sessions: number;
    annotations: number;
  } | null;
}

export interface DeviceApi {
  getStatus(): Promise<Readonly<DeviceStatusV1>>;
  getRecordsPage(
    after: number | null,
    limit: number,
  ): Promise<Readonly<RecordsPageV1>>;
  getSessions(): Promise<Readonly<SessionsResponseV1>>;
  postAnnotation(
    request: AnnotationRequest,
  ): Promise<Readonly<AnnotationAckV1>>;
  exportData(format: ExportFormat): Promise<Readonly<ExportPayload>>;
  validateBackup(rawBackup: string): Promise<Readonly<BackupValidationResult>>;
  applyBackup(ticket: string): Promise<Readonly<OperationResult>>;
  setRetention(settings: RetentionSettings): Promise<Readonly<OperationResult>>;
  deleteRecords(request: DeleteRequest): Promise<Readonly<OperationResult>>;
  factoryErase(
    request: FactoryEraseRequest,
  ): Promise<Readonly<OperationResult>>;
  subscribeLive(handlers: LiveStreamHandlers): () => void;
}

function scalarLength(value: string): number {
  return [...value].length;
}

function deriveIntervalEnd(record: Readonly<AggregateRecord>): string | null {
  if (record.interval_start === null) {
    return null;
  }

  const startMs = Date.parse(record.interval_start);
  if (!Number.isFinite(startMs)) {
    return null;
  }

  return new Date(startMs + record.duration_ms)
    .toISOString()
    .replace(".000", "");
}

function toCsv(records: readonly Readonly<AggregateRecord>[]): string {
  const header = [
    "sequence",
    "interval_start",
    "interval_end",
    "level_eq_dbfs",
    "peak_125ms_dbfs",
    "quality_flags",
    "calibration_state",
    "calibration_offset_db",
    "clock_quality",
  ];

  const rows = records.map((record) => [
    String(record.sequence),
    record.interval_start ?? "",
    deriveIntervalEnd(record) ?? "",
    String(record.level_eq_dbfs),
    String(record.peak_125ms_dbfs),
    record.quality_flags.join("|"),
    record.calibration.state,
    record.calibration.offset_db === null
      ? ""
      : String(record.calibration.offset_db),
    record.clock_quality,
  ]);

  return [header, ...rows]
    .map((row) =>
      row.map((cell) => `"${cell.replaceAll('"', '""')}"`).join(","),
    )
    .join("\n");
}

function parseResponseJson(text: string): unknown {
  try {
    return JSON.parse(text) as unknown;
  } catch {
    throw new Error("Response did not contain valid JSON");
  }
}

function clampInteger(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, Math.trunc(value)));
}

export class HttpDeviceApi implements DeviceApi {
  constructor(
    private readonly fetchFn: typeof fetch,
    private readonly eventSourceFactory: (url: string) => EventSourceLike,
  ) {}

  async getStatus(): Promise<Readonly<DeviceStatusV1>> {
    const response = await this.fetchFn("/api/v1/status", {
      headers: { Accept: "application/json" },
    });
    if (!response.ok) {
      throw new Error(`GET /api/v1/status failed with ${response.status}`);
    }

    return parseDeviceStatus(parseResponseJson(await response.text()));
  }

  async getRecordsPage(
    after: number | null,
    limit: number,
  ): Promise<Readonly<RecordsPageV1>> {
    const params = new URLSearchParams();
    if (after !== null) {
      params.set("after", String(after));
    }
    params.set("limit", String(clampInteger(limit, 1, 200)));

    const response = await this.fetchFn(
      `/api/v1/records?${params.toString()}`,
      {
        headers: { Accept: "application/json" },
      },
    );

    if (!response.ok) {
      throw new Error(`GET /api/v1/records failed with ${response.status}`);
    }

    return parseRecordsPage(parseResponseJson(await response.text()));
  }

  async getSessions(): Promise<Readonly<SessionsResponseV1>> {
    const response = await this.fetchFn("/api/v1/sessions", {
      headers: { Accept: "application/json" },
    });
    if (!response.ok) {
      throw new Error(`GET /api/v1/sessions failed with ${response.status}`);
    }

    return parseSessionsResponse(parseResponseJson(await response.text()));
  }

  async postAnnotation(
    request: AnnotationRequest,
  ): Promise<Readonly<AnnotationAckV1>> {
    if (
      scalarLength(request.note.trim()) === 0 ||
      scalarLength(request.note.trim()) > 240
    ) {
      throw new Error("Annotation note must be between 1 and 240 characters");
    }

    const response = await this.fetchFn("/api/v1/annotations", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json",
      },
      body: JSON.stringify({
        schema: "quiet-trace/annotation-request/v1",
        note: request.note.trim(),
        session_id: request.sessionId,
        sequence: request.sequence,
      }),
    });

    if (!response.ok) {
      throw new Error(
        `POST /api/v1/annotations failed with ${response.status}`,
      );
    }

    const payload = parseResponseJson(await response.text()) as Record<
      string,
      unknown
    >;
    return {
      schema: "quiet-trace/annotation-ack/v1",
      annotation_id: String(payload.annotation_id ?? ""),
      accepted_at: String(
        payload.accepted_at ?? new Date().toISOString().replace(".000", ""),
      ),
      session_id: (payload.session_id as string | null | undefined) ?? null,
      sequence:
        typeof payload.sequence === "number" &&
        Number.isInteger(payload.sequence)
          ? payload.sequence
          : null,
      note: request.note.trim(),
    };
  }

  async exportData(format: ExportFormat): Promise<Readonly<ExportPayload>> {
    const response = await this.fetchFn(`/api/v1/export/${format}`, {
      headers: {
        Accept: format === "json" ? "application/json" : "text/csv",
      },
    });

    if (!response.ok) {
      throw new Error(
        `GET /api/v1/export/${format} failed with ${response.status}`,
      );
    }

    return {
      format,
      filename: `quiet-trace-export-v1.${format}`,
      contentType:
        format === "json"
          ? "application/json;charset=utf-8"
          : "text/csv;charset=utf-8",
      body: await response.text(),
    };
  }

  async validateBackup(
    rawBackup: string,
  ): Promise<Readonly<BackupValidationResult>> {
    const response = await this.fetchFn("/api/v1/restore/validate", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json",
      },
      body: rawBackup,
    });

    if (!response.ok) {
      throw new Error(
        `POST /api/v1/restore/validate failed with ${response.status}`,
      );
    }

    const payload = parseResponseJson(await response.text()) as Record<
      string,
      unknown
    >;
    return {
      ok: Boolean(payload.ok),
      code: String(payload.code ?? "UNKNOWN"),
      message: String(payload.message ?? "Unknown validation response"),
      ticket: payload.ticket ? String(payload.ticket) : null,
      summary:
        payload.summary && typeof payload.summary === "object"
          ? {
              records: Number(
                (payload.summary as Record<string, unknown>).records ?? 0,
              ),
              sessions: Number(
                (payload.summary as Record<string, unknown>).sessions ?? 0,
              ),
              annotations: Number(
                (payload.summary as Record<string, unknown>).annotations ?? 0,
              ),
            }
          : null,
    };
  }

  async applyBackup(ticket: string): Promise<Readonly<OperationResult>> {
    const response = await this.fetchFn("/api/v1/restore/apply", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json",
      },
      body: JSON.stringify({ ticket }),
    });

    if (!response.ok) {
      throw new Error(
        `POST /api/v1/restore/apply failed with ${response.status}`,
      );
    }

    const payload = parseResponseJson(await response.text()) as Record<
      string,
      unknown
    >;
    return {
      ok: Boolean(payload.ok),
      code: String(payload.code ?? "UNKNOWN"),
      message: String(payload.message ?? "Unknown restore response"),
    };
  }

  async setRetention(
    settings: RetentionSettings,
  ): Promise<Readonly<OperationResult>> {
    const payload = {
      schema: "quiet-trace/retention-request/v1",
      max_records: clampInteger(settings.maxRecords, 100, 100_000),
      retention_days: clampInteger(settings.retentionDays, 1, 3650),
    };

    const response = await this.fetchFn("/api/v1/retention", {
      method: "PUT",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json",
      },
      body: JSON.stringify(payload),
    });

    if (!response.ok) {
      throw new Error(`PUT /api/v1/retention failed with ${response.status}`);
    }

    return {
      ok: true,
      code: "RETENTION_UPDATED",
      message: "Retention settings updated.",
    };
  }

  async deleteRecords(
    request: DeleteRequest,
  ): Promise<Readonly<OperationResult>> {
    const response = await this.fetchFn("/api/v1/records", {
      method: "DELETE",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json",
      },
      body: JSON.stringify({
        schema: "quiet-trace/delete-request/v1",
        session_id: request.sessionId,
        before_sequence: request.beforeSequence,
      }),
    });

    if (!response.ok) {
      throw new Error(`DELETE /api/v1/records failed with ${response.status}`);
    }

    return {
      ok: true,
      code: "DELETE_ACCEPTED",
      message: "Delete command submitted.",
    };
  }

  async factoryErase(
    request: FactoryEraseRequest,
  ): Promise<Readonly<OperationResult>> {
    const response = await this.fetchFn("/api/v1/factory-erase", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json",
      },
      body: JSON.stringify({
        schema: "quiet-trace/factory-erase-request/v1",
        confirmation_text: request.confirmationText,
        physical_confirmed: request.physicalConfirmed,
      }),
    });

    if (!response.ok) {
      throw new Error(
        `POST /api/v1/factory-erase failed with ${response.status}`,
      );
    }

    return {
      ok: true,
      code: "FACTORY_ERASE_ARMED",
      message: "Factory erase requested. Confirm on physical controls.",
    };
  }

  subscribeLive(handlers: LiveStreamHandlers): () => void {
    return startReconnectingLiveStream({
      url: "/api/v1/live",
      createEventSource: this.eventSourceFactory,
      parseEvent: (eventType, data) => {
        const payload = parseResponseJson(data);
        if (eventType === "aggregate") {
          return parseAggregateRecord(payload);
        }
        if (eventType === "status") {
          return parseDeviceStatus(payload);
        }

        return payload;
      },
      ...handlers,
    });
  }
}

interface FixtureState {
  records: readonly Readonly<AggregateRecord>[];
  sessions: Readonly<SessionsResponseV1>["sessions"];
  annotations: readonly Readonly<AnnotationAckV1>[];
  generation: number;
  retention: RetentionSettings;
}

function nowIsoSeconds(): string {
  const now = new Date();
  now.setMilliseconds(0);
  return now.toISOString().replace(".000", "");
}

function deriveSessions(records: readonly Readonly<AggregateRecord>[]) {
  if (records.length === 0) {
    return [] as Readonly<SessionsResponseV1>["sessions"];
  }

  const splitIndex = Math.max(1, Math.floor(records.length / 2));
  const morning = records.slice(0, splitIndex);
  const evening = records.slice(splitIndex);

  const summarize = (
    sessionId: string,
    label: string,
    bucket: readonly Readonly<AggregateRecord>[],
    calibrationState: "uncalibrated" | "reference_adjusted",
    clockQuality: "monotonic_only" | "host_set",
    qualityFlags: readonly string[],
  ) => {
    if (bucket.length === 0) {
      return null;
    }

    const eqAverage =
      bucket.reduce((total, record) => total + record.level_eq_dbfs, 0) /
      bucket.length;

    return {
      session_id: sessionId,
      label,
      records: bucket.length,
      interval_start: bucket.at(0)?.interval_start ?? null,
      interval_end: deriveIntervalEnd(
        bucket.at(-1) as Readonly<AggregateRecord>,
      ),
      level_eq_avg_dbfs: Number(eqAverage.toFixed(2)),
      peak_dbfs: Math.max(...bucket.map((record) => record.peak_125ms_dbfs)),
      calibration_state: calibrationState,
      clock_quality: clockQuality,
      quality_flags: [...qualityFlags],
    };
  };

  const sessions = [
    summarize(
      "session_morning_lab",
      "Morning lab run",
      morning,
      "reference_adjusted",
      "host_set",
      ["calibration_pending"],
    ),
    summarize(
      "session_evening_office",
      "Evening office sample",
      evening,
      "uncalibrated",
      "monotonic_only",
      ["wall_time_unknown", "uncalibrated"],
    ),
  ].filter((entry) => entry !== null);

  return parseSessionsResponse({
    schema: "quiet-trace/sessions/v1",
    sessions,
  }).sessions;
}

export class FixtureDeviceApi implements DeviceApi {
  private readonly restoreTickets = new Map<string, BackupPayloadV1>();

  private state: FixtureState = {
    records: fixtureRecordHistory,
    sessions: fixtureSessions.sessions,
    annotations: [],
    generation: fixtureStatus.generation,
    retention: {
      maxRecords: 50_000,
      retentionDays: 30,
    },
  };

  async getStatus(): Promise<Readonly<DeviceStatusV1>> {
    const totalSamples = this.state.records.length * 4_000;
    const droppedSamples = this.state.records.reduce(
      (total, record) =>
        total + (record.quality_flags.includes("dropped_samples") ? 12 : 0),
      0,
    );

    return parseDeviceStatus({
      ...fixtureStatus,
      records: this.state.records.length,
      generation: this.state.generation,
      counters: {
        clipping_samples: this.state.records.filter((record) =>
          record.quality_flags.includes("clipping_detected"),
        ).length,
        framing_error_samples: 0,
        acquisition_overruns: 0,
        dropped_intervals: this.state.records.filter((record) =>
          record.quality_flags.includes("interval_dropped"),
        ).length,
        clock_uncertain_intervals: this.state.records.filter(
          (record) => record.clock_quality === "monotonic_only",
        ).length,
        valid_samples: totalSamples - droppedSamples,
        dropped_samples: droppedSamples,
      },
    });
  }

  async getRecordsPage(
    after: number | null,
    limit: number,
  ): Promise<Readonly<RecordsPageV1>> {
    const pageSize = clampInteger(limit, 1, 200);
    const sorted = [...this.state.records].sort(
      (a, b) => a.sequence - b.sequence,
    );
    const filtered =
      after === null
        ? sorted
        : sorted.filter((record) => record.sequence > after);
    const page = filtered.slice(0, pageSize);
    const hasMore = filtered.length > pageSize;
    const nextAfter = hasMore ? (page.at(-1)?.sequence ?? null) : null;

    return parseRecordsPage({
      schema: "quiet-trace/records-page/v1",
      records: page,
      paging: {
        after,
        next_after: nextAfter,
        limit: pageSize,
        has_more: hasMore,
      },
      generated_at: nowIsoSeconds(),
      clock_quality: page.at(-1)?.clock_quality ?? "unknown",
      quality_flags: page.some(
        (record) => record.clock_quality === "monotonic_only",
      )
        ? ["wall_time_partial"]
        : [],
    });
  }

  async getSessions(): Promise<Readonly<SessionsResponseV1>> {
    return parseSessionsResponse({
      schema: "quiet-trace/sessions/v1",
      sessions: this.state.sessions,
    });
  }

  async postAnnotation(
    request: AnnotationRequest,
  ): Promise<Readonly<AnnotationAckV1>> {
    const note = request.note.trim();
    const length = scalarLength(note);
    if (length === 0 || length > 240) {
      throw new Error("Annotation note must be between 1 and 240 characters");
    }

    const annotation = Object.freeze({
      schema: "quiet-trace/annotation-ack/v1" as const,
      annotation_id: `ann_${Date.now().toString(36)}_${this.state.annotations.length + 1}`,
      accepted_at: nowIsoSeconds(),
      session_id: request.sessionId,
      sequence: request.sequence,
      note,
    });

    this.state = {
      ...this.state,
      annotations: [...this.state.annotations, annotation],
      generation: this.state.generation + 1,
    };

    return annotation;
  }

  async exportData(format: ExportFormat): Promise<Readonly<ExportPayload>> {
    const payload: BackupPayloadV1 = parseBackupPayload({
      schema: "quiet-trace/export/v1",
      exported_at: nowIsoSeconds(),
      source: "fixture",
      records: this.state.records,
      sessions: this.state.sessions,
      annotations: this.state.annotations,
    });

    if (format === "csv") {
      return {
        format,
        filename: `quiet-trace-export-v1-${Date.now()}.csv`,
        contentType: "text/csv;charset=utf-8",
        body: toCsv(payload.records),
      };
    }

    return {
      format,
      filename: `quiet-trace-export-v1-${Date.now()}.json`,
      contentType: "application/json;charset=utf-8",
      body: JSON.stringify(payload, null, 2),
    };
  }

  async validateBackup(
    rawBackup: string,
  ): Promise<Readonly<BackupValidationResult>> {
    try {
      const payload = parseBackupPayload(parseResponseJson(rawBackup));
      const ticket = `restore_${Date.now().toString(36)}`;
      this.restoreTickets.set(ticket, payload);
      return {
        ok: true,
        code: "VALID",
        message: "Backup payload is valid aggregate-only data.",
        ticket,
        summary: {
          records: payload.records.length,
          sessions: payload.sessions.length,
          annotations: payload.annotations.length,
        },
      };
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return {
        ok: false,
        code: "SCHEMA_MISMATCH",
        message,
        ticket: null,
        summary: null,
      };
    }
  }

  async applyBackup(ticket: string): Promise<Readonly<OperationResult>> {
    const payload = this.restoreTickets.get(ticket);
    if (!payload) {
      return {
        ok: false,
        code: "UNKNOWN_TICKET",
        message: "Restore ticket expired or unknown. Validate backup again.",
      };
    }

    this.state = {
      ...this.state,
      records: payload.records,
      sessions: payload.sessions,
      annotations: payload.annotations,
      generation: this.state.generation + 1,
    };

    this.restoreTickets.delete(ticket);

    return {
      ok: true,
      code: "RESTORE_APPLIED",
      message: "Backup restored to local aggregate store.",
    };
  }

  async setRetention(
    settings: RetentionSettings,
  ): Promise<Readonly<OperationResult>> {
    const maxRecords = clampInteger(settings.maxRecords, 100, 100_000);
    const retentionDays = clampInteger(settings.retentionDays, 1, 3650);

    const trimmed = [...this.state.records]
      .sort((a, b) => a.sequence - b.sequence)
      .slice(-maxRecords);

    this.state = {
      ...this.state,
      records: trimmed,
      sessions: deriveSessions(trimmed),
      retention: { maxRecords, retentionDays },
      generation: this.state.generation + 1,
    };

    return {
      ok: true,
      code: "RETENTION_UPDATED",
      message: `Retention set to ${retentionDays} days, max ${maxRecords} records.`,
    };
  }

  async deleteRecords(
    request: DeleteRequest,
  ): Promise<Readonly<OperationResult>> {
    const sessionId = request.sessionId;
    const before = request.beforeSequence;

    const filtered = this.state.records.filter((record, index) => {
      const inferredSession =
        index < this.state.records.length / 2
          ? "session_morning_lab"
          : "session_evening_office";

      if (sessionId !== null && inferredSession !== sessionId) {
        return true;
      }

      if (before !== null && record.sequence >= before) {
        return true;
      }

      return sessionId !== null || before !== null ? false : false;
    });

    const removed = this.state.records.length - filtered.length;
    this.state = {
      ...this.state,
      records: filtered,
      sessions: deriveSessions(filtered),
      generation: this.state.generation + 1,
    };

    return {
      ok: true,
      code: "DELETE_COMPLETED",
      message: `Deleted ${removed} aggregate records from local storage.`,
    };
  }

  async factoryErase(
    request: FactoryEraseRequest,
  ): Promise<Readonly<OperationResult>> {
    if (request.confirmationText !== "ERASE") {
      return {
        ok: false,
        code: "CONFIRMATION_TEXT_MISMATCH",
        message: "Type ERASE exactly to arm factory erase.",
      };
    }

    if (!request.physicalConfirmed) {
      return {
        ok: false,
        code: "PHYSICAL_CONFIRM_REQUIRED",
        message: "Hold the physical button for 3 seconds to confirm erase.",
      };
    }

    this.state = {
      ...this.state,
      records: [],
      sessions: [],
      annotations: [],
      generation: this.state.generation + 1,
    };

    return {
      ok: true,
      code: "FACTORY_ERASE_COMPLETED",
      message: "Factory erase complete. Re-run setup and time configuration.",
    };
  }

  subscribeLive(handlers: LiveStreamHandlers): () => void {
    const timer = setInterval(() => {
      const latestSequence = this.state.records.at(-1)?.sequence ?? 100;
      const latestMonotonicStart =
        this.state.records.at(-1)?.monotonic_start_ms ?? 2_520_000;
      const latestRecord = parseAggregateRecord({
        ...fixtureRecordHistory[0],
        sequence: latestSequence + 1,
        interval_start: nowIsoSeconds(),
        monotonic_start_ms: latestMonotonicStart + 60_000,
        clock_quality: "host_set",
      });

      this.state = {
        ...this.state,
        records: [...this.state.records, latestRecord].slice(
          -this.state.retention.maxRecords,
        ),
      };

      handlers.onEvent?.({
        type: "aggregate",
        payload: latestRecord,
      });
    }, 5_000);

    handlers.onOpen?.();

    return () => {
      clearInterval(timer);
    };
  }
}
