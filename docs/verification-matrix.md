# Verification matrix

Status: issue-5 static/simulation gates are passing; issue-6 bench evidence is pending. A planned check is not a passed check.

## Evidence classes

- **Static analysis:** source/configuration inspection, compiler/type/lint checks, KiCad ERC/DRC, schema and BOM validation. It cannot prove physical behavior.
- **Simulation or synthetic fixture:** deterministic generated inputs, host tests, mocked faults/network loss, and dashboard fixtures. It cannot prove microphone, enclosure, RF, USB electrical, flash endurance, or browser-platform behavior.
- **Bench measurement:** identified physical revision, firmware commit, instruments, setup, measured values, tolerances, and pass/fail criteria.
- **Field observation:** optional real-room workflow/usability observation. It is not calibration, certification, or legal/safety evidence.

Reports and releases must label evidence with one of these classes and must state unavailable evidence explicitly.

| Requirement / claim | Static gate | Simulation / fixture gate | Bench gate | Field gate |
|---|---|---|---|---|
| Raw audio never crosses DSP boundary | Typed interface review; forbidden-field/schema scan; crash/log config audit | Negative persistence/API/USB/export tests; buffer lifecycle instrumentation | Inspect flash/network/USB/log outputs during end-to-end run and induced fault | Not required |
| Aggregate math | Formula/code review; compiler warnings | Deterministic DC, sine, clipping, silence, histogram, missing-window vectors with documented tolerance | Compare digital framing and reported values against captured instrument/reference setup | Optional stability survey, not compliance |
| Calibration labeling | Schema/UI wording review | State/offset/invalid metadata tests | Documented reference-meter comparison with placement, method, date, revisions | Optional repeatability observation |
| Clock/sequence semantics | Schema/state-machine review | Reset, missing wall time, backward wall-clock jump, reconnect, wrap/corruption fixtures | Power-cycle and network-loss run | Optional long-duration observation |
| Seven-day retention target | Record-size/partition calculation | Ring wrap, migration, corruption, interrupted-write and capacity tests | Measured write rate, power-loss recovery, flash diagnostics | Optional soak observation |
| Setup authentication and erase | Threat-model/config review | Button capability timeout, random-secret, auth, rate/body limits, export exclusions | Physical setup/erase confirmation and USB recovery | Not required |
| Offline operation | Architecture/dependency review | Simulated Wi-Fi loss and no-internet integration | Remove AP/internet and confirm logging + USB export | Optional user workflow |
| Dashboard accessibility | Semantic/CSS/type/lint review; automated accessibility scan | Keyboard/component tests at 320 CSS px fixture viewport | Physical device-hosted browser connection | Manual keyboard/screen-reader checks across named browsers |
| USB 5 V SELV electrical behavior | Schematic/ERC/datasheet review | Power-budget calculation only | Rail/current/transient/ESD-safe functional measurements | Not required |
| Microphone framing and overload | Datasheet/pin/timing review | Synthetic driver framing, overrun, clipping fixtures | Logic-analyzer framing, acoustic overload/clipping observation | Optional room behavior |
| Antenna/acoustic enclosure constraints | Datasheet-backed KiCad/CAD rule review | RF/acoustic simulation only if actually run | Inspect clearances; compare enclosure/port configurations | Optional range/acoustic observation |
| Cost USD 35–60 target / USD 75 ceiling | Source-backed BOM and dated supplier snapshot | Not applicable | Purchased invoice may be recorded separately | Not applicable |
| Reproducible release | Pinned source/dependencies, clean build, checksums, licenses | Install/restore/package smoke tests | Flash/boot released artifact on revision A | Optional release workflow |

## Foundation CI contract

Every change initially runs:

1. Dashboard exact-lock install, Prettier check, ESLint, TypeScript, Vitest, and Vite production build.
2. Host C++ configure/build with warnings as errors and CTest.
3. ESP-IDF v6.1 ESP32-S3 configure/build in the pinned toolchain image.

These are static/synthetic checks only. They do not establish selected hardware, microphone input, calibration, acoustic accuracy, flash endurance, Wi-Fi interoperability, or physical USB behavior.

## Promotion rules

- “Implemented” requires merged source plus passing applicable static/synthetic gates.
- “Bench tested” requires a stored report naming board revision, commit, date, instruments, setup, raw measurement observations, and criteria.
- “Calibrated” requires the documented reference comparison; fixture offsets never qualify.
- “Field observed” must remain separate from controlled bench evidence.
- “Compliant,” “certified,” “occupational,” “medical,” “hearing safe,” “legal evidence,” “surveillance,” and “emergency” claims are prohibited.
