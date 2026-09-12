# Issue #6 DRC repair feasibility analysis (2026-09-12)

Related issue: <https://github.com/rwrife/quiet-trace/issues/6>
Evidence class: **STATIC_ANALYSIS** (KiCad 9 headless DRC + scripted geometry probes).
No bench measurement is made or implied in this document.

## Purpose

Issue #6 requires resolving or narrowly documenting every ERC/DRC exception
against revision A. This note documents the *why* behind the remaining PCB DRC
exceptions captured in
[`issue-6-prebench-drc-2026-09-08.rpt`](issue-6-prebench-drc-2026-09-08.rpt)
(re-verified identical on today's `main` in
[`issue-6-drc-recheck-2026-09-12.rpt`](issue-6-drc-recheck-2026-09-12.rpt)),
and records the concrete repair attempts made so a follow-up layout pass does
not re-explore the same dead ends.

## Verification environment

- Image: `parts-tally-kicad:9-arm64` (KiCad 9 pcbnew/kicad-cli), run pinned
  as `--user $(id -u):$(id -g)` with the worktree bind-mounted.
- Command (exit code 5 = violations present, captured, not hidden):

  ```text
  kicad-cli pcb drc --exit-code-violations --severity-error --severity-warning \
      --format report -o <report.rpt> hardware/kicad/quiet-trace.kicad_pcb
  ```

## Open DRC exceptions (re-verified 2026-09-12)

1. `shorting_items` / `solder_mask_bridge` / GND-zone `clearance` — the B.Cu
   `VBUS_RAW` entry run at x=1.32 (y 18.0→21.6) passes under the J1
   USB4105 shield tab S1 (thru-hole pad, `*.Cu` layers, absolute
   bbox x[0.845, 2.945] y[19.18, 20.18]). Real short; must be fixed.
2. `courtyards_overlap` R5 ↔ U2. Fixable by moving R5; see attempt C below.
3. `unconnected_items` — F.Cu GND zone reports missing connection (no GND
   stitching vias tie F.Cu/B.Cu pours). Fixable, but pad-via placement on a
   two-layer board should be decided in the same interactive pass as (1).

## Measured obstacles at the USB-C fanout (all coordinates absolute, mm)

| Obstacle | Net | Layers | Extent |
|---|---|---|---|
| J1 A12/B1 pads | GND | F.Cu | x[0.745,1.895] y[20.225,21.375] |
| J1 A9/B4 pads | VBUS_RAW | F.Cu | x[0.745,1.895] y[20.45,22.175] |
| J1 A1/B12 pads | GND | F.Cu | x[0.745,1.895] y[26.625,27.775] |
| J1 mounting lug (NPTH) | — | — | center (2.395,21.11) r=0.325 → x[2.07,2.72] y[20.785,21.435] |
| J1 mounting lug (NPTH) | — | — | center (2.395,26.89) r=0.325 |
| S1 shield tab (lower) | GND | **all Cu** | x[0.845,2.945] y[19.18,20.18] |
| S1 shield tab (upper) | GND | **all Cu** | x[0.845,2.945] y[27.82,28.82] |
| CC2 F.Cu stub + via | CC2 | F.Cu | stub y[22.224,22.374] x[1.32,2.519]; via ann. r=0.35 @ (2.519,22.299) → reaches y=21.949 |
| CC1 F.Cu via | CC1 | F.Cu | ann. r=0.35 @ (2.409,25.303) → reaches y=24.953 |
| USB_D_P fanout | USB_D_P_CONN | F.Cu | pads y[22.95,23.55]/[23.95,24.55]; via @ (2.100,23.25) r≈0.49 |
| USB_D_N fanout | USB_D_N_CONN | F.Cu | pads y[23.45,24.05]/[24.45,25.05]; via @ (3.500,24.75) r≈0.49 |
| CC2 B.Cu diagonal | CC2 | B.Cu | (2.519,22.299)→(4.060,20.758), w=0.2 |
| Design-rule min clearance | — | — | Default 0.20; hole clearance 0.25; DRU `VBUS_RAW` width 0.40–0.45 |

Key facts:

- The J1 signal pads are **F.Cu-only** SMD; the current design routes VBUS on
  B.Cu by tying the two F.Cu VBUS pads together with a B.Cu run that is the
  source of exception (1). The natural repair is to escape VBUS on **F.Cu**.
- On F.Cu, the 0.4 mm VBUS band (±0.20 clearance ⇒ 0.8 mm corridor) must pass
  between the lug NPTH + A12/B1 pad row on one side and the CC2 stub/via +
  USB data fanout on the other.
- Lower escape corridor requirement (south of CC2): band y[21.3,21.7]. Lug
  hole edge reaches y=21.435 and x=2.07; a 0.4 mm track connected to the pads
  at x≤1.895 must cross hole-clearance territory (0.25 mm from hole edge ⇒
  forbidden x[1.82, 2.97] at y≈21.1) with no same-net justification. **Sealed.**
- Upper escape corridor (north of CC1 via, y≥25.653 + clearance): blocks
  USB_D_P and USB_D_N fanout vias at x=2.10/3.50 y≈23.25/24.75 and crosses
  the A1/B12 GND pad row + second lug at the top. **Sealed.**
- B.Cu is not an escape layer for the fanout segment: S1 tabs are `*.Cu`
  pads (all copper layers), and the CC2 B.Cu diagonal already occupies the
  gap between the shield tab and the board's lower edge.

## Repair attempts made (scripted, deterministic, KiCad 9 pcbnew)

Each attempt applied scripted edits to a pristine copy of the board, then
re-ran real `kicad-cli pcb drc`. Only attempts that reached a DRC run are
listed:

| Attempt | Edit | Measured post-edit DRC |
|---|---|---|
| A — B.Cu hop at x=2.75 (pre-existing draft script) | Remove the B.Cu entry at x=1.32; hop east from the VBUS pads at y=21.45 to x=2.75, south to y=18.0, east along the old corridor | 6 violations: `solder_mask_bridge` and `clearance` 0.137 mm vs the CC2 F.Cu stub/via at (2.519,22.299), `hole_clearance` 0.000 mm vs the lug NPTH at (2.395,21.11), plus the R5-move silk pair below |
| B — F.Cu south lane y=21.5 (x 1.32→9.0, drop x=9.0 to F1) | Move the escape to F.Cu at the pad centerline; delete the whole B.Cu entry/corridor | 4 violations: `hole_clearance` 0.000 mm — the lane necessarily crosses the lug hole circle at (2.395,21.11); starting west of the lug is impossible because the VBUS pads sit west of it, and starting east of it is electrically disconnected (produces `unconnected_items`). `via_dangling` warning on the old (10.6,17) VBUS via after its B.Cu feeder was removed. The CC2 lane itself clears at y=21.5 (via annulus edge 21.949 vs band top 21.7 = 0.249) — the lug hole is the sole blocker |
| C — F.Cu north lane y=27.2 (over the top, x 1.32→9.0) | Escape north of CC1 instead of south | 12 violations: `tracks_crossing`/`shorting_items` against USB_D_P_CONN/USB_D_N_CONN tracks and their F.Cu fanout vias at (2.10,23.25)/(3.50,24.75), `solder_mask_bridge` + `shorting_items` on the A1/B12 GND pads, second lug `hole_clearance`, `clearance` 0.05 mm vs the USB_D_P via |

Additionally, every attempt moved R5 to (26.5, 21.5, rot 0) with rebuilt
+3V3/EN stubs. The `courtyards_overlap` R5↔U2 error was
cleared every time, **but** the move introduced two new warnings measured in
every post-edit DRC: `silk_overlap` (R5 silk segment at (26.26,22.02) vs the
R3 reference field at (27.0,22.27)) and `silk_over_copper`. R3's silk label
must be relocated in the same pass.

All attempts were reverted; `hardware/kicad/quiet-trace.kicad_pcb` on this
branch is **byte-identical to `origin/main`**.

## Conclusion and recommended path (for the follow-up layout pass)

The USB-C fanout region in revision A is over-packed for a two-layer board at
the current pad-escape order: the 0.4 mm VBUS escape cannot be threaded
between the shield tabs, mounting lugs, CC stubs, and USB data fanout by
nudging tracks alone. A correct fix changes the fanout plan itself, e.g.:

1. Move the two VBUS pads' escape first, immediately after the pad row:
   stub each VBUS pad with a short 0.45 mm F.Cu neck **before** the lug and
   then re-order the CC/USB data stubs to leave a clean south lane; or
2. Route VBUS on F.Cu between the USB data pairs and CC2 by moving the CC2
   F.Cu stub/via west of the data fanout via column (x<2.0 impossible; so
   move the data fanout vias east to a x≥3.6/4.2 column instead), giving the
   VBUS lane y[21.3,21.7] x[1.9,3.4]; or
3. Relocate D2/U3/F1 and widen the connector zone by moving the entire J1
   fanout field +2 mm east, which is the least surgical and touches the
   enclosure port cutout (mechanical/README constraints).

Whichever is chosen must also, in the same pass: rebuild the R5 pull-up with
an R3 silk/reference-field relocation (attempt C showed the label collision),
and add GND stitching vias to close the F.Cu/B.Cu zone connection item.
After any such change: re-run ERC, DRC, `verify_netlist.py`, `verify_pcb.py`,
BOM export, and re-export Gerbers for visual inspection per issue #7's gate.

## Blocker statement

This static analysis resolves none of the three error classes by itself; it
documents them narrowly with measurements so the remaining DRC work in #6 can
be executed as one focused interactive layout iteration rather than trial and
error. No fabrication output should be produced from revision-A copper until
these are closed (issue #7 explicitly gates on this).
