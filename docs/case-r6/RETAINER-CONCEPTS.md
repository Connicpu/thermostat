# Shell hold-down

The panel drops onto the carrier. The shell clips to the backplate. Four bridges printed in the shell press the border. Nothing locks the panel. One file, `case/scratch/r6_shell_hold.scad`, drives the meshes, the slice and the figures.

Capture is the requirement. Preload above twice the 0.57 N load is not required. The 0.57 N is the 0.30 N cable plus a 5 g knock on the 5.5 g panel.

## The shell and the clips

The snap window runs from z 2.2 to 4.2 around the bump at 3.2. At the wall's inner face the 42° bump is 1.125 mm tall, so a shell with no bridge force can sit **±0.44 mm** in that window. The shell rim is on the wall at the window centre, so the shell does not move toward the wall past that centre.

The bridges push the shell out onto the flank. Four clips there, friction 0.3, E 1500, resist **10.1 N per millimetre** of climb. The bump apex is 0.56 mm of climb past first contact. The thickest corner climbs **0.29 mm**, so the clips stay on the flank.

A clip's sideways force at a full 0.50 mm deflection is 3.67 N. Walking the four walls over the bumps takes about 19–24 N. At the thick corner the bridges add about 3 N, so the press is about **22–27 N**. Pulling the shell off is about **13–18 N**. The bridges push in that same direction, up to about 3 N.

## The bridges

Each bridge is a fixed-fixed beam, **0.80 mm** thick (four layers) and **1.20 mm** wide, printed in the shell. The pillars enter the recess skin by 0.10 mm. The shell prints face down. The span's first layer is a bridge along the edge. The land is a **0.45 mm** nub on the beam centreline, so the load does not twist the beam. There is no 0.90 torsion factor.

The free land, shell centred in the window, is at **z 23.72**. The beam bottom is 0.45 mm above that. At every stack corner the pillars stay at least **0.24 mm** off the panel, and the beam stays **0.27 mm** under the recess ceiling.

| | Left bridges | Right bridges |
|---|---|---|
| Span between pillars | 33 mm | 30 mm |
| Land centre, board x | 48.6 | 100.0 |

The right-hand lands sit toward the cable end so a lift there has a short lever.

## Where the panel sits

The carrier pads and the added columns top out at **z 22.90**, which is 0.15 mm under `Z_panel_b`. The columns are solid from the PCB face. A tie at 1.20 mm joins the top-left column to the spine. The long-edge fences stand **0.20 mm** off the outline.

A nominal panel has **±0.20 mm** of room between those fences. Ribs on the shell stand 0.15 mm off the outer face of each fence, so the shell and the carrier share that y. With a short panel sitting on the bottom fence, and the shell 0.15 mm off the rib, the land is:

| Edge | Distance from the panel edge |
|---|---|
| Top | 1.95 to 2.85 |
| Bottom | 2.35 to 3.05 |

The active area starts 3.82 mm from the edge. The bridge's inboard face, with the panel against the fence, is 3.55 mm from that edge.

## Capture at the 243 corners

Board ±0.16, carrier ±0.15, panel 1.1 / 1.2 / 1.3, shell print ±0.10, bridge ±0.05. E 1500.

| | |
|---|---|
| Corners | 243 |
| Land touching | 81 |
| Gap, land clear of the panel | 162 |
| Pillar air, minimum | 0.24 mm |
| Ceiling air, minimum | 0.27 mm |
| Climb, maximum | 0.29 mm |
| Corners past the bump apex | 0 |

A gap at a thin corner is acceptable. The panel is still between the stops and the bridges.

## J4

Free length 14.40 mm, straight run 4.69 mm, lower leg 0.50 mm. Rest insertion at carrier −0.15 / 0 / +0.15 is 3.98 / 3.74 / 3.51 mm.

After the preload is gone, the cable at the outline (x 108.46) tips the panel. The 5 g knock is another 0.27 N at the panel centre. The cable case uses the 35% modulus. The knock uses the short-term modulus. Stops under the panel keep the far end from dropping.

| Carrier | Cable lift | J4, cable | Cable + 5 g lift | J4, cable + 5 g |
|---|---|---|---|---|
| −0.15 | 0.10 mm | 3.82 mm | 0.03 mm | 3.94 mm |
| 0 | 0.10 mm | 3.59 mm | 0.03 mm | 3.70 mm |
| +0.15 | 0.10 mm | 3.35 mm | 0.03 mm | 3.47 mm |

The worst is **3.35 mm**. The floor is 3.00 mm. The 3.15 mm figure is the target while preload is still there. It is not a gate.

### Cable-end lip, not in this carrier

A lip on the carrier over the cable end, 0.15 mm above a 1.20 mm panel, with a 45° underside, would let that end tilt in and then drop. Across panel 1.1–1.3 and a lip printed ±0.05 mm the gap runs from **0.00 to 0.30 mm**. The thick corner can touch the lip. The thin corner can open to 0.30 mm, which is more lift than the bridges already allow (0.10 mm). The lip is not in the sliced carrier. J4 above is the design without it.

## Clearance

Short carrier, panel back at 22.75. A 10 N press with the preload gone, the panel rigid and free to tip:

| Part | Rest gap | Tip | Minimum |
|---|---|---|---|
| U7 | 0.15 | 0.002 | 0.15 mm |
| U8 | 0.15 | 0.006 | 0.14 mm |
| D4 | 1.65 | — | 1.65 mm |
| C1/C41 | 1.65 | — | 1.65 mm |
| J1/C43 tails | 2.15 | — | 2.09 mm |
| D7 | 2.80 | — | 2.80 mm |
| D8 | 2.80 | — | 2.80 mm |
| R31 | 3.60 | — | 3.60 mm |
| R32 | 3.50 | — | 3.50 mm |

The top-left column is x 29.6–33.4, y 29.3–30.4. It clears U7 by 0.20 mm and R32 by 0.10 mm. U3 stays 17 mm from the added stops. The bridges clear the shell solid by 0.54 mm.

## Slice

Bambu Studio 02.08.02.61. Flattened stock presets: `Bambu Lab P2S 0.4 nozzle`, `0.20mm Standard @BBL P2S`, `Generic PETG @BBL P2S`. The machine start gcode is the P2S template, unmodified. Filament PETG, nozzle 255 °C, `Textured PEI Plate` at 70 °C, infill 15%, 5 top and 3 bottom, layer 0.20 mm, supports off. The gcode has `M140 S70`, `M109 S255` and `M190 S70` from that template. No warning string in any of the three 3mf files.

| Part | Layers | Bridge blocks | Floating-shell blocks | Overhang-wall blocks |
|---|---|---|---|---|
| Shell with the bridges | 131 | 55 | 77 | 225 |
| Same shell, bridges omitted | 131 | 54 | 76 | 211 |
| Carrier, stops at 22.90 | 26 | 5 | 7 | 26 |

`case/scratch/r6_shell_fingers.3mf`, `case/scratch/r6_carrier_p2s.3mf`.

## Assembly

1. Drop the panel onto the carrier. It sits on the pads at 22.90. The fences take the long edges. The x-stops take the cable end. The tail enters J4.
2. Set the shell on the backplate and press until the four clips click. The ribs take the fences. The lands meet the border where the stack is thick, and stay clear where it is thin.

Pull the shell off the clips and lift the panel out.
