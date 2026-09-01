# Revision-A part selection

Checked: 2026-08-31. Manufacturer datasheets are the electrical ground truth. Distributor observations are point-in-time evidence only; stock and price must be rechecked before ordering.

## Controller/module comparison

| Requirement | ESP32-S3-WROOM-1-N8R8 | ESP32-S3-MINI-1-N8 |
|---|---|---|
| Manufacturer / exact MPN | Espressif Systems / `ESP32-S3-WROOM-1-N8R8` | Espressif Systems / `ESP32-S3-MINI-1-N8` |
| Supply / source capability | 3.0–3.6 V; Espressif recommends an external supply capable of at least 0.5 A (datasheet Table 6-2) | 3.0–3.6 V; at least 0.5 A external supply (datasheet Table 6-2) |
| Native USB | GPIO19/module pin 13 is USB D−; GPIO20/module pin 14 is USB D+ (Table 2-1) | GPIO19/module pin 23 is USB D−; GPIO20/module pin 24 is USB D+ (Table 2-1) |
| Memory | 8 MB Quad-SPI flash + 8 MB Octal-SPI PSRAM | 8 MB Quad-SPI flash; no PSRAM |
| Operating limit | N8R8 is an R8 variant: −40 to +65 °C normally; up to +85 °C only when PSRAM ECC is enabled, with usable PSRAM reduced by 1/16 (Table 1-1 notes) | −40 to +85 °C (Table 1-1) |
| Package / assembly | 18.0 × 25.5 × 3.1 mm shielded SMD module with PCB antenna; castellated pads plus ground pad; reflow profile and land pattern are in Sections 9–10 | 15.4 × 20.5 × 2.4 mm shielded SMD module with PCB antenna; smaller, denser pad field; land pattern and reflow guidance are in Sections 9–10 |
| Antenna constraint | Module antenna must overhang the host-board edge when possible; otherwise preserve Espressif’s clearance/keepout in the module hardware design guidelines | Same class of PCB-antenna edge/keepout requirement, in a smaller module |
| Lifecycle evidence | Current Espressif datasheet v1.8 and orderable distributor listings; no EOL/NRND notice observed. Formal lifecycle/PCN status was not API-verified and remains a procurement recheck. | Current Espressif datasheet and an exact-MPN retail listing were found; authorized-distributor stock/price and formal lifecycle status are `TBD` because no authenticated catalog result was available. |
| Point-in-time availability | LCSC `C2913201`: 6,547 shown, USD 4.9831 each on 2026-08-30 | Exact-MPN listing found; live authorized stock quantity and unit price `TBD` on 2026-08-30 |

Sources:

- [ESP32-S3-WROOM-1/WROOM-1U datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf): Tables 1-1, 2-1, 6-2; Sections 8–10.
- [ESP32-S3-MINI-1/MINI-1U datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3-mini-1_mini-1u_datasheet_en.pdf): Tables 1-1, 2-1, 6-2; Sections 8–10.
- [DigiKey ESP32-S3-WROOM-1-N8R8 listing](https://www.digikey.com/en/products/detail/espressif-systems/ESP32-S3-WROOM-1-N8R8/15295891) reported orderable/ships today during the check.

**Selection:** `ESP32-S3-WROOM-1-N8R8`. Its 8 MB PSRAM provides margin for bounded volatile DSP buffers and dashboard/network work without allowing raw-audio persistence. The official KiCad symbol/footprint pair is mature, the larger pitch reduces assembly risk, and point-in-time stock was observed. The +65 °C normal upper limit is acceptable only for the product’s stated indoor home-office/shared-room environment; the design does not claim operation outside that context.

## Digital MEMS microphone comparison

| Requirement | TDK InvenSense ICS-43434 | Infineon IM69D130V01XTSA1 |
|---|---|---|
| Interface / timing | 24-bit I²S; high-performance sample rate 23–51.6 kHz and low-power 6.25–18.75 kHz; digital timing in Table 5 | PDM; operating clock bands centered at 768 kHz, 1.536 MHz, 2.4 MHz, and 3.072 MHz; allowable PDM clock 0.4–3.3 MHz (Table 4) |
| Supply / current | 1.65–3.63 V; 490 µA typical / 550 µA maximum high-performance, 230 µA typical / 300 µA maximum low-power, 20 µA maximum sleep | 1.62–3.6 V; mode-dependent current specified in electrical characteristics; exact rail budgeting must use the selected clock mode |
| Sensitivity / tolerance | −26 dBFS nominal, ±1 dB at 1 kHz/94 dB SPL | −36 dBFS nominal, ±1 dB at 1 kHz/94 dB SPL |
| Frequency response | 60 Hz–20 kHz | 28 Hz low-frequency roll-off; 20 Hz–20 kHz characterization |
| Acoustic overload / distortion | 120 dB SPL AOP at 10% THD | 130 dB SPL AOP at 10% THD; ≤1% THD through 128 dB SPL |
| Operating limit | −40 to +85 °C | −40 to +85 °C operating range (datasheet absolute/recommended tables) |
| Package / assembly | 3.50 × 2.65 × 0.98 mm, six-pad bottom-port LGA; Sn/Pb and lead-free reflow compatible; minimum 0.5 mm PCB sound hole and no wash ingress | 4.0 × 3.0 × 1.2 mm, five-pad bottom-port LGA; reflow/land pattern in package section |
| Lifecycle evidence | Exact part remains orderable at DigiKey and LCSC; no EOL/NRND notice observed. Formal manufacturer lifecycle API status was not available and remains a procurement recheck. | DigiKey listing explicitly reports `Part Status: Active`; orderable/ships-today listing observed. |
| Point-in-time availability | LCSC `C5656610`: 3,332 shown, USD 6.7694 each on 2026-08-30; DigiKey also reported orderable/ships today | DigiKey reported orderable/ships today on 2026-08-30; live quantity and unit price `TBD` |

Sources:

- [ICS-43434 datasheet rev 1.2](https://product.tdk.com/system/files/dam/doc/product/sw_piezo/mic/mems-mic/data_sheet/ds-000069-ics-43434-v1.2.pdf): Tables 1–6, Pin Configuration and Function Descriptions, Applications Information, Package Dimensions.
- [DigiKey ICS-43434 listing](https://www.digikey.com/en/products/detail/tdk-invensense/ICS-43434/6140298).
- [IM69D130 datasheet](https://www.infineon.com/dgdl/Infineon-IM69D130-DataSheet-v01_00-EN.pdf): Sections 1–4, especially Tables 1, 4, and 5.
- [DigiKey IM69D130V01XTSA1 listing](https://www.digikey.com/en/products/detail/infineon-technologies/IM69D130V01XTSA1/8030732).

**Selection:** `ICS-43434`. Native I²S reduces firmware integration risk, its ±1 dB sensitivity tolerance supports repeatable aggregate comparisons, and the 120 dB SPL AOP is adequate for the product’s indoor ambient scope. This selection does **not** make Quiet Trace calibrated or standards-compliant; calibration remains blocked on a documented physical reference-meter comparison.

## Selected support components

| Ref | Exact part / package | Datasheet-backed role and critical mapping |
|---|---|---|
| U2 | Diodes Inc. `AP2112K-3.3TRG1`, SOT-23-5 | 600 mA fixed 3.3 V LDO. Pins 1/2/3/4/5 = VIN/GND/EN/NC/VOUT. Input and output each have 4.7 µF; NC is explicitly no-connect. |
| U3 | ST `USBLC6-2SC6`, SOT-23-6; LCSC `C7519` | USB 2.0 ESD array. Pins 1↔6 are I/O1, 3↔4 are I/O2, pin 2 GND, pin 5 VBUS. The schematic routes each differential conductor straight through its corresponding pair. The exact ST catalog mapping was rechecked on 2026-08-31; live stock/price remain `TBD`. |
| J1 | GCT `USB4105-GF-A` | USB-C 2.0 receptacle. A4/A9/B4/B9 are VBUS; A6/B6 D+; A7/B7 D−; A5/B5 CC1/CC2 with separate 5.1 kΩ Rd; SBU pins no-connect. |
| F1 | BHFUSE `BSMD1206-050-16V`, 1206 | 0.5 A hold / 1 A trip resettable VBUS protection; not a mains fuse. |
| D2 | Nexperia `PESD5V0S1BA,115`, SOD-323 | Bidirectional 5 V TVS on protected USB VBUS. Pin 1 to `USB_5V`, pin 2 to GND; footprint is the exact SOD-323 variant. |
| SW1/SW2 | C&K `KMR221GLFS`, KMR2 | Active-low RESET and dual-purpose GPIO0 BOOT/SETUP_MARK controls. |
| D1 | Lite-On `LTST-C170KGKT`, 0805 | Status output through 1 kΩ. Status meaning must also use timing/pattern; never color alone. |

## USB power and thermal strategy

- The board is USB 5 V SELV only. `VBUS_RAW` enters through the 500 mA-hold resettable fuse F1; the protected `USB_5V` node has D2 TVS protection and feeds U2 plus U3's VBUS clamp reference.
- U2 is a 600 mA-rated fixed 3.3 V LDO with 4.7 µF input/output capacitors. It was selected for low part count and adequate burst capability, not because 600 mA continuous operation is thermally available in this enclosure.
- Datasheet static bound: the AP2112 SOT-25 package lists 184 °C/W junction-to-ambient with no heatsink. For a 5.0 V to 3.3 V drop, `P=(5.0-3.3)×I`. At 25 °C ambient, the no-heatsink estimate is 56.3/87.6/118.8/181.4 °C junction at 100/200/300/500 mA continuous load. At 40 °C ambient those estimates become 71.3/102.6/133.8/196.4 °C. The theoretical 150 °C junction boundary at 40 °C is about 352 mA continuous; this is not a recommended operating point.
- The ESP32 module's ≥500 mA source recommendation is a transient-capability requirement, not an allowed continuous LDO thermal load. Firmware must avoid sustained maximum-radio load, and issue #3 must provide generous U2 copper. Bring-up must measure 3V3 current and U2 temperature during worst-case Wi-Fi logging. If sustained current or temperature margin is unacceptable, replace U2 with a qualified buck regulator before fabrication release.
- These figures are static datasheet calculations only. No prototype, enclosure, airflow, USB source, or thermal behavior has been physically tested.

## Selected controller and microphone pin/footprint audit

- U1 uses official `RF_Module:ESP32-S3-WROOM-1` symbol and footprint.
  - Module pins 1, 40, 41 → GND.
  - Pin 2 → +3V3; pin 3 → EN.
  - Pins 4/5/6 (GPIO4/5/6) → `MIC_SD`, `MIC_WS`, `MIC_BCLK`.
  - Pins 13/14 (GPIO19/20) → USB D−/D+ through 22 Ω resistors and U3.
  - Pin 25 (GPIO48) → status LED; pin 27 (GPIO0) → BOOT/SETUP_MARK.
  - Pins 36/37 → UART RX/TX on the optional recovery header.
  - Every unused module pin has an explicit no-connect marker.
- U4 uses official `Sensor_Audio:ICS-43434` symbol and `Sensor_Audio:InvenSense_ICS-43434-6_3.5x2.65mm` footprint.
  - Pins 1/2/3/4/5/6 = WS/LR/GND/SCK/VDD/SD exactly as the manufacturer pin table.
  - LR is tied low, VDD is +3V3 with local 100 nF, and SD has the manufacturer-recommended 100 kΩ pull-down.
- These mappings were cross-checked against manufacturer pin tables and against the KiCad-generated netlist. PCB pad-net cross-verification remains an issue-#3 task because no PCB exists yet.

## Procurement cautions

- Stock and prices are snapshots, not promises. Recheck every line before ordering.
- Do not substitute LCSC `C2687116` for U3: that code resolves to a UMW implementation, not the selected STMicroelectronics part. Use exact ST code `C7519` or revalidate any alternate electrically and mechanically.
- Any BOM unit cost without an observed source remains `TBD`; no estimate was invented.
- `J2` is DNP by default.
- The BOM is not an acoustic calibration record, qualification report, or certification evidence.
