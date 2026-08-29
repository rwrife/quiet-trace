# Privacy threat model and data-flow inventory

Status: foundation threat model for issue #1. It defines implementation gates; it is not penetration-test or hardware evidence.

## Assets and adversaries

Protected assets are conversations and acoustic content, Wi-Fi credentials, device authentication secrets, aggregate history, annotations, calibration metadata, and user control of export/erase. Consider accidental developer leakage, malformed local clients, curious or hostile same-LAN clients, a person with temporary USB access, corrupted flash/import data, and dependency/build compromise.

A determined owner with physical flash-debug access is outside the MVP confidentiality boundary, but physical access must still not reveal raw audio because raw audio is never persisted. Internet attackers should have no route because there is no cloud service, inbound relay, or required internet connection.

## Data classes and allowed surfaces

| Data class | Volatile DSP RAM | Flash | HTTP/SSE | USB | Logs/crash | Browser | Export/backup |
|---|---:|---:|---:|---:|---:|---:|---:|
| Raw samples / waveform / encoded audio | **Bounded only** | Forbidden | Forbidden | Forbidden | Forbidden | Forbidden | Forbidden |
| Ordered/reconstructable spectra or fine time series | Forbidden outside active calculation | Forbidden | Forbidden | Forbidden | Forbidden | Forbidden | Forbidden |
| One-minute aggregate + quality flags | Allowed | Allowed | Allowed | Allowed | Counters/IDs only | Allowed | Allowed |
| User annotation/session labels | Allowed | Allowed | Allowed | Allowed | Request-independent text forbidden | Allowed | Allowed |
| Calibration metadata | Allowed | Allowed | Allowed | Allowed | State only | Allowed | Allowed |
| Wi-Fi credentials | Setup buffer only, zeroized | Device configuration only | Setup write only; never returned | Setup write only; input redacted | Forbidden | Never exported; token storage separate | Forbidden |
| Device authentication secret/token | Auth buffer only, zeroized | Protected device config | Presented/used only as required | Setup/recovery only | Forbidden | Per-device credential storage only | Forbidden |
| Aggregate diagnostics | Allowed | Bounded counters | Allowed | Allowed | Allowed without private payloads | Allowed | Optional documented counters |

“Forbidden” applies to normal operation, diagnostics, debug builds, assertions, core dumps, panic handlers, test snapshots, telemetry, and support bundles.

## Data flow

```text
microphone
  -> bounded acquisition buffer
  -> DC/weighting/energy calculation
  -> minute accumulator (energy/counts only)
  -> typed AggregateRecordV1
     -> checksummed flash ring
     -> authenticated local HTTP/SSE
     -> bounded USB serial
     -> device-hosted dashboard
     -> user-initiated aggregate export

setup button -> expiring setup capability -> credential/config writer
                                           (separate from aggregate/export path)
```

The sample buffer has one owner. After aggregation, it is overwritten before reuse/release. The accumulator cannot reconstruct samples: it holds scalar energy totals, a maximum, unordered histogram counts, and quality counters. Persistence/network/USB serializers accept typed aggregate/config DTOs rather than arbitrary maps or byte arrays.

## Threats and controls

| Threat | Required controls | Verification gate |
|---|---|---|
| Raw samples reach storage/API/logs | Typed aggregate-only interfaces; no generic blob fields; explicit overwrite; forbidden-key/schema tests; flash/network/USB audit | Host/target negative tests plus physical end-to-end inspection |
| Spectrum/fine series becomes reconstructable | No FFT/spectral endpoint; unordered coarse histogram only; bounded live cadence; threat review for every schema change | Contract tests and release schema audit |
| Debug/crash path leaks buffers | No sample logging; disable/minimize production core dumps; panic path cannot serialize sample memory; redact secrets | Build-config audit, forced-fault target test, image/string inspection |
| Same-LAN client reads or erases data | Random per-device credential, authenticated normal API, request/body/rate limits, origin policy, physical confirmation for factory erase | Protocol tests and LAN security test; TLS limitation disclosed |
| Setup is opened remotely/default secret guessed | Recent long press creates expiring capability; random secret; no MAC-derived or universal credential; close setup after timeout/reset | State-machine and entropy-source tests; bench button timing |
| Credentials leak via export/restore/logging | Separate config schema/partition; allowlisted export fields; redacted input; golden negative tests | Export/backup/log/USB inspection |
| Malformed import corrupts history | Size/schema/checksum/range/capacity validation before mutation; transactional apply; unknown breaking version rejected | Fuzz/property and power-loss tests |
| Flash rollback/corruption fabricates continuity | Store generation, monotonic sequence, checksums, migration and reset-recovery flags | Corruption and interrupted-write injection tests |
| Fixture data is mistaken for hardware evidence | Explicit `?fixture=1`; disconnected production default; visible fixture banner; no synthetic source in production target | App tests and release review |
| Supply-chain code adds exfiltration | Exact dependency lock, review, vulnerability scan, reproducible build, no analytics/cloud dependencies | CI audit and release SBOM/notices |
| Browser data outlives device erase | Browser stores only scoped token/preferences; UI clears local state during confirmed erase; aggregate history remains device-authoritative | Browser storage and erase tests |
| Annotation causes injection/private log spill | Bounded UTF-8 text, text rendering (not HTML), no unrelated annotation in errors/logs | XSS/length/error tests |

## Privacy review triggers

Any new persistent field, endpoint, SSE event, USB command, diagnostic payload, crash facility, microphone mode, live cadence, export format, analytics dependency, remote access path, or multi-microphone processing requires threat-model review and negative boundary tests. Source identification, speech, speaker identity, recordings, spectra, and cloud relay are product non-goals and cannot be added as ordinary feature scope.

## Residual limits

- Plain local HTTP may expose authenticated traffic to a capable hostile LAN observer until a usable, tested transport-security design exists. Users must be told to use a trusted local network.
- Aggregate levels and user annotations can still reveal occupancy routines; access control, deletion, and minimal retention matter even without audio.
- Buffer overwrite reduces accidental reuse but does not prove that optimized code or hardware DMA retains no transient copy. Target inspection and controlled-memory tests are required.
- No penetration test, bench test, field observation, or certification has occurred at foundation stage.
