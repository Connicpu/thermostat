# Retainer concept

Four PETG pads under the display panel push it up. A PETG plate on each station holds it down, and only the land of that plate touches the panel. The shell clips to the backplate. It does not touch the plate, the pads, or the display panel, and it is not a stop.

The pads are the stiff size. The cable allowance is 0.30 N until Connie measures the tail, and the springs are sized for 35% of the load remaining. Thicker sections are used wherever the shell allows them, because the printer is a Bambu Lab P2S with the stock 0.20 mm Standard profile and Generic PETG HF. No custom settings, no painted supports, no modifier meshes.

PETG only, apart from the four drywall-anchor screws. Fingers only. The screw bracket in `concept-c-screw-bracket.png` stays parked. The outboard-leaf sketch in `concept-option2-outboard.png` stays parked.

Sketches:

- `docs/case-r6/concept-pad-section.png`
- `docs/case-r6/concept-pad-plan.png`

Scratch, not the case:

- `case/scratch/r6_pad_concept_proto.scad`
- `case/scratch/r6_pad_seat.stl` — one station, eight separate bodies
- `case/scratch/r6_pad_coupon.stl` — pad, plate, clamp bar, and cap, each flat on the bed
- `case/scratch/r6_pad_print.3mf` — that coupon, supports off

`docs/case-r6/pad_calc.py` prints the force table. `docs/case-r6/pad_check.py` checks the four stations against the board parts.

## The cable

Connie, with the panel in hand and the board vertical: the tail takes almost no force to hold the gap. Fingertips do not resolve a few tenths of a newton, and that hold was not the seated fold at the 1.98 mm radius. The sign is unknown. An opening residual loads the plate. A closing residual loads the pads.

The design allowance is **0.30 N** until she measures it. 0.06 N remains plausible. It is not the number used to size the pads.

Shock is 5.5 g × 5 g = 0.270 N. With the 0.30 N allowance the load is 0.570 N. The pads are sized for twice that after only 35% of the elastic force remains. PETG at 4.5–15 MPa, 35–50 °C below Tg, can lose more than half of a sustained stress over years. 35% kept is the design value. 50% is not.

A scale check, if she wants the number: board face-down over a 0.01 g scale, panel face-down on a 4.45 mm spacer, tail seated in J4. Read the panel's weight first, with the tail slack. A higher reading is an opening tail. Repeat at 4.35, 4.45, and 4.60 mm, and again after a day. This concept does not wait on that reading.

## What the worst corner holds

E = 1800 N/mm². Four pads. Each beam is L = 18 mm, w = 2.4 mm, t = 3.00 mm (fifteen layers of 0.20 mm). The free nub top is z = 23.55.

The deflection window is 0.50 mm: panel thickness 0.20, pad print 0.10, and 0.20 between the carrier shoulder and the pad. The thin corner is δ = 0.20 mm. The thick corner is δ = 0.70 mm.

`k = E w t³ / (4 L³)`

| Pad thickness | k | Force at δ = 0.20 |
|---|---|---|
| 2.95 mm | 4.75 N/mm | 0.95 N |
| 3.00 mm | 5.00 N/mm | 1.00 N |
| 3.05 mm | 5.25 N/mm | 1.05 N |

A ±0.05 mm error in the 3.00 mm thickness changes the force, at the same deflection, by about ±5% (the cube of 2.95/3.00 and 3.05/3.00). The deflection window moves the force much more than the thickness does. The thin corner already includes both.

The web from the cap to the nub is 0.60 mm thick and 24 mm long, about 65 N/mm. In series with the thin pad the delivered force at δ = 0.20 is 0.89 N. Four pads at 35% retained hold 1.24 N.

| Cable allowance | Load | Held, worst corner | Margin |
|---|---|---|---|
| 0.06 N | 0.330 N | 1.24 N | 3.76× |
| 0.15 N | 0.420 N | 1.24 N | 2.96× |
| 0.30 N | 0.570 N | 1.24 N | 2.18× |

2.18× at 0.30 N clears 2×. A measurement at 0.06 N or 0.15 N only adds margin.

At a pad 0.05 mm thick and δ = 0.70, the sustained strain is 0.99%. Sliding the plate in adds 0.15 mm. That install case is 1.20%, under 2.4%. One station on the thick pad is about 4.5 N. Press the plate, not the display panel.

The shelf is the outboard edge of the plate bar. The cap is the lip, inboard of that shelf and still outboard of the panel edge. The nub is at y = 2.50. With the shelf near y = −1.5 and the cap near y = −0.8,

H / F = (2.50 − (−1.5)) / (−0.8 − (−1.5)) = 5.7.

The arm is that plan distance. It is not the 0.40 mm plate thickness. At the thick corner, F is about 3.7 N and H is about 21 N. Two posts, each 1.2 mm by 0.8 mm, carry it in compression: about 11 MPa, 0.6% of E, under the 1% interlayer limit.

## Layout

The contact plane is z = 24.30, the land underside. A 0.40 mm land puts its top at 24.70. The shell ceiling over the land is the original recess, z = 25.51. A 0.60 mm land would leave 0.10 mm of air at stack offset 0.51, so the land stays two layers thick. It is 1.20 mm wide, y = 2.00–3.20, which is three beads of a 0.40 mm nozzle. The worst seal line is 1.92 mm inboard of nominal and the worst active-area limit is 3.12 mm, so the land clears both.

The nub centre is y = 2.50, on the land centre. The nub is 4.0 × 1.2 mm. Its inboard edge is 3.10, 0.02 mm outside the worst active-area limit of 3.12. The web and the outboard bar are at z ≥ 24.50, so they do not meet the panel edge or the seal line.

| Panel | Back | Gap | Inserted | Air over the optos |
|---|---|---|---|---|
| 1.1 | 23.20 | 4.60 | 3.27 | 0.60 |
| 1.2 | 23.10 | 4.50 | 3.43 | 0.50 |
| 1.3 | 23.00 | 4.40 | 3.58 | 0.40 |

A tall carrier and a 1.1 mm panel insert 3.11 mm. A land 0.05 mm thin raises the panel 0.05 mm and the insertion is 3.03 mm, still at or above 3.0. That 0.05 mm is the rise budget. The cap is on the carrier, so its thickness does not add rise.

The land top is z = 24.76. Air at stack offset 0.51 is 0.24 mm, and 0.21 mm with 0.03 mm of tilt. The cap top is z = 25.16, under a local ceiling at 25.92. Air there is 0.22 mm after the same tilt. The pocket is only over the cap. The face skin there is 0.33 mm. Over the land the skin is the original 0.74 mm.

The beam bottom is z = 19.55. The PCB top is 18.60, so the tip has 0.95 mm of air, more than the 0.85 mm the thick corner and the slide-in need. Nothing is under the tip. The root sits in a 4 mm clamp. A 1.20 mm bar, printed flat, pins the root. The carrier has no ceiling bridge over the spring.

The backstop top is z = 22.75. The lowest service back, a 1.3 mm panel on a short carrier with a thick land, is 22.85, so the travel onto the stop is 0.10 mm. U6, U7, and U8 are MOC3063S opto-triacs, 4.0 mm tall, top at 22.60. The stop is 0.15 mm above that top. A 10 N press on the glass reaches the stop and does not reach the optos. U7 straddles the top edge at x 28.8–40.2. The backstop there is a rib on each side of U7, not a bridge across it. The panel is stiff enough that stopping the edge stops the glass over U7.

## Stations

Each plate is 22 mm, x = nub − 16 to nub + 6, in the seat mesh and on the plan. Nubs:

| Edge | Nub x |
|---|---|
| Top | 64 and 100 |
| Bottom | 72 and 100 |

The beams are inboard of the edge, y about 1.0–3.3 mm onto the panel, bottom at 19.55. That misses U7, the ESP32 module (its top is 21.75 and it stops at y 25.5, outboard of the beams), C1/C41, and the J1/C43 tails (they stop at y 59.5; the bottom beams start at 59.64). `pad_check.py` reports no clashes.

## Fingers

The shell is off.

1. Drop a pad in, root outboard, nub up. Drop the 1.20 mm clamp bar onto the root. The side walls keep it.
2. Snap the cap onto the two posts. The hooks are 1.2 mm. The cap stays on the carrier.
3. Set the panel on the nubs. The tail is already in J4.
4. Press one station, on the plate, about 4.5 N at the thick pad. Slide the plate in from the open end until the 1.2 mm detent clicks. The slot under the cap is open. The land is the only contact on the panel.
5. Repeat the other three stations.

To remove one, push its ear outboard. The ear is outboard of the panel edge. Slide the plate out the open end. The cap stays. The plate cannot walk out to the shell: the posts and the detent locate it, and the nearest carrier face is 0.21 mm inside the shell wall.

## Print

Connie's machine is a P2S. Bambu Studio and OrcaSlicer are not on this machine, so the slice is PrusaSlicer 2.9.2 with the stock P2S numbers that PrusaSlicer can set: 0.40 mm nozzle, 0.20 mm layers, PETG at 255 °C and a 70 °C bed, Arachne, supports off. No painted supports and no modifier meshes.

`r6_pad_coupon.stl` is the pad (nub up), the plate (land on the bed, frame in the same first layers), the 1.20 mm clamp bar, and the 0.60 mm cap. Filament 118.76 mm. No overhang perimeter. No floating object, no loose extrusions, no low bed adhesion.

The carrier station, feet down, is 45.20 mm of filament. No overhang perimeter. The spring is not under a printed bridge. The cap and the clamp bar are separate flat parts, so their stop faces are bed faces.

The land is 0.40 mm thick because the ceiling does not allow 0.60 mm there. It is 1.20 mm wide. The cap, the clamp bar, the posts, the detent, and the web are 0.60 mm or 1.20 mm.

## Gates

Checked across 1.1 / 1.2 / 1.3 mm, the panel shift, stack offset 0 / 0.16 / 0.26 / 0.51, carrier ±0.10, and pad ±0.05. The scratch checker covers the station footprints. The rest are the mesh and the slice.

- Pad tip clearance 0.95 mm, root engagement 4 mm, clamp bar 1.20 mm with no bridge on the spring.
- δ 0.20–0.70. Sustained strain 0.99%. Install 1.20%. Margin 2.18× at 0.30 N and 35% retained.
- Retention features 0.60 mm or 1.20 mm, except the land at 0.40 mm under the ceiling. The slice shows those thicknesses as real layers.
- Rise 0.05 mm from a thin land. Inserted length 3.03 mm at the tall print and a 1.1 mm panel.
- Plate contact is the land only, y 2.00–3.20. Nub centre at 2.50.
- Air at offset 0.51, with 0.03 mm tilt: 0.21 mm over the land, 0.22 mm over the cap.
- The retainer stops 0.21 mm inside the shell wall. The shell is not a stop in y.
- Opto air at rest on a short 1.3 mm panel is 0.30 mm. On the backstop it is 0.15 mm. A 10 N press ends on the stop.
- Install and removal are a slide through the open end of the cap, with the ear outboard of the panel.
- Station footprints miss every listed board part and pin tail.
- PrusaSlicer 2.9.2 of the coupon and of the carrier station: no overhang perimeter on the land or the nub.
