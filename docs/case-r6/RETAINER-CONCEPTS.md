# Shell hold-down

The separate plate is withdrawn. The panel drops onto the carrier and sits on a backstop. Nothing locks it. The shell clips only to the backplate, and four PETG fingers on the shell press the border land.

The load the fingers have to beat is the cable plus the shock, **0.57 N**. Twice that is **1.14 N**, and that 1.14 N is the held force after 35% creep, not the load. Margin is held force divided by 0.57 N.

## The seat, and what happened to dz

The shell today is not sitting on anything. Its snap window is 2.0 mm tall, from z 2.2 to 4.2, around the bump at 3.2. The shell can float in that window. The old 0–0.51 mm stack was that float plus a board, a carrier and a shell print error added on one side (0.16 + 0.10 + 0.10 + 0.15).

The shell gets a ledge that lands on the backplate lip. The lip top is at 5.5 mm. The clips are still deflected when the ledge is down on the lip, so they pull the shell onto the seat and it cannot sit high. The distance from that ledge to the finger is one shell print. What is left of the shell's own height is **±0.10 mm**.

Board thickness and carrier height still move the panel. They are in the corner table. They are not dz. A rigid rib still cannot cover them: 1.1 mm and 1.3 mm panels differ by 0.20 mm at the front, and the opto-to-J4 band only has about 0.18 mm of panel travel, so a rib with no give misses one thickness or blocks the shell on the other. The give is in the fingers.

## The fingers

Four fingers, two on each long edge. Each is a 2.40 mm thick beam, 2.40 mm wide, 28 mm from the clamp face to the nub centre, printed on edge. The beam is outboard of the edge, 0.40 mm clear of it, inside a recess that runs 3.0 mm outboard of the outline. A 0.60 mm bar reaches in from the shell side of the beam and stops 0.20 mm short of the glass. The last 0.40 mm, y 2.05–2.45, is the land face. Nothing else touches the panel.

The panel rests on the backstop at 22.90 mm. Shell off, it does not rise. The existing support pads move to 22.90 mm when this is built. The scratch columns are already there, tied into the carrier.

Free contact of a finger, before the panel bends it, is 22.40 mm. On a nominal 1.2 mm panel the bend is 1.70 mm. Across the 36 corners the bend is 1.35–2.05 mm, and 1.30–2.10 mm with the finger 0.05 mm thin or thick.

| | |
|---|---|
| Thin corner, held after 35% creep | 1.16 N |
| Margin, held / 0.57 N | 2.04× |
| Thick corner, spring force | 6.08 N |
| Four clips, holding force | 14.67 N |
| Clip hold left after the thick corner | 8.60 N |
| Sustained strain at the thick corner | 0.98% |

The clips take 3.67 N each to snap, 14.7 N for the four. At the thick corner the fingers add 6.1 N, so the press to seat the shell is about **21 N**. That is a firm two-handed push, no tool. Once the ledge is on the lip, 8.6 N of clip hold is still unused.

E = 1800 N/mm². Shock is 5.5 g on the 5.5 g panel, 0.270 N. The 0.30 N cable is the allowance Connie asked to keep; she has said the tail takes almost no force.

## Corners

dz is the shell print error, −0.10, −0.05, +0.05, +0.10. Carrier is −0.15, 0, +0.15. Panel is 1.1, 1.2, 1.3. That is the 36. Every one has the finger on the land. The finger ±0.05 mm is included in the thin and thick rows above.

J4, shell off, panel on the backstop. The case formula: free length 14.40, straight run 4.69, lower leg 0.50.

| Carrier | Panel back | Inserted |
|---|---|---|
| −0.15 | 22.75 | 3.98 mm |
| 0 | 22.90 | 3.74 mm |
| +0.15 | 23.05 | 3.51 mm |

All of them are over 3.15 mm. Thickness does not move the back.

The backstop at carrier −0.15 is 22.75 mm. The opto-triac top is 22.60 mm. Air is 0.15 mm. A 10 N press ends on the stop. U6 is not under the panel. The fingers and the columns miss U3, U7, R31, D7, D8, C1/C41 and the J1 tails.

The recess ceiling slices to 25.45 mm (the 0.74 mm skin becomes 0.80 mm). At the tight corner the beam top is 24.95 mm. Air is 0.50 mm. The rest of the shell is the shipped shell.

## Slice

Bambu Studio 02.08.02.61. Machine `Bambu Lab P2S 0.4 nozzle`, process `0.20mm Standard @BBL P2S`, filament `Generic PETG @BBL P2S`. Supports off, layer 0.20 mm, nozzle 255 °C.

The finger, printed on edge, is 26 layers. `warning_message` is empty. The carrier with the five backstop columns is one solid, 26 layers, `warning_message` empty.

`case/scratch/r6_finger.stl`, `r6_finger_p2s.3mf`, `r6_carrier_stops.stl`.

## Fingers

1. Drop the panel onto the carrier. It sits on the backstop. The fences take the long edges, the x-stops take the cable end, and the tail enters J4. Lift it straight back out. Nothing clicks.
2. Set the shell on the backplate and press until the four clips click and the ledge is on the lip. The fingers meet the land and hold the panel on the stop.

Pull the shell off the clips and lift the panel out.
