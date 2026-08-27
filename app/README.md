# Quiet Trace local dashboard plan

Status: application boundary and UX plan; no app project or successful build exists yet.

## Target

A responsive TypeScript/Vite web application is built into the firmware and served by Quiet Trace on the local network. It targets current mobile and desktop browsers at 320 CSS px and above. A fixture-data static preview supports development without hardware. No app store, account, hosted backend, or subscription is required.

## Responsibilities

- First-run physical-presence setup and clear connection/recovery guidance.
- Device name, local time source, room/session labels, retention, and calibration metadata.
- Live aggregate level with explicit calibrated/uncalibrated and quality status.
- Historical timeline, summary cards, session comparison, and local annotations.
- Versioned CSV/JSON export, backup/restore, selective delete, factory erase confirmation, and portable data documentation.
- Privacy screen that explains exactly what is and is not stored, plus current record/storage counts.
- Diagnostics limited to aggregate/quality/firmware counters—never raw microphone data.

## Setup flow

1. User powers the device and holds the physical setup button.
2. Dashboard connects through the temporary local setup network or USB-guided fallback.
3. User names the device, sets time, optionally provides local Wi-Fi credentials, and receives a randomized device credential.
4. Device closes setup after a timeout and redirects to normal authenticated local mode.
5. User starts a named logging session or leaves default rolling logging enabled.

The exact browser/platform behavior must be validated before claiming universal captive-portal or USB browser support.

## Data ownership

The authoritative aggregate history lives on the device. Browser local storage may keep non-sensitive UI preferences and a credential scoped to that device; it must not become the only copy of user data. Exports are user-initiated and versioned. Restores are validated before mutation. Wi-Fi credentials, device secrets, and browser tokens are excluded from data exports.

## Permissions

- Local-network access is required for the device-hosted dashboard.
- No phone/computer microphone, camera, contacts, location, notification, or background permission is requested in MVP.
- File-picker access occurs only when the user restores or downloads data.

## Accessibility

- Keyboard operability, visible focus, semantic landmarks/labels, and sensible heading order.
- Text/pattern/icon redundancy; color is never the only signal.
- Reduced-motion support and no required auto-animation.
- Scalable text, high-contrast themes, 44 CSS px target sizing, and chart data available as an accessible table/summary.
- Calibration and data-quality state shown in plain language near every level value.

## Protocol boundary

The app consumes only the versioned aggregate/config API in [`docs/protocol.md`](../docs/protocol.md). Types should be generated or checked against a committed schema. Fixture contracts enable app tests without physical hardware; those tests are not hardware evidence.
