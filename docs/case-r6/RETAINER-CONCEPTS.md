# Shell hold-down

The display panel drops onto the carrier. The shell clips to the backplate. Four bridges printed in the shell press the border. Nothing locks the panel. One file, `case/scratch/r6_shell_hold.scad`, drives the meshes, the slice and the figures.

The panel may sit with a little play. A corner where the land is clear of the panel is acceptable. The lands have to be wide enough for the stock line width, and the shell ribs have to stay on the fences.

## The shell and the clips

The snap window runs from z 2.2 to 4.2 around the bump at 3.2. At the wall's inner face the 42° bump is 1.125 mm tall, so a shell with no bridge force can sit **±0.44 mm** in that window. The shell rim is on the wall at the window centre, so the shell does not move toward the wall past that centre.

The bridges push the shell out onto the flank. Four clips there, friction 0.3, E 1500, resist **10.1 N per millimetre** of climb. The bump apex is 0.56 mm of climb past first contact. The thickest corner climbs **0.19 mm**, so the clips stay on the flank.

A clip's sideways force at a full 0.50 mm deflection is 3.67 N. Walking the four walls over the bumps takes about 19–24 N. The bridges add about 2 N once the shell is on the flank, so the press is about **19–24 N**. Pulling the shell off is about **17–20 N**.

## The bridges

Each bridge is a fixed-fixed beam, **0.80 mm** thick (four layers) and **1.20 mm** wide, printed in the shell. The pillars enter the recess skin by 0.10 mm. The shell prints face down. The span's first layer is a bridge along the edge. The land is a nub **0.50 mm** wide, **4.0 mm** long and **0.45 mm** tall, on the beam, so the stock line width of 0.42 mm can extrude it. The load is close enough to the beam centreline that the torsion factor is 1.00.

The free land, shell centred in the window, is at **z 23.72**. The beam bottom is 0.45 mm above that. At every stack corner the pillars stay at least **0.13 mm** off the panel, and the beam stays **0.16 mm** under the recess ceiling. The right-hand root reaches **1.4%** strain at E 1500, about 21 MPa.

| | Left bridges | Right bridges |
|---|---|---|
| Span between pillars | 33 mm | 30 mm |
| Land centre, board x | 48.6 | 98.0 |
| Land to the nearer pillar | 16.5 mm | 6.0 mm |

The right-hand land is 6 mm from the inner face of its pillar. A land 8 mm from that face lets the thick corner put a pillar on the panel, and the 10 N corner press then reaches U7.

## Where the panel sits

The carrier pads and the added columns top out at **z 22.90**, which is 0.15 mm under `Z_panel_b`. The columns are solid from the PCB face. A tie at 1.20 mm joins the top-left column to the spine. The long-edge fences stand **0.20 mm** off the outline.

A nominal panel has **±0.20 mm** of room between those fences. Ribs on the shell stand 0.10 mm off the outer face of each fence. The rib bottoms are at `Z_in − 1.85`, so the rib and the fence overlap by at least **0.60 mm** at every stack corner. The nearest carrier clearance beside a rib is 0.10 mm.

The land runs from 2.47 to 2.97 mm off the edge. With a short panel on the bottom fence, the 0.10 mm rib gap, and 0.05 mm of print error on the land and the rib, the contact is:

| Edge | Distance from the panel edge |
|---|---|
| Top | 1.92 to 3.12 |
| Bottom | 2.32 to 3.32 |

That strip stays between the seal line at 1.92 mm and the window at 3.32 mm. The active area starts 3.82 mm from the edge. The bridge's inboard face, with the panel against the fence, is 3.55 mm from that edge.

## Capture at the 243 corners

Board ±0.16, carrier ±0.15, panel 1.1 / 1.2 / 1.3, shell print ±0.10, bridge ±0.05. E 1500.

| | |
|---|---|
| Corners | 243 |
| Land touching | 89 |
| Gap, land clear of the panel | 154 |
| Pillar air, minimum | 0.13 mm |
| Ceiling air, minimum | 0.16 mm |
| Climb, maximum | 0.19 mm |
| Rib and fence overlap, minimum | 0.60 mm |
| Corners past the bump apex | 0 |

A gap at a thin corner is acceptable. The panel is still between the stops and the bridges.

## J4

Free length 14.40 mm, straight run 4.69 mm, lower leg 0.50 mm. With the panel on the stop the tail depth is the stop height.

| Carrier | Panel back | Inserted |
|---|---|---|
| −0.15 | 22.75 | 3.98 mm |
| 0 | 22.90 | 3.74 mm |
| +0.15 | 23.05 | 3.51 mm |

The target is 3.15 mm. Those three backs are the seated depths. They are not the depth at every corner once the cable pulls.

With the shell fitted, a thin corner has a gap under the land. The 0.30 N cable lifts the cable end about the left land until the right land takes the gap. As printed, insertion runs from **2.66 to 3.98 mm**, and **39 of 243** corners are under 3.15 mm. After the bridges relax to 0.35 of their stiffness, insertion runs from **2.40 to 3.95 mm**, and **87 of 243** corners are under 3.15 mm. A 5 g knock on top of that cable brings the minimum to 2.05 mm. A miss of 3.15 mm does not reject the design.

The cable end can lift out of J4 from day one, at a thin corner, and again after the bridges relax. Take the shell off, press the tail back in, and close the slider.

## Clearance

Short carrier, panel back at 22.75. A 10 N press at the panel corner, the panel rigid, the preload gone, the loose-corner gap 0.62 mm and the bridge stiffness kept at 0.35:

| Part | Rest gap | Tip | Minimum |
|---|---|---|---|
| U7 | 0.150 | 0.106 | 0.044 mm |
| U8 | 0.150 | 0.028 | 0.122 mm |
| D4 | 1.65 | 0.07 | 1.58 mm |
| C1/C41 | 1.65 | — | 1.65 mm |
| J1/C43 tails | 2.15 | — | 2.15 mm |
| D7 | 2.80 | — | 2.80 mm |
| D8 | 2.80 | — | 2.80 mm |
| R31 | 3.60 | — | 3.60 mm |
| R32 | 3.50 | — | 3.50 mm |

The top-left column is x 29.6–33.4, y 29.3–30.4. It clears U7 by 0.20 mm and R32 by 0.10 mm. Without that column the same press puts the panel on U7. U3 stays 17 mm from the added stops. The bridges clear the shell solid by 0.54 mm.

## Slice

Bambu Studio 02.08.02.61. Flattened stock presets: `Bambu Lab P2S 0.4 nozzle`, `0.20mm Standard @BBL P2S`, `Generic PETG @BBL P2S`. The machine start gcode is the P2S template, unmodified. Filament PETG, nozzle 255 °C, `Textured PEI Plate` at 70 °C, infill 15%, 5 top and 3 bottom, layer 0.20 mm, classic walls, line width 0.42 mm, `detect_thin_wall` off, supports off. The gcode has `M140 S70`, `M109 S255` and `M190 S70` from that template. No warning string in any of the three 3mf files. The stock-shell log also prints:

```
ZFiller: encounter idx from clip: 25
ZFiller: encounter idx from clip: 14
ZFiller: encounter idx from clip: 14
```

The four lands extrude on the layers at print z 2.0, 2.2, 2.4 and 2.6.

| Part | Layers | Bridge blocks | Floating-shell blocks | Overhang-wall blocks |
|---|---|---|---|---|
| Shell with the bridges | 131 | 55 | 77 | 231 |
| Same shell, bridges omitted | 131 | 58 | 79 | 211 |
| Carrier, stops at 22.90 | 26 | 5 | 7 | 26 |

`case/scratch/r6_shell_bridge.3mf`, `case/scratch/r6_shell_stock.3mf`, `case/scratch/r6_carrier_p2s.3mf`.

## Assembly

1. Drop the panel onto the carrier. It sits on the pads at 22.90. The fences take the long edges. The x-stops take the cable end. The tail enters J4.
2. Set the shell on the backplate and press until the four clips click. The ribs take the fences. The lands meet the border where the stack is thick, and stay clear where it is thin.

Pull the shell off the clips and lift the panel out. If the cable end has left J4, press the tail back in and close the slider before the shell goes on.
