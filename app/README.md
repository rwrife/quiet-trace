# Quiet Trace dashboard scaffold

A device-hosted TypeScript/Vite shell for the local aggregate-only dashboard. The default route is disconnected and never invents production sensor behavior. Add `?fixture=1` explicitly to display deterministic fixture data; the page labels that data as synthetic and uncalibrated.

## Pinned setup

Use Node.js `22.23.1` and npm `10.9.8`:

```bash
npm ci
npm run format:check
npm run lint
npm run typecheck
npm run test:run
npm run build
```

For local development:

```bash
npm run dev
```

No cloud account, analytics, phone/computer microphone, camera, contacts, location, notification, or background permission belongs in this app. The browser consumes only versioned aggregate/configuration contracts. Wi-Fi credentials and device/browser secrets are excluded from exports and fixtures.

Current tests cover the aggregate-only forbidden-field boundary and the explicit fixture-mode gate. They are fixture/static evidence, not browser-platform, firmware, microphone, calibration, bench, or field evidence.
