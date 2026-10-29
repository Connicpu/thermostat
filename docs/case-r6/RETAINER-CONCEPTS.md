# Retainer concept

The display panel is held by the carrier. Four PETG pads under the panel push it up. A PETG plate on each long edge holds it down. The plate's underside is the contact, on the border. The shell clips only to the backplate. It does not touch the plate, the pads, or the display panel, and it is not a stop.

PETG only, apart from the four drywall-anchor screws in the backplate. No tools. Fingers only. The screw bracket in `concept-c-screw-bracket.png` is not developed here. It waits on Connie.

Sketches:

- `docs/case-r6/concept-pad-section.png` — section through one pad, and the beam
- `docs/case-r6/concept-pad-plan.png` — the panel, the four pads, the two plates
- `docs/case-r6/concept-option2-outboard.png` — the outboard leaf, as a second option

Scratch, not the case:

- `case/scratch/r6_pad_concept_proto.scad`
- `case/scratch/r6_pad_seat.stl` — one station: carrier, pad, plate, washer, panel, shell
- `case/scratch/r6_pad_coupon.stl` — the pad, the plate, and the washer, flat on the bed

`docs/case-r6/pad_calc.py` prints the numbers below.

## The cable

Connie has the display panel in hand: "The cable is very flexible, it requires almost no force to hold the panel at a fixed distance when the board is oriented vertically."

The board is vertical in that check, so the panel's weight is in the plane of the board. The force she is describing is the tail. The fold is the U under the panel, centerline radius 1.975 mm. A residual that opens the fold pushes the panel up into the plate, and the plate reacts it. A residual that closes the fold pulls the panel off the plate, and the pads react it. Her observation does not separate the sign. The magnitude is about zero.

The tail is 12.50 mm wide and 0.30 mm thick. The outer fiber at R = 1.975 mm is (0.15) / 1.975 = 7.6%. That is past yield for the polyimide. A fully elastic film at the Kapton HN modulus, 2.5 GPa, would hold the semicircle with about 14 N (`F = πEI / (4R²)`, `I = 0.0281 mm⁴`). The plastic moment of the 0.30 mm section, at a 2% yield strain and a shape factor of 1.5, is about 14 N·mm, and the matching force is about 5.6 N. That is the force to form the fold, not the force left in it once it is seated. The seated force is the allowance below.

The design allowance is 0.06 N, the low end of the 0.06–1.5 N band, kept because "almost no force" is not a measured zero. It is applied as a closing force, so the pads have a budget for it. The shock is 5.5 g × 5 g = 0.270 N. The load is 0.330 N. The pads are sized for at least twice that after 50% relaxation: 0.659 N.

A kitchen scale is optional. Stand the board on edge, support the panel at the seated gap on the scale, and read the force with the tail seated. The pads above do not wait on that reading.

## Layout

Coordinates are the case. The panel outline is x 29.46–108.46, y 26.24–62.94. The active area is x 32.46–99.36, y 30.06–59.12. The polarization-seal line is 1.52 mm inboard of the edge. The active-area margin used here is 0.50 mm, so the band on the long edges runs from 1.52 mm to 3.32 mm inboard. The shell window is 0.50 mm outside the active area, 3.32 mm inboard of the nominal edge at the panel front. The recess ceiling is z = 25.51 (`Z_in + 1.26`). The shell wall is 2.16 mm outboard of the edge at the low side of the recess and 2.50 mm outboard at the recess cut. The fences stand in that strip.

The contact plane is z = 24.30. It is the top of the carrier shoulder. The plate sits on that shoulder, so the plate's underside is that plane, and the plate's own thickness is above it. A 0.40 mm plate has its top at 24.70. Every panel thickness meets the same front:

| Panel | Back | Gap | Inserted, 0.50 mm leg | Air over the 4.0 mm optos |
|---|---|---|---|---|
| 1.1 | 23.20 | 4.60 | 3.27 | 0.60 |
| 1.2 | 23.10 | 4.50 | 3.43 | 0.50 |
| 1.3 | 23.00 | 4.40 | 3.58 | 0.40 |

A carrier printed 0.10 mm tall raises the 1.1 back to 23.30, gap 4.70, inserted 3.11 mm. A carrier printed 0.10 mm short lowers the 1.3 back to 22.90, and the opto clearance is 0.30 mm. Both are on the gate. The plate thickness does not move the panel: the shoulder does.

The land is y = 2.05–2.45 mm inboard of the nominal edge, 0.40 mm wide. The panel can sit 0.20 mm outboard of nominal (against the fence) or 0.40 mm inboard (36.50 mm panel, all of the 0.60 mm opening on one side). At that inboard shift the seal line is 1.92 mm inboard of nominal, and the land's outer edge at 2.05 mm clears it by 0.13 mm. At the outboard shift the active-area limit is 3.12 mm inboard of nominal, and the land's inner edge at 2.45 mm clears it by 0.67 mm. The window at the panel front is 3.32 mm inboard, 0.87 mm past the land.

On the panel, the pad and the plate share one centre, 2.25 mm inboard. The pad pushes up. The plate pushes down. They are equal, opposite, and on the same point in plan, so R/H = 1. There is no moment on the panel. The plate's own root moment stays in the plate, between the shoulder and the washer, outboard of the edge.

The washer is 0.20 mm, on the outboard part of the plate. Its ear is the same layer, outboard of the panel edge. A tooth on the washer sits in a groove in the stem, open toward the slide-in end, with 0.20 mm of play before the closed side. The end stop is 0.20 mm past the other end of the plate. The local ceiling over the washer is z = 25.80, which leaves 0.45 mm of the 2.00 mm face. The pocket runs from the shell wall to 0.20 mm inboard of the edge. It does not enter the window. Over the land the ceiling stays z = 25.51.

Air under the ceiling, plate top at 24.75 (the 0.40 mm plate printed 0.05 mm thick):

| Stack dz | Air over the plate | Air over the washer top at 25.00 |
|---|---|---|
| 0 | 0.76 | 0.80 |
| 0.16 | 0.60 | 0.64 |
| 0.26 | 0.50 | 0.54 |
| 0.51 | 0.25 | 0.29 |

The plate is clamped on the shoulder, so it does not swing. The bending rise at the land, at the thin-corner force, is about 0.01 mm. A 5° tilt across the 0.40 mm land is 0.03 mm. At dz = 0.51 the air is then 0.22 mm over the plate and 0.26 mm over the washer.

A gap that stayed open under the washer would lift the plate, and the panel with it, by the same amount. The amplification is 1. The 0.05 mm in the seat mesh is the parting that keeps the bodies apart. Service contact closes it. Left open, 0.05 mm of rise at the tall 1.1 case leaves 3.03 mm in J4.

## The pads

E = 1800 N/mm². Four pads. Each is a cantilever, L = 12.7 mm, w = 3.3 mm, t = 1.40 mm, with a nub on the free end. The nub is the only contact with the panel back. The free nub is at z = 23.45, which is 0.25 mm above the 1.1 back.

`I = 3.3 × 1.40³ / 12 = 0.754 mm⁴`

`k = 3 × 1800 × 0.754 / 12.7³ = 1.99 N/mm`

A pad printed 0.05 mm thin is t = 1.35 mm and 0.05 mm lower, so the 1.1 deflection is 0.20 mm and k = 1.78 N/mm. A pad printed 0.05 mm thick is t = 1.45 mm and the 1.3 deflection is 0.50 mm. These faces are on the bed and on the top of the nub, so the ×1.59 factor from a first layer printed in air does not apply. At ×1.59 on the thickness the 0.50 mm case would be 0.76% strain and 0.63 N, which is recorded and not the design.

The plate from the outboard clamp to the land centre is 2.45 mm, 0.40 mm thick, and 16 mm along the edge at one station. That cantilever is 31 N/mm. In series with the thin pad the delivered force is 0.338 N. Four pads after 50% relaxation are 0.675 N, 2.05× the 0.330 N load.

| Pad | Panel | δ | Strain | Force, one pad | Four pads, relaxed |
|---|---|---|---|---|---|
| nominal, 1.40 mm | 1.1 | 0.25 | 0.33% | 0.50 N | 0.99 N |
| nominal | 1.2 | 0.35 | 0.46% | 0.70 N | 1.39 N |
| nominal | 1.3 | 0.45 | 0.59% | 0.90 N | 1.79 N |
| thin, 1.35 mm | 1.1 | 0.20 | 0.25% | 0.36 N | 0.71 N |
| thin, with the plate | 1.1 | 0.20 | 0.25% | 0.34 N | 0.68 N |
| thick, 1.45 mm | 1.3 | 0.50 | 0.67% | 1.10 N | 2.21 N |

Strain is `1.5 × t × δ / L²`. The 0.67% row is the sustained case, under 1%. Sliding the plate in takes another 0.15 mm. On the thick pad and the 1.3 panel that is δ = 0.65 mm and 0.88%, under the 2.4% install gate. One edge is two pads: 2.4 N at the nominal 1.3 panel, 2.9 N on the thick pad.

The root moment in the beam is in the layer plane. The across-layer member is the stem, whose length is case z. At 1.10 N and the 2.45 mm plate span the moment is 2.7 N·mm. The shoulder and the washer form that couple across the 0.40 mm plate, 6.7 N. On the 1.00 × 0.65 mm stem alone that is 10 MPa, 0.58% of E, under the 1% interlayer limit. The shoulder is 7.5 × 1.25 mm and shares the couple.

A ledge 1.60 mm wide and 1.00 mm thick, with 0.70 mm of overhang, is 2099 N/mm by `k = 3EI / L³`. Bending stress in that ledge is in the layer plane. The across-layer check is the stem above.

## Fingers

The shell is off for both jobs.

1. Drop each pad into its trench, the root under the lip, the nub up.
2. Set the panel on the nubs. The tail is already in J4.
3. Press one long edge down, about 2.4 N, until the front is just under the plate. Slide the plate in from the open end until it meets the end stop and the tooth clicks. 0.20 mm of play remains at the tooth.
4. Do the other edge the same way.

To take it apart, push the ear toward the open end. The ear is outboard of the panel edge, in the washer's own layer, so the push is not on the display panel. Slide the plate out. Lift the panel off the nubs, then the pads out of the trenches.

## Slice

PrusaSlicer 2.9.2, the Debian `2.9.2+dfsg-1` binary. The coupon is `case/scratch/r6_pad_coupon.stl`: the pad with its nub, the 0.40 mm plate, and the 0.20 mm washer, each on the bed. Nozzle 0.40 mm, layers 0.20 mm, Arachne, PETG. The gcode sets 240 °C and an 80 °C bed. Supports off. Filament 46.29 mm. Estimated time 1 m 20 s.

No layer is an overhang perimeter. The log has no floating object, no loose extrusions, and no low bed adhesion.

- z = 0.2 is external perimeter and solid infill. That is the plate's underside, the bearing face, and the washer's stop face, both on the bed. It is also the pad's bottom.
- The plate's top is z = 0.4, top solid infill.
- The nub is the top 1.00 mm of the pad. z = 2.4 is the preload face: perimeters that fill the 4.0 × 2.4 mm nub. There is no bridge under it.

## Option 2

`concept-option2-outboard.png` is the other arrangement. A leaf 1.6 mm wide and 10 mm long sits outboard of the panel edge, where the strip is about 2.2 mm. A rigid finger reaches in over the land. A fixed pad under the finger is the upward bearing. In plan the shelf, the lip, and the finger are on one line, with the lip between the other two, so the leaf's free body closes in x and in y. At the same 1% strain and the same 0.20 mm thin-corner deflection the leaf is about t = 1.33 mm and about 0.34 N. It is not the one to build. The pad under the panel is, because its contact faces print on the bed and on a top surface, and the panel front stays on one plane.

## Gates to add

All of these read the assembled mesh, at 1.1, 1.2, and 1.3 mm, at the panel's inboard and outboard shift, and at stack dz of 0, 0.16, 0.26, and 0.51 mm.

- Plan-view R/H from the nub and the land. Both positive, and the two centres within 0.2 mm in x and in y.
- Seat-gap rise. A gap under the washer lifts the panel 1:1. At the tall print and the 1.1 panel the inserted length stays at or above 3.0 mm, including that rise. The tooth play is 0.20 mm and does not lift the panel.
- Stem interlayer stress, `F / A` or `Mc / I` from the stem section, at the thick-pad 1.3 force. The gate is 1% of E.
- Air to the shell ceiling over the plate and over the washer, at every stack dz, with 0.03 mm of tilt on the land. The floor is 0.21 mm.
- Land against the 1.52 mm line, the active area minus 0.50 mm, and the window, at the worst panel shift.
- A sweep of the slide-in and the slide-out, shell off, ear outboard of the panel edge, tooth leaving through the open end.
- A 2.9.2 slice of the coupon. No overhang perimeter on the plate's bed face or on the nub's top face.
