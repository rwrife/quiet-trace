# Quiet Trace local dashboard

A device-hosted TypeScript/Vite dashboard for aggregate-only monitoring and
local data operations. By default, the app attempts local device endpoints.
Use `?fixture=1` for deterministic synthetic data (clearly labeled fixture mode).

## Implemented scope (issue #5)

- Setup/connection/recovery guidance (including USB serial fallback)
- Live aggregate status and quality signal cards
- Paginated history timeline table
- Session comparison summary
- Local annotation workflow with bounded text input
- Versioned JSON/CSV export controls
- Backup validation + ticketed restore apply flow
- Retention settings and selective delete operations
- Factory erase UX requiring typed `ERASE` + physical confirmation checkbox
- Privacy + permissions disclosures with explicit aggregate-only boundary

## Pinned toolchain

- Node.js `22.23.1`
- npm `10.9.8`

## Verification commands

```bash
npm ci
npm run format:check
npm run lint
npm run typecheck
npm run test:run
npm run test:accessibility
npm run build
npm run check:bundle-size
npm run check:firmware-contract
```

## Security/privacy notes

- Raw microphone/PCM payloads are treated as forbidden contract fields and are
  rejected by semantic validators.
- Export payloads are aggregate-only and intentionally exclude credentials,
  Wi-Fi passwords, browser tokens, and firmware secrets.
- Device mode is local-network only. Fixture mode never implies calibrated
  real-world measurements.

## Evidence boundary

Automated tests in this package are software/fixture evidence. They do **not**
prove physical microphone behavior, hardware calibration traceability, or
mobile/browser-specific runtime behavior on real devices.
