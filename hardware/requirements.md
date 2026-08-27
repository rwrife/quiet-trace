# Quiet Trace hardware requirements

Status: draft requirements for validation in backlog issue #1. A requirement is not test evidence.

## Electrical

| ID | Requirement | Planned verification |
|---|---|---|
| E-01 | Accept 5 V USB SELV input only; no battery or mains circuitry. | Schematic review and bench input check. |
| E-02 | Protect the USB input/data interface as required by selected connector/controller documentation. | Datasheet review, ERC, schematic inspection. |
| E-03 | Remain stable during controller Wi-Fi current transients with documented decoupling. | Datasheet calculation and oscilloscope rail capture. |
| E-04 | Acquire a digital MEMS microphone at a documented sample rate/word format without sustained overruns. | Synthetic driver test plus bench overrun counter. |
| E-05 | Expose labeled ground, input rail, regulated rail, reset/boot, and digital-audio test points. | Schematic/PCB inspection and continuity check. |
| E-06 | Preserve the controller module antenna keepout with no copper, components, or enclosure obstruction forbidden by manufacturer guidance. | Datasheet-backed PCB inspection and DRC/rule-area evidence. |
| E-07 | Use no persistent path capable of accepting raw sample arrays, encoded audio, transcripts, or reconstructable spectra. | Architecture review, schema tests, flash/API audit. |

## Acoustic and data quality

| ID | Requirement | Planned verification |
|---|---|---|
| A-01 | Label output uncalibrated until a reference-meter offset is stored with date and method. | UI/API tests and export inspection. |
| A-02 | Document microphone sensitivity tolerance, frequency response, acoustic overload point, and enclosure port constraints from the selected manufacturer datasheet. | Datasheet review with citations. |
| A-03 | Calculate aggregates in bounded RAM windows and overwrite/discard raw samples immediately after use. | Code review, unit test instrumentation, heap/flash audit. |
| A-04 | Store, at most, one aggregate record per minute by default: equivalent/average metric, bounded peak, histogram buckets, quality flags, annotation link, and calibration metadata. | Data-schema and export tests. |
| A-05 | Detect and report acquisition overruns, clipping, invalid calibration, clock uncertainty, and missing intervals. | Synthetic fixtures and bench injection. |
| A-06 | Define any weighting/filter approximation and numeric error bounds; do not claim IEC/ANSI compliance. | DSP fixture report and documentation review. |

## Mechanical

| ID | Requirement | Planned verification |
|---|---|---|
| M-01 | Fit a target enclosure no larger than 90 × 65 × 35 mm, excluding cable, unless documented measurement forces revision. | CAD/PCB measurement. |
| M-02 | Provide at least two secure mounting points and service access with common hand tools. | CAD/assembly review. |
| M-03 | Keep the microphone acoustic port unobstructed and separated from status/button/USB mechanical noise paths as the datasheet requires. | Datasheet-backed CAD section and assembled inspection. |
| M-04 | Provide visible reference designators, pin-1/polarity markings, board name/revision, USB/power labeling, and test-point labels. | Silkscreen audit. |

## Environmental and safety

| ID | Requirement | Planned verification |
|---|---|---|
| S-01 | Indoor dry use only, planning range 10–35 °C and 20–80% RH non-condensing; final operating limits cannot exceed the narrowest selected component rating. | BOM/datasheet audit. |
| S-02 | No certified, medical, occupational, legal-evidence, security, emergency, or hearing-safety claims. | Documentation/UI review. |
| S-03 | No raw audio storage/transmission and no sound-source, speech, or speaker identification. | Threat-model and release audit. |
| S-04 | Enclosure prevents casual contact with conductive parts while retaining ventilation/acoustic access. | Assembly inspection. |

## Connectivity, privacy, and recovery

| ID | Requirement | Planned verification |
|---|---|---|
| C-01 | Continue aggregate logging without internet and after local Wi-Fi loss. | Network-disconnect integration test. |
| C-02 | Offer USB serial setup/export/recovery when Wi-Fi is unavailable. | End-to-end bench test. |
| C-03 | Require physical presence to open a time-limited initial/setup session and never ship a universal default secret. | Protocol test and threat-model review. |
| C-04 | Exclude Wi-Fi credentials and authentication secrets from ordinary exports/backups. | Golden-file and negative tests. |
| C-05 | Support versioned CSV and JSON export plus selective/all-data erase. | Round-trip and deletion tests. |
| C-06 | Dashboard must remain usable at 320 CSS px, with keyboard navigation, visible focus, semantic labels, reduced-motion support, and patterns/text in addition to color. | Automated checks plus manual accessibility checklist. |

## Cost and sourcing

| ID | Requirement | Planned verification |
|---|---|---|
| B-01 | Target total prototype cost USD 35–60 and remain below USD 75, excluding tools, phone/computer, and reference meter. | Live BOM pricing snapshot before order. |
| B-02 | Record Manufacturer and exact MPN for every schematic part; validate package, ratings, pinout, lifecycle, and availability. | Schematic property and datasheet audit. |
| B-03 | Track enclosure, cable, fasteners, certified USB supply, and calibration/reference equipment separately as non-schematic items. | `bom/bom.csv` and non-schematic BOM review. |
| B-04 | Prefer hand-assemblable modules/parts for revision A and document any JLCPCB extended/hand-solder constraints without assuming availability. | Assembly/BOM review. |
