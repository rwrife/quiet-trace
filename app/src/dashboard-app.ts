import { resolveDataMode, type DataMode } from "./data-mode";
import {
  FixtureDeviceApi,
  HttpDeviceApi,
  type BackupValidationResult,
  type DeleteRequest,
  type DeviceApi,
  type ExportFormat,
  type OperationResult,
  type RetentionSettings,
} from "./api/device-api";
import type { AggregateRecord } from "./contracts/aggregate-record";
import {
  type AnnotationAckV1,
  type DeviceStatusV1,
  type RecordsPageV1,
  type SessionsResponseV1,
} from "./contracts/device-protocol";
import type { EventSourceLike } from "./live-stream";

interface DashboardState {
  status: Readonly<DeviceStatusV1> | null;
  recordsPage: Readonly<RecordsPageV1> | null;
  sessions: Readonly<SessionsResponseV1> | null;
  annotations: readonly Readonly<AnnotationAckV1>[];
  connection: "ready" | "connecting" | "error";
  message: string;
  error: string | null;
  pageCursor: number | null;
  cursorBackstack: number[];
  backupDraft: string;
  backupValidation: Readonly<BackupValidationResult> | null;
}

export interface DashboardAppOptions {
  mode?: DataMode;
  api?: DeviceApi;
}

const DEFAULT_PAGE_LIMIT = 8;

function escapeHtml(value: string): string {
  return value
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function formatMaybeIso(value: string | null): string {
  return value === null ? "Unknown" : value;
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

function formatDb(value: number): string {
  return `${value.toFixed(1)} dBFS`;
}

function calibrationLabel(state: DeviceStatusV1["calibration_state"]): string {
  switch (state) {
    case "reference_adjusted":
      return "Reference adjusted";
    case "invalid":
      return "Invalid calibration";
    default:
      return "Uncalibrated";
  }
}

function summarizeLiveRecord(
  page: Readonly<RecordsPageV1> | null,
): AggregateRecord | null {
  return page?.records.at(-1) ?? null;
}

function makeDefaultApi(mode: DataMode): DeviceApi {
  if (mode === "fixture") {
    return new FixtureDeviceApi();
  }

  return new HttpDeviceApi(
    fetch.bind(globalThis),
    (url) => new EventSource(url) as unknown as EventSourceLike,
  );
}

export class DashboardApp {
  private readonly mode: DataMode;

  private readonly api: DeviceApi;

  private readonly state: DashboardState = {
    status: null,
    recordsPage: null,
    sessions: null,
    annotations: [],
    connection: "connecting",
    message: "Loading dashboard data…",
    error: null,
    pageCursor: null,
    cursorBackstack: [],
    backupDraft: "",
    backupValidation: null,
  };

  private unsubscribeLive: (() => void) | null = null;

  constructor(
    private readonly root: HTMLElement,
    options: DashboardAppOptions = {},
  ) {
    this.mode = options.mode ?? resolveDataMode(new URL(window.location.href));
    this.api = options.api ?? makeDefaultApi(this.mode);
  }

  async init(): Promise<void> {
    this.render();
    this.unsubscribeLive = this.api.subscribeLive({
      onOpen: () => {
        this.state.connection = "ready";
        this.state.message =
          this.mode === "fixture"
            ? "Fixture stream active — synthetic aggregates are labeled."
            : "Device stream connected.";
        this.render();
      },
      onError: (error) => {
        this.state.connection = "error";
        this.state.error = error.message;
        this.render();
      },
      onEvent: (event) => {
        if (event.type === "status") {
          this.state.status = event.payload as DeviceStatusV1;
        }

        if (event.type === "aggregate") {
          if (
            this.state.pageCursor === null &&
            this.state.recordsPage !== null
          ) {
            const appended = [
              ...this.state.recordsPage.records,
              event.payload as AggregateRecord,
            ].slice(-DEFAULT_PAGE_LIMIT);
            const nextAfter = this.state.recordsPage.paging.has_more
              ? this.state.recordsPage.paging.next_after
              : (appended.at(-1)?.sequence ?? null);
            this.state.recordsPage = {
              ...this.state.recordsPage,
              records: appended,
              paging: {
                ...this.state.recordsPage.paging,
                next_after: nextAfter,
              },
            };
          }
        }

        this.render();
      },
    });

    await this.refreshAll();
  }

  private async refreshAll(): Promise<void> {
    this.state.connection = "connecting";
    this.state.error = null;
    this.state.message =
      this.mode === "fixture"
        ? "Loading synthetic aggregate fixture…"
        : "Connecting to local device…";
    this.render();

    try {
      const [status, recordsPage, sessions] = await Promise.all([
        this.api.getStatus(),
        this.api.getRecordsPage(this.state.pageCursor, DEFAULT_PAGE_LIMIT),
        this.api.getSessions(),
      ]);

      this.state.status = status;
      this.state.recordsPage = recordsPage;
      this.state.sessions = sessions;
      this.state.connection = "ready";
      this.state.message =
        this.mode === "fixture"
          ? "Fixture mode active — no hardware permissions requested."
          : "Connected to local device endpoints.";
      this.state.error = null;
    } catch (error) {
      this.state.connection = "error";
      this.state.error = error instanceof Error ? error.message : String(error);
      this.state.message =
        this.mode === "fixture"
          ? "Fixture failed to load."
          : "No local device API detected. Use setup + recovery guidance below.";
    }

    this.render();
  }

  private async loadNextPage(): Promise<void> {
    if (!this.state.recordsPage?.paging.next_after) {
      return;
    }

    this.state.cursorBackstack.push(this.state.pageCursor ?? 0);
    this.state.pageCursor = this.state.recordsPage.paging.next_after;
    await this.refreshAll();
  }

  private async loadPreviousPage(): Promise<void> {
    if (this.state.cursorBackstack.length === 0) {
      return;
    }

    const previous = this.state.cursorBackstack.pop() ?? 0;
    this.state.pageCursor = previous === 0 ? null : previous;
    await this.refreshAll();
  }

  private async submitAnnotation(form: HTMLFormElement): Promise<void> {
    const formData = new FormData(form);
    const note = String(formData.get("note") ?? "").trim();
    const sessionRaw = String(formData.get("session") ?? "");
    const sequenceRaw = String(formData.get("sequence") ?? "").trim();

    const ack = await this.api.postAnnotation({
      note,
      sessionId: sessionRaw === "" ? null : sessionRaw,
      sequence: sequenceRaw === "" ? null : Number(sequenceRaw),
    });

    this.state.annotations = [ack, ...this.state.annotations].slice(0, 16);
    this.state.message = "Annotation captured locally.";
    this.state.error = null;
    form.reset();
    this.render();
  }

  private triggerDownload(
    format: ExportFormat,
    body: string,
    contentType: string,
    filename: string,
  ): void {
    const blob = new Blob([body], { type: contentType });
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = filename;
    link.rel = "noopener";
    link.click();
    URL.revokeObjectURL(link.href);

    this.state.message = `Exported ${format.toUpperCase()} (${filename})`;
    this.state.error = null;
    this.render();
  }

  private async runRetentionUpdate(form: HTMLFormElement): Promise<void> {
    const formData = new FormData(form);
    const maxRecords = Number(formData.get("max_records") ?? "50000");
    const retentionDays = Number(formData.get("retention_days") ?? "30");

    const result = await this.api.setRetention({
      maxRecords,
      retentionDays,
    } satisfies RetentionSettings);
    this.consumeResult(result);
    await this.refreshAll();
  }

  private async runDelete(form: HTMLFormElement): Promise<void> {
    const formData = new FormData(form);
    const confirmed = String(formData.get("confirm_delete") ?? "") === "on";

    if (!confirmed) {
      throw new Error("Enable delete confirmation before removing records.");
    }

    const request: DeleteRequest = {
      sessionId: String(formData.get("session_id") ?? "") || null,
      beforeSequence: String(formData.get("before_sequence") ?? "")
        ? Number(formData.get("before_sequence"))
        : null,
    };

    const result = await this.api.deleteRecords(request);
    this.consumeResult(result);
    await this.refreshAll();
  }

  private async validateRestoreDraft(): Promise<void> {
    if (this.state.backupDraft.trim().length === 0) {
      throw new Error(
        "Paste or upload a backup JSON document before validation.",
      );
    }

    const validation = await this.api.validateBackup(this.state.backupDraft);
    this.state.backupValidation = validation;
    this.consumeResult(validation);
    this.render();
  }

  private async applyRestore(): Promise<void> {
    const ticket = this.state.backupValidation?.ticket;
    if (!ticket) {
      throw new Error(
        "Run backup validation first to receive a restore ticket.",
      );
    }

    const result = await this.api.applyBackup(ticket);
    this.consumeResult(result);
    await this.refreshAll();
  }

  private async runFactoryErase(form: HTMLFormElement): Promise<void> {
    const formData = new FormData(form);
    const confirmationText = String(formData.get("confirmation_text") ?? "");
    const physicalConfirmed =
      String(formData.get("physical_confirmed") ?? "") === "on";

    const result = await this.api.factoryErase({
      confirmationText,
      physicalConfirmed,
    });
    this.consumeResult(result);
    await this.refreshAll();
  }

  private consumeResult(result: Readonly<OperationResult>): void {
    if (result.ok) {
      this.state.message = result.message;
      this.state.error = null;
    } else {
      this.state.error = result.message;
    }
  }

  private render(): void {
    const sessions = this.state.sessions?.sessions ?? [];
    const records = this.state.recordsPage?.records ?? [];
    const liveRecord = summarizeLiveRecord(this.state.recordsPage);
    const annotationRows =
      this.state.annotations.length === 0
        ? '<li class="muted">No local annotations yet.</li>'
        : this.state.annotations
            .map(
              (entry) =>
                `<li><strong>${escapeHtml(entry.accepted_at)}</strong> — ${escapeHtml(entry.note)}</li>`,
            )
            .join("");

    const comparisonA = sessions.at(0) ?? null;
    const comparisonB = sessions.at(1) ?? null;
    const delta =
      comparisonA && comparisonB
        ? comparisonA.level_eq_avg_dbfs - comparisonB.level_eq_avg_dbfs
        : null;

    const sessionOptions = [
      '<option value="">All sessions</option>',
      ...sessions.map(
        (session) =>
          `<option value="${escapeHtml(session.session_id)}">${escapeHtml(session.label)}</option>`,
      ),
    ].join("");

    const statusSummary =
      this.state.status === null
        ? "No status payload received"
        : `${this.state.status.records} records · ${calibrationLabel(this.state.status.calibration_state)} · clock ${this.state.status.clock_quality}`;

    this.root.innerHTML = `
      <div class="app-shell">
        <header class="card">
          <h1>Quiet Trace local dashboard</h1>
          <p class="muted">Mode: <strong>${escapeHtml(this.mode)}</strong> · Connection: <strong>${escapeHtml(this.state.connection)}</strong></p>
          <p>${escapeHtml(this.state.message)}</p>
          ${this.state.error ? `<p class="error" role="alert">${escapeHtml(this.state.error)}</p>` : ""}
          <nav aria-label="Dashboard sections">
            <a href="#setup">Setup & recovery</a>
            <a href="#live">Live aggregates</a>
            <a href="#history">History</a>
            <a href="#data">Data controls</a>
            <a href="#privacy">Privacy</a>
          </nav>
          <button id="refresh-now" class="target-control" type="button">Refresh now</button>
        </header>

        <section id="setup" class="card" aria-labelledby="setup-title">
          <h2 id="setup-title">Setup, connection, and recovery</h2>
          <p>${escapeHtml(statusSummary)}</p>
          <ol>
            <li>Power from USB 5 V SELV only (indoor, dry use).</li>
            <li>Complete setup token flow, then set UTC time before trusting wall-clock fields.</li>
            <li>If Wi-Fi is down, use USB serial recovery and export commands.</li>
            <li>For ownership transfer, run factory erase and physically confirm on-device controls.</li>
          </ol>
          ${this.mode === "device" ? '<p class="muted">Expected local endpoints: /api/v1/status, /api/v1/records, /api/v1/sessions, /api/v1/live.</p>' : ""}
        </section>

        <section id="live" class="card" aria-labelledby="live-title">
          <h2 id="live-title">Live level + quality</h2>
          ${
            liveRecord
              ? `
            <dl class="metrics">
              <div><dt>Leq</dt><dd>${formatDb(liveRecord.level_eq_dbfs)}</dd></div>
              <div><dt>Peak 125 ms</dt><dd>${formatDb(liveRecord.peak_125ms_dbfs)}</dd></div>
              <div><dt>Calibration</dt><dd>${escapeHtml(liveRecord.calibration.state)}</dd></div>
              <div><dt>Clock quality</dt><dd>${escapeHtml(liveRecord.clock_quality)}</dd></div>
            </dl>
            <p>Interval: ${escapeHtml(formatMaybeIso(liveRecord.interval_start))} → ${escapeHtml(formatMaybeIso(deriveIntervalEnd(liveRecord)))}</p>
            <p>Quality flags: ${escapeHtml(liveRecord.quality_flags.join(", ") || "none")}</p>
          `
              : '<p class="muted">Awaiting aggregate events.</p>'
          }
        </section>

        <section id="history" class="card" aria-labelledby="history-title">
          <h2 id="history-title">History timeline & paging</h2>
          <table>
            <caption>Aggregate intervals (latest loaded page)</caption>
            <thead>
              <tr>
                <th scope="col">Sequence</th>
                <th scope="col">Interval end</th>
                <th scope="col">Leq</th>
                <th scope="col">Peak</th>
                <th scope="col">Flags</th>
              </tr>
            </thead>
            <tbody>
              ${
                records
                  .map(
                    (record) => `
                    <tr>
                      <td>${record.sequence}</td>
                      <td>${escapeHtml(formatMaybeIso(deriveIntervalEnd(record)))}</td>
                      <td>${formatDb(record.level_eq_dbfs)}</td>
                      <td>${formatDb(record.peak_125ms_dbfs)}</td>
                      <td>${escapeHtml(record.quality_flags.join(", ") || "none")}</td>
                    </tr>
                  `,
                  )
                  .join("") ||
                '<tr><td colspan="5" class="muted">No records loaded.</td></tr>'
              }
            </tbody>
          </table>
          <div class="button-row">
            <button id="page-prev" class="target-control" type="button" ${this.state.cursorBackstack.length === 0 ? "disabled" : ""}>Newer page</button>
            <button id="page-next" class="target-control" type="button" ${this.state.recordsPage?.paging.has_more ? "" : "disabled"}>Older page</button>
          </div>
        </section>

        <section id="sessions" class="card" aria-labelledby="sessions-title">
          <h2 id="sessions-title">Session and room comparison</h2>
          ${
            comparisonA && comparisonB
              ? `<p><strong>${escapeHtml(comparisonA.label)}</strong> average ${formatDb(comparisonA.level_eq_avg_dbfs)} vs <strong>${escapeHtml(comparisonB.label)}</strong> average ${formatDb(comparisonB.level_eq_avg_dbfs)} (Δ ${delta?.toFixed(1)} dBFS).</p>`
              : '<p class="muted">Need at least two sessions to compare.</p>'
          }
        </section>

        <section id="annotations" class="card" aria-labelledby="annotations-title">
          <h2 id="annotations-title">Annotations</h2>
          <form id="annotation-form">
            <label for="annotation-note">Note (1–240 chars)</label>
            <textarea id="annotation-note" class="target-control" name="note" maxlength="240" required></textarea>

            <label for="annotation-session">Session</label>
            <select id="annotation-session" class="target-control" name="session">${sessionOptions}</select>

            <label for="annotation-sequence">Optional sequence</label>
            <input id="annotation-sequence" class="target-control" name="sequence" type="number" min="0" step="1" />

            <button class="target-control" type="submit">Save local annotation</button>
          </form>
          <ul>${annotationRows}</ul>
        </section>

        <section id="data" class="card" aria-labelledby="data-title">
          <h2 id="data-title">Export, restore, retention, delete</h2>
          <div class="button-row">
            <button id="export-json" class="target-control" type="button">Export JSON (v1)</button>
            <button id="export-csv" class="target-control" type="button">Export CSV (v1)</button>
          </div>

          <form id="retention-form">
            <fieldset>
              <legend>Retention settings</legend>
              <label for="retention-max-records">Max records</label>
              <input id="retention-max-records" class="target-control" name="max_records" type="number" min="100" max="100000" value="50000" required />
              <label for="retention-days">Retention days</label>
              <input id="retention-days" class="target-control" name="retention_days" type="number" min="1" max="3650" value="30" required />
              <button class="target-control" type="submit">Apply retention</button>
            </fieldset>
          </form>

          <form id="delete-form">
            <fieldset>
              <legend>Selective delete</legend>
              <label for="delete-session">Session scope</label>
              <select id="delete-session" class="target-control" name="session_id">${sessionOptions}</select>
              <label for="delete-before-sequence">Delete records before sequence</label>
              <input id="delete-before-sequence" class="target-control" name="before_sequence" type="number" min="0" step="1" />
              <label><input name="confirm_delete" type="checkbox" /> I understand this only affects local aggregate history.</label>
              <button class="target-control" type="submit">Delete selected records</button>
            </fieldset>
          </form>

          <form id="restore-form">
            <fieldset>
              <legend>Backup restore</legend>
              <label for="restore-json">Backup JSON</label>
              <textarea id="restore-json" class="target-control" name="backup_json" rows="6" placeholder='{"schema":"quiet-trace/export/v1", ...}'>${escapeHtml(this.state.backupDraft)}</textarea>
              <div class="button-row">
                <button id="restore-validate" class="target-control" type="button">Validate backup</button>
                <button id="restore-apply" class="target-control" type="button" ${this.state.backupValidation?.ok ? "" : "disabled"}>Apply restore</button>
              </div>
              ${
                this.state.backupValidation
                  ? `<p>Validation: <strong>${escapeHtml(this.state.backupValidation.code)}</strong> — ${escapeHtml(this.state.backupValidation.message)}</p>`
                  : '<p class="muted">Validate first to get a restore ticket.</p>'
              }
            </fieldset>
          </form>

          <form id="factory-erase-form">
            <fieldset>
              <legend>Factory erase (physical confirmation required)</legend>
              <label for="factory-confirmation-text">Type ERASE</label>
              <input id="factory-confirmation-text" class="target-control" name="confirmation_text" type="text" autocomplete="off" required />
              <label><input name="physical_confirmed" type="checkbox" /> I physically held the hardware confirmation button.</label>
              <button class="target-control" type="submit">Run factory erase</button>
            </fieldset>
          </form>
        </section>

        <section id="privacy" class="card" aria-labelledby="privacy-title">
          <h2 id="privacy-title">Privacy and data boundaries</h2>
          <ul>
            <li>Aggregate-only payloads: no raw PCM/audio samples are persisted or exported.</li>
            <li>No cloud account, relay, analytics, advertising, or subscription path in this UI.</li>
            <li>Exports intentionally omit Wi-Fi credentials, browser tokens, and device secrets.</li>
            <li>Calibration labels are explicitly state-only; this dashboard does not claim standards compliance.</li>
          </ul>
          <p class="muted">Local counts — records: ${this.state.status?.records ?? 0}, sessions: ${sessions.length}, annotations: ${this.state.annotations.length}.</p>
        </section>

        <section class="card" aria-labelledby="permissions-title">
          <h2 id="permissions-title">Permissions and network behavior</h2>
          <p>This app does not request microphone, camera, geolocation, push, or background-sync permissions.</p>
          <p class="muted">Device mode uses local network endpoints only; fixture mode uses synthetic data only.</p>
        </section>
      </div>
    `;

    this.attachHandlers();
  }

  private attachHandlers(): void {
    const bindClick = (id: string, handler: () => Promise<void> | void) => {
      const element = this.root.querySelector<HTMLButtonElement>(`#${id}`);
      if (!element) {
        return;
      }

      element.onclick = async () => {
        try {
          await handler();
        } catch (error) {
          this.state.error =
            error instanceof Error ? error.message : String(error);
          this.render();
        }
      };
    };

    bindClick("refresh-now", async () => {
      await this.refreshAll();
    });

    bindClick("page-next", async () => {
      await this.loadNextPage();
    });

    bindClick("page-prev", async () => {
      await this.loadPreviousPage();
    });

    bindClick("export-json", async () => {
      const exportPayload = await this.api.exportData("json");
      this.triggerDownload(
        exportPayload.format,
        exportPayload.body,
        exportPayload.contentType,
        exportPayload.filename,
      );
    });

    bindClick("export-csv", async () => {
      const exportPayload = await this.api.exportData("csv");
      this.triggerDownload(
        exportPayload.format,
        exportPayload.body,
        exportPayload.contentType,
        exportPayload.filename,
      );
    });

    bindClick("restore-validate", async () => {
      const restoreJson =
        this.root.querySelector<HTMLTextAreaElement>("#restore-json");
      this.state.backupDraft = restoreJson?.value ?? "";
      await this.validateRestoreDraft();
    });

    bindClick("restore-apply", async () => {
      await this.applyRestore();
    });

    const annotationForm =
      this.root.querySelector<HTMLFormElement>("#annotation-form");
    annotationForm?.addEventListener("submit", async (event) => {
      event.preventDefault();
      try {
        await this.submitAnnotation(annotationForm);
      } catch (error) {
        this.state.error =
          error instanceof Error ? error.message : String(error);
        this.render();
      }
    });

    const retentionForm =
      this.root.querySelector<HTMLFormElement>("#retention-form");
    retentionForm?.addEventListener("submit", async (event) => {
      event.preventDefault();
      try {
        await this.runRetentionUpdate(retentionForm);
      } catch (error) {
        this.state.error =
          error instanceof Error ? error.message : String(error);
        this.render();
      }
    });

    const deleteForm = this.root.querySelector<HTMLFormElement>("#delete-form");
    deleteForm?.addEventListener("submit", async (event) => {
      event.preventDefault();
      try {
        await this.runDelete(deleteForm);
      } catch (error) {
        this.state.error =
          error instanceof Error ? error.message : String(error);
        this.render();
      }
    });

    const factoryEraseForm = this.root.querySelector<HTMLFormElement>(
      "#factory-erase-form",
    );
    factoryEraseForm?.addEventListener("submit", async (event) => {
      event.preventDefault();
      try {
        await this.runFactoryErase(factoryEraseForm);
      } catch (error) {
        this.state.error =
          error instanceof Error ? error.message : String(error);
        this.render();
      }
    });
  }

  dispose(): void {
    this.unsubscribeLive?.();
    this.unsubscribeLive = null;
  }
}
