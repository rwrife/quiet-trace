# ADR-0001: Revision-A MVP architecture

- **Status:** Proposed for review
- **Date:** 2026-08-28
- **Decision owners:** Quiet Trace maintainers
- **Scope:** Foundation decision for GitHub issue #1

## Context

Quiet Trace needs enough local compute for digital microphone acquisition and bounded DSP, local history, a device-hosted dashboard, and USB recovery. Its defining constraint is stronger than “do not save recordings”: raw samples and reconstructable derivatives must be unable to cross the volatile DSP boundary. Revision A is USB-powered indoor equipment, not a certified meter or safety instrument.

Exact orderable controller, microphone, and protection parts remain a datasheet/BOM decision for issue #2. This ADR selects architecture classes without pretending that electrical pinout, RF, acoustic, lifecycle, or availability validation has happened.

## Decision

### Controller and module strategy

Use an **ESP32-S3 module or replaceable module board** supported by ESP-IDF. Revision A will not start with a bare RF SoC. The module approach reduces RF layout and assembly risk, retains Wi-Fi and native USB options, and leaves enough compute for fixed-window DSP. Issue #2 must compare at least two exact options and select an MPN from manufacturer documentation and current sourcing evidence.

Rejected for revision A:

- A Linux SBC: higher power, slower recovery, larger writable attack surface, and unnecessary media capabilities.
- A phone-only app: consumes a personal microphone, varies by phone, and weakens the visibly non-recording product boundary.
- A small MCU without networking/native USB headroom: would require more companion hardware and split the trust boundary.
- A bare ESP32-S3: increases RF, assembly, antenna, and power-integrity risk before the product behavior is proven.

### Microphone strategy

Use one **digital MEMS microphone** behind an acquisition interface. I2S is preferred when viable; PDM is permitted only if the selected controller path and microphone timing are both manufacturer-supported. Exact sample rate, word alignment, clock polarity, pinout, sensitivity, frequency response, overload point, acoustic-port geometry, and package are blocked on issue #2 datasheet validation.

An analog microphone/preamp is rejected for revision A because gain, bias, noise, clipping, and calibration would add an uncharacterized analog chain. Multiple microphones are rejected because they invite beamforming/source-localization scope and increase reconstructability risk.

### Power

Accept **USB 5 V SELV only** from a certified supply or computer. There is no battery, charger, mains input, PoE, actuator, alarm, or safety-critical output. The selected module/carrier must expose and validate its regulated rails, transient decoupling, USB protection, and recovery behavior.

### Processing and persistence

Microphone frames enter fixed-size volatile buffers owned by the acquisition/DSP path. DSP computes the contract in [metrics.md](../metrics.md), transfers only an aggregate record, and explicitly overwrites/releases each sample buffer. Persistence accepts typed aggregate records, never generic objects or byte streams.

Store versioned, checksummed, append-oriented aggregate records in an on-device flash ring. The minimum planning target is 7 days at one 60-second record per minute: 10,080 records. At the hard planning ceiling of 256 bytes per record that reserves 2,580,480 bytes (2.461 MiB), before partition/filesystem overhead. Actual partition size, wear policy, and power-loss behavior require implementation and target testing.

Removable storage is rejected for MVP because it expands exfiltration, corruption, connector, and enclosure scope. Cloud persistence is prohibited.

### Local transports and recovery

Use a versioned local HTTP/JSON API for request/response operations and bounded-rate Server-Sent Events for live **aggregates/status only**. Serve the static TypeScript/Vite dashboard from the device. Retain a line-bounded USB serial path for setup, aggregate export, erase, and recovery when Wi-Fi is unavailable.

First setup requires a recent physical long press and a time-limited setup window. The device creates a random per-device secret; there is no universal password or MAC-derived credential. Ordinary LAN requests are authenticated. Wi-Fi credentials and device/browser secrets never enter exports, diagnostics, logs, or crash artifacts. Factory erase needs authenticated confirmation plus physical confirmation.

HTTPS certificate/user-experience feasibility on a local device remains unresolved. Until a tested mechanism exists, documentation must treat the local LAN as a trust boundary and must not claim protection from a hostile same-LAN attacker. Internet, cloud relay, remote administration, analytics, advertisements, and accounts are rejected.

## Privacy invariant

Raw or encoded audio, sample arrays, waveform fragments, reconstructable spectra, speech, transcripts, speaker identity, and source identification may exist in no persistent, network, USB, log, diagnostic, browser, export, backup, or crash surface. Only bounded volatile acquisition/DSP buffers may hold microphone samples. See [privacy-threat-model.md](../privacy-threat-model.md).

## Consequences

- The host-testable domain contract and dashboard can proceed before exact hardware selection.
- Hardware drivers remain blocked on selected MPN datasheets; fixture mode must be explicit and cannot masquerade as sensor data.
- A single-point reference comparison can produce an offset-labeled estimate, but not weighting accuracy or IEC/ANSI compliance.
- Loss of Wi-Fi cannot stop aggregate logging; USB recovery remains mandatory.
- Seven-day retention is a capacity target, not measured flash endurance or recovery evidence.
- Bench, acoustic, RF, browser-platform, and field claims remain unavailable until the corresponding evidence exists.
