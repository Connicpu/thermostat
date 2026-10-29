# Shell hold-down

The separate plate, clip, and detent are withdrawn. The panel drops onto the carrier. The shell clips only to the backplate, and its ribs are what press the panel. Nothing locks the panel in.

Scratch, not the shipped case:

- `case/scratch/r6_shell_hold.scad` includes `case/thermostat_case.scad`
- `docs/case-r6/shell-hold-section.png` — shell off, and shell on
- `docs/case-r6/shell-hold-plan.png`
- `docs/case-r6/shell_hold_check.py` — the gate numbers below

## What does not close

Two gates fight each other, and a third one does not fit in the shell.

The panel can sit only in a narrow band. Shell off, the free nub has to stay at or below 23.28 mm or the tail in J4 drops under 3.15 mm at a tall carrier and a thick leaf. The backstop has to stay at or above 22.90 mm or a short carrier leaves less than 0.15 mm over the opto-triacs (top 22.60). That is 0.15 mm of useful spring travel, and a leaf printed 0.05 mm thin or thick uses most of it.

Sized so the thin leaf still holds 2× after 35% creep, the four leaves push **17.8 N** at the thick leaf. The four backplate clips hold **14.7 N** (3.67 N each, the existing clip formula). The thick corner can push the shell back off the clips. Scaling the leaves down until the thick corner stays under the clips drops the thin corner to about 1.6×. **2× at the thin leaf and a shell that stays clipped at the thick leaf are not both available inside the J4 and opto limits.**

The shell rib has a second, separate problem. Its tip is 24.10 mm at dz = 0, which is the front of a 1.2 mm panel sitting on the backstop. Across dz 0–0.51, carrier ±0.15, and panel 1.1–1.3, that rib **misses the panel on 22 of 36 corners**, including every corner at dz = 0.51. Dropping the rib far enough to touch the worst loose corner (front 24.00 at dz 0.51) puts the tip near 23.49 mm. At the tight corner the panel front on the backstop is 24.35 mm, so the rib stands 0.86 mm proud of a seated shell. The shell cannot finish clipping unless that rib yields 0.86 mm.

A leaf thick enough to yield that far and still make the force does not fit. Above the panel there is about 1.5 mm between a high panel front and a 0.40 mm shell skin. A 1.4 mm rib in that gap, 2.4 mm wide and 34 mm long, is about a tenth of the required retained force. The opto clearance, the J4 rise limit, and a 0.51 mm shell stack cannot be met together by a PETG rib in the space the shell has.

## What the geometry does

The panel drops in from the front. The existing fences locate the long edges (0.20 mm air). The x-stops locate the cable end. The tail goes into J4. No clip, no screw, no detent.

Four PETG leaves sit under the border. Each is 10 mm from the clamp face to the nub centre, 2.60 mm thick (13 layers), nub 0.80 mm, and the only contact is the nub at y 2.05–2.45. Top leaves are 2.80 mm wide. Bottom leaves are 2.50 mm wide so they stay above the J1 tails (y ≤ 59.5) and clear of D7/D8. Free nub top is 23.05 mm. Beam bottom is 19.65 mm, 1.05 mm over the PCB.

Five backstop columns top out at 22.90 mm. They are tied into the carrier. A 10 N press ends on them. At carrier −0.15 the stop is 22.75 mm, 0.15 mm over the opto top. Travel from a seated panel is zero, because the panel is already on the stop once the shell has pushed it there. U6 is not under the panel.

The shell ribs are bosses on the recess ceiling, 2.80 mm along the edge and 0.40 mm across the land (the land is only 0.40 mm wide). Shoulders are not modeled; the contact is the land itself. Design tip is 24.10 mm.

Nub centres, board x: 62 and 100 on the top edge, 72 and 104 on the bottom. `shell_hold_check.py` reports no clash with U3, U6, U7, U8, R31, D7, D8, C1/C41, or the J1/C43 tails.

## Force, when the shell has seated the panel

E = 1800 N/mm². Four leaves. 35% retained. Shock 0.270 N. Cable allowance 0.30 N. The load to beat is 1.139 N.

Compression is free nub minus backstop, plus the leaf's own ±0.05 mm. Carrier height moves the nub and the stop together, so it drops out.

| Leaf | Compression | Held after creep | Margin | Strain | Spring force |
|---|---|---|---|---|---|
| thin, −0.05 | 0.10 mm | 2.77 N | 2.43× | 0.38% | 7.91 N |
| thick, +0.05 | 0.20 mm | 6.21 N | 5.45× | 0.80% | 17.75 N |

Install strain is the same order. It is under 2.4%. The 17.75 N thick-leaf force is the number that exceeds the 14.67 N clip hold.

Clip snap is 3.67 N per clip, 14.7 N for the four. Seating the shell means that snap plus the spring force, because the leaves compress as the clips crest. At the thin leaf that is about 23 N. At the thick leaf it is about 33 N, and once seated the springs can outweigh the clips.

## J4, shell off

The panel sits on the free nub. Insertion uses the case formula (free length 14.40, straight 4.69, lower leg 0.50).

| Carrier | Leaf | Panel back | Inserted |
|---|---|---|---|
| −0.15 | −0.05 | 22.85 | 3.82 |
| −0.15 | +0.05 | 22.95 | 3.66 |
| 0 | −0.05 | 23.00 | 3.58 |
| 0 | +0.05 | 23.10 | 3.43 |
| +0.15 | −0.05 | 23.15 | 3.35 |
| +0.15 | +0.05 | 23.25 | 3.19 |

The low corner is 3.19 mm. Panel thickness does not move the back when the panel is sitting on the nub.

## Rib reach

Tip = 24.10 + dz. The panel front, still on the nub because the rib has not pushed it down, is 23.05 + carrier + thickness.

| dz | Carrier | Thickness | Front | Rib | |
|---|---|---|---|---|---|
| 0 | −0.15 | 1.1 | 24.00 | 24.10 | misses |
| 0 | −0.15 | 1.3 | 24.20 | 24.10 | touches |
| 0 | +0.15 | 1.1 | 24.30 | 24.10 | touches |
| 0 | +0.15 | 1.3 | 24.50 | 24.10 | touches |
| 0.51 | any of these | 1.1 or 1.3 | 24.00–24.50 | 24.61 | misses |

22 of the 36 corners miss. Every dz = 0.51 corner misses.

## Air and the slice

The recess skin is 0.74 mm and slices to 0.80 mm, so the ceiling is 25.45 mm. The highest shell-off front is 24.50 mm. Air is 0.95 mm. The ribs are the only new shell contact. Everywhere else the shell is the shipped one.

Bambu Studio 02.08.02.61, machine `Bambu Lab P2S 0.4 nozzle`, process `0.20mm Standard @BBL P2S`, filament `Generic PETG @BBL P2S`. Supports off, layer 0.20 mm, nozzle 255 °C. The gcode header reports sparse infill 20% and 2 walls. `warning_message` in each `result.json` is empty.

The log also printed, on each run, before the slice finished:

```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
error_code 65544, description: Wayland: Failed to connect to display
glfwInit return error, code 65544
```

No floating-region or cantilever warning was printed. The leaf is 17 layers (3.40 mm, the 2.60 mm beam plus the 0.80 mm nub). The rib coupon is 10 layers (0.80 mm skin plus a 1.20 mm rib). The carrier with the backstops is 26 layers and one solid.

Files: `case/scratch/r6_leaf.stl`, `r6_rib.stl`, `r6_carrier_stops.stl`, and the matching `r6_leaf_p2s.3mf` and `r6_rib_p2s.3mf`.

## Fingers

1. Drop the panel onto the carrier. The fences take the long edges. The cable end meets the x-stops and the tail enters J4. Lift it back out the same way. Nothing clicks.
2. Set the shell on the backplate and press until the four clips click. Where a rib reaches the border, it presses the panel onto the backstop and compresses the leaf under that nub.

To open, pull the shell off the clips and lift the panel.
