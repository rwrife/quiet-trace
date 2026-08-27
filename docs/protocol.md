# Quiet Trace device/app protocol draft

Status: design contract for issue-driven implementation. Endpoint names and schemas may change before v1; no running server is claimed.

## Principles

1. Local-first: no internet dependency or cloud relay.
2. Aggregate-only: no endpoint, event, file, or debug command carries raw samples, encoded audio, reconstructable spectra, speech, transcripts, or speaker identity.
3. Physical-presence setup: privileged first setup/reset opens only after a button hold and expires automatically.
4. Versioned and bounded: every payload has a schema version, timestamps/sequence quality, length limits, and explicit error responses.
5. User-owned data: deterministic export, restore validation, retention, and erase.

## Transports

- Local HTTP/JSON under `/api/v1` for configuration and historical aggregates.
- Server-Sent Events at `/api/v1/live` for aggregate/status updates; no audio stream.
- USB serial command protocol for setup, diagnostics, aggregate export, erase, and recovery when Wi-Fi is unavailable.

TLS feasibility, local certificate UX, and authenticated LAN threat model must be resolved during architecture work. Until then, the device must be treated as accessible to other clients on the same trusted local network, and setup must not expose a universal default credential.

## Authentication assumptions

- A long physical press opens a short setup window and temporary local network.
- Setup generates a unique random device secret; it is not derived from MAC address/serial and is never committed to source.
- Normal API requests require that credential or an implementation-equivalent per-device token.
- Wi-Fi credentials and auth secrets never appear in logs, ordinary diagnostics, exports, or backups.
- Factory erase requires authenticated UI confirmation plus physical-button confirmation.
- Rate limits and bounded bodies protect the constrained device.

## Common envelope

```json
{
  "schema": "quiet-trace/v1",
  "device_id": "user-visible-random-id",
  "sequence": 1234,
  "time": "2026-08-27T05:00:00Z",
  "clock_quality": "synced",
  "quality_flags": []
}
```

`clock_quality` is one of `unknown`, `monotonic_only`, `host_set`, or `synced`. Consumers must tolerate missing wall-clock time while retaining monotonic sequence order.

## Aggregate record

Illustrative shape:

```json
{
  "schema": "quiet-trace/aggregate/v1",
  "sequence": 1234,
  "interval_start": "2026-08-27T05:00:00Z",
  "duration_ms": 60000,
  "level_eq_db": 47.2,
  "peak_window_db": 61.0,
  "histogram_counts": [2, 18, 33, 7],
  "calibration": {
    "state": "uncalibrated",
    "offset_db": null,
    "calibrated_at": null,
    "method": null
  },
  "quality_flags": ["uncalibrated"],
  "session_id": "office-morning"
}
```

Metric names are provisional. Firmware/UI must not present uncalibrated numeric values as certified dB(A), and must document weighting/window definitions before release. Histogram edges are configuration metadata, not a spectrum.

## Proposed endpoints

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/api/v1/status` | Firmware, storage, clock, calibration, overrun, clipping, and network summary. |
| `GET` | `/api/v1/live` | SSE aggregate/status events at a bounded rate. |
| `GET` | `/api/v1/records?after=&limit=` | Paginated aggregate records. |
| `GET` | `/api/v1/sessions` | Room/session metadata and summary counts. |
| `POST` | `/api/v1/sessions` | Start/name a local session. |
| `POST` | `/api/v1/annotations` | Add bounded user text linked to sequence/time. |
| `GET` | `/api/v1/export?format=json|csv` | Stream versioned aggregate data only. |
| `POST` | `/api/v1/restore/validate` | Validate backup without mutation. |
| `POST` | `/api/v1/restore/apply` | Apply previously validated backup with confirmation. |
| `DELETE` | `/api/v1/records` | Selective/all aggregate deletion with confirmation. |
| `PUT` | `/api/v1/config` | Retention, display, room labels, and allowed local settings. |
| `POST` | `/api/v1/calibration` | Store/clear reference comparison metadata. |

## Live event types

- `aggregate`: latest bounded-window or minute statistic, calibration and quality flags.
- `storage`: used/free records and pending flush state.
- `clock`: clock quality/source change.
- `diagnostic`: bounded counters such as overruns/clipping; no buffers.
- `annotation`: acknowledgment of user-entered marker.

## USB serial draft

Line-oriented commands with JSON responses:

- `status`
- `setup begin` (requires recent physical button)
- `wifi set` (secret input must be redacted from echo/logging)
- `time set`
- `export json|csv`
- `erase records|factory` (physical confirmation)
- `reboot recovery`

Parsing must use explicit limits and reject unknown/oversized fields. A production design may use CBOR/framing if line JSON proves unreliable; the behavior contract remains aggregate-only.

## Error model

Errors return a stable code, safe message, request correlation ID, and retry guidance. They never include credentials, raw sample contents, internal memory dumps, or user annotation text unrelated to the request.

## Compatibility

- Additive fields are ignored by older readers.
- Breaking changes increment the schema path/version.
- Exports carry firmware version, schema version, calibration metadata, and clock-quality semantics.
- Restore validates schema, size, checksums, ranges, and capacity before changing live data.
