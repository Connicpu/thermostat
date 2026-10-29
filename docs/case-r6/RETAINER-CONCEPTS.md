# Shell hold-down

The panel drops onto the carrier and sits there. Nothing locks it. The shell clips only to the backplate, in the snap window the case already has. Four bridges printed in the shell press the border land. One parametric file, `case/scratch/r6_shell_hold.scad`, drives the meshes, the slice and the figures.

The load is the cable plus a 5 g shock on the 5.5 g panel, **0.30 + 0.27 = 0.57 N**. Twice that is **1.14 N**. That 1.14 N is the held force after creep, not the load. Margin is held force divided by 0.57 N.

## The clips

The shell is not seated on a lip. The snap window runs from z 2.2 to 4.2 around the bump at 3.2. At the wall's inner face the 42° bump is 1.125 mm tall, so the shell can float **±0.44 mm** with the clips relaxed and the full 0.50 mm of engagement left for the life of the case.

The bridges push the shell out, onto the flank. Four clips on that flank, with friction 0.3, resist about **10.1 N per millimetre** of climb (E 1500). The apex is 0.56 mm of climb past first contact. Past that the shell unclips. At the thick corner the climb is **0.44 mm**, so the clips stay on the flank.

The sideways spring of one clip at a full 0.50 mm deflection is 3.67 N. The pull along the shell, for four clips, is about 6.8 N at friction 0.3 and 8.6 N at friction 0.2, and only at that full deflection. The press that walks the walls over the bumps is about 19–24 N. With the bridges adding their thick-corner force the press is about **24–28 N**. Pulling the shell off is the same clip job as the shipped case, about **13–18 N**. The bridges push the same way as that pull, so they do not add to it.

## The bridges

Four bridges, two on each long edge. Each is a fixed-fixed beam, **33 mm** between the pillars, **2.90 mm** wide and **0.80 mm** thick (four layers). The pillars are part of the shell. They enter the recess skin by 0.10 mm, so the bridge is not a loose part and it does not need a lock step.

The shell prints face down, the same way the shipped shell prints. The bridge spans between the pillars, so the first layer of the span is a bridge. The land is a 0.60 mm nub on the panel side of the beam, at the middle of the span, fused to the beam. Bending is along the beam, which is the extrusion direction. The stress across the layers is shear at the pillars, **0.34 MPa** at the thick corner.

The free land face, with the shell centred in the window, is at **z 22.74**. On the panel it is the only contact. It sits at **y 2.20–2.30** from the edge.

Two shoes, one on each long edge, centre the panel on the shell. Each is a 0.80 mm leaf standing 1.20 mm tall, preloaded 0.20 mm into the edge. The top leaf spans 20 mm (strain 0.48%). The bottom leaf spans 14 mm (strain 0.98%). The long-edge fences stand **1.05 mm** outboard of the outline, so the shoes can centre the panel when the shell is off the carrier by the lip clearance. The x-stops are unchanged.

## Where the panel sits

The carrier pads, the x-stop rails under the outline, and five added columns are all at **z 22.90**. The columns are solid from the PCB face up, and each one joins a rail or a pad that is already there, so the carrier is one piece and the added stops are not bridges. A short carrier (−0.15 mm) puts the panel back at 22.75.

| Corner | Board | Carrier | Panel | Shell print | Finger | Held after creep | Strain | Climb |
|---|---|---|---|---|---|---|---|---|
| Worst hold | −0.16 | −0.15 | 1.1 | +0.10 | −0.05 | 0.34 N | — | 0.03 mm |
| Thick | +0.16 | +0.15 | 1.3 | −0.10 | +0.05 | — | 0.98% | 0.44 mm |

243 corners (board ±0.16, carrier ±0.15, panel 1.1/1.2/1.3, shell print ±0.10, finger ±0.05). Every one still touches after 35% creep. E is 1500 N/mm², the warm-wall value. The same worst corner is 0.41 N at E 1800 and 0.47 N at E 2100. A further 0.10 mm of pad-to-pad warp leaves 0.23 mm of contact.

The worst held force is **0.34 N, 0.59×** the 0.57 N load. That is short of 2×. The 0.30 N cable is held. A 5 g knock at that corner lifts the panel **0.25 mm** and the bridges are still on it. The flank is the limit: a stiffer bridge climbs past the bump and the shell unclips before the held force reaches 1.14 N.

Sustained strain at the thick corner is **0.98%**. Pushing the shell to the inner end of the window during fitting bends the bridges 2.26 mm, **2.11%**, and that is only while the shell is being pressed on.

## J4

Free length 14.40 mm, straight run 4.69 mm, lower leg 0.50 mm. The panel back is the carrier stop.

| Carrier | Panel back | Inserted, at rest | Inserted, cable rise 0.08 mm |
|---|---|---|---|
| −0.15 | 22.75 | 3.98 mm | 3.85 mm |
| 0 | 22.90 | 3.74 mm | 3.61 mm |
| +0.15 | 23.05 | 3.51 mm | 3.37 mm |

The 0.08 mm rise is the 0.30 N cable against the four bridges at E 1500, with the preload gone and the short-term modulus left. All of those are over 3.15 mm.

If the modulus that remains after the set is the 35% value, the same cable lifts the panel 0.24 mm and the +0.15 carrier inserts **3.13 mm**. That is under 3.15 mm.

## Clearance

The panel back on a short carrier is 22.75 mm. A 10 N press bends the glass between the stops; the bend below is that deflection at the part. Nothing touches.

| Part | Top | Rest gap | Bend under 10 N | Minimum |
|---|---|---|---|---|
| U7 | 22.60 | 0.15 | 0.010 | 0.14 mm |
| U8 | 22.60 | 0.15 | 0.001 | 0.15 mm |
| D4 | 21.10 | 1.65 | 0.001 | 1.65 mm |
| C1/C41 | 21.10 | 1.65 | 0 | 1.65 mm |
| J1/C43 tails | 20.60 | 2.15 | 0 | 2.15 mm |
| D7 | 19.95 | 2.80 | 0.013 | 2.79 mm |
| D8 | 19.95 | 2.80 | 0.057 | 2.74 mm |
| R31 | 19.15 | 3.60 | 0 | 3.60 mm |
| R32, R42 | 19.25 | 3.50 | 0.001 | 3.50 mm |

U6 is not under the panel. U3's top is 21.75 (21.91 on a thick board). The bridges clear it by 1.37 mm with the shell at the inner end of the window and the board thick. The nearest bridge-to-carrier gap in the mesh is 0.16 mm. The beams clear the shell solid by 1.37 mm, and the pillars bite 0.10 mm into the recess skin.

At the thick corner the bridge top is 0.27 mm under the recess ceiling.

## The land

The shoes centre the panel, and the opened fences do not stop them. What is left is the glass width, ±0.10 mm on an edge, and ±0.05 mm on the printed land.

| | From the panel edge |
|---|---|
| Land | 2.05 to 2.45 |
| Bridge, worst | 0.05 outboard face, 3.25 inboard face |
| Window (active area minus 0.50) | 3.32 |
| Seal line | 1.52, and 1.92 at the worst of that stack |

The land stays on the border. The bridge stays on the panel, short of the window.

## Slice

Bambu Studio 02.08.02.61. Machine `Bambu Lab P2S 0.4 nozzle`, process `0.20mm Standard @BBL P2S`, filament `Generic PETG @BBL P2S`. The presets were flattened before the run: filament type PETG, nozzle 255 °C, bed `Textured PEI Plate` at 70 °C, infill 15%, 5 top and 3 bottom layers, layer 0.20 mm, supports off.

| Part | Layers | Bed | Nozzle wait | Bridge toolpaths | Floating-shell toolpaths |
|---|---|---|---|---|---|
| Shell with the bridges | 131 | M190 S70 | M109 S255 | 61 | 80 |
| Same shell without the bridges | 131 | M190 S70 | M109 S255 | 58 | 77 |
| Carrier with the stops at 22.90 | 26 | M190 S70 | M109 S255 | 1 | 7 |

The gcode header says `filament_type = PETG` and `curr_bed_type = Textured PEI Plate`. Neither 3mf contains a warning string. The three extra bridge toolpaths are the finger spans. The shipped shell already has 58 bridges and 77 floating-shell toolpaths of its own.

`case/scratch/r6_shell_fingers.3mf`, `case/scratch/r6_carrier_p2s.3mf`.

## Putting it together

1. Drop the panel onto the carrier. It sits on the pads at 22.90. The x-stops take the cable end. The fences take a hard shove in y. The tail enters J4. Lift it straight back out.
2. Set the shell on the backplate and press until the four clips click. The shoes centre the panel. The bridges meet the land.

Pull the shell off the clips and lift the panel out.

## What this does not reach

- **2× at the worst corner.** The held force there is 0.34 N, 0.59× the 0.57 N load. The cable is held. A 5 g knock lifts that corner 0.25 mm. A stiffer bridge unclips the shell before the held force reaches 1.14 N.
- **J4 after a crept modulus.** With the short-term modulus the worst insertion is 3.37 mm. With the 35% modulus left, the +0.15 carrier inserts 3.13 mm.
