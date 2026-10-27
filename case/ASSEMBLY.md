# Assembly

The case is a 147 × 71 × 26.4 mm PETG shell, backplate, and panel carrier. The board, the carrier, and the panel are one subassembly. The shell is a cover: it clamps the board and snaps onto the backplate, and it does not hold the panel.

Coordinates below are board coordinates unless noted: origin at the board's top-left, x to the right, y down, z up from the PCB top. Case z is out from the wall, with the wall at z = 0. The outer depth, 26.4 mm, is the shell front (`Z_out`).

## What has to fit

The panel is a Good Display GDEY029T94. The datasheet table gives the outline as 36.7 (H) × 79.0 (V) × 1.2 (D) mm and the active area as 29.056 × 66.896 mm. On the board that outline is x 29.46–108.46, y 26.24–62.94. The active area sits inside it with a 3.0 mm border on the left, 3.82 mm on the top and bottom, and 9.1 mm on the right, which is the end the cable leaves.

The mechanical drawing (datasheet page 7) dimensions the tail:

- free length past the outline, 14.40 mm
- width 12.50 ± 0.1 mm
- thickness 0.30 ± 0.03 mm
- 24 contacts at 0.5 mm pitch, span P0.5 × 23 = 11.50 mm
- contact / stiffener length 3.66 mm
- the drawing marks the contact side

Pin 1 is 23.5 mm from the tail edge that is nearest pin 24, and that edge is 11.50 mm from the outline. That lands pad 1 at board y = 49.74, which is where the footprint puts it. The contacts are on the display-face side of the tail. Folded 180° under the panel, they face the board.

J4 is a Jushuo AFC07-S24FCA-00 (LCSC C262567). It is a right-angle, 2.0 mm high, bottom-contact FPC connector with a horizontal slide lock, the "drawer" style. The drawing title says "0.5Pitch H=2.0 FPC ZIF R/A Type3 Lower SMT CONN". The mechanism view is a slider, not a flip latch: the slider-open dimension is 0.85 mm, sticking out on the cable side of the housing. DIM_B, the body depth, is 3.90 mm. For 24 contacts the drawing's four length columns are 11.50, 12.60, 16.50, and 17.60 mm. The side view also shows 2.70, 2.20, 0.50 ± 0.05, 0.30 ± 0.03, and R0.20. R0.20 is a housing fillet. The footprint's silk front face is at board x = 103.77, and the mouth faces +x, so the tail enters from the right. The recommended FPC thickness on the drawing is 0.30 ± 0.03 mm, which matches the panel tail. The same drawing recommends a contact length of 3.0 mm minimum.

The side view does not label a single "FPC centerline" height. The fold below uses 0.50 mm for the lower leg, because 0.50 ± 0.05 is the small vertical callout next to the 0.30 mm FPC thickness. That callout is not labeled as the centerline. A later section shows how far the insertion moves if the leg is 0.30 mm or 0.70 mm instead.

The optos under the left of the panel (U6, U7, U8, MOC3063S) are modeled at 4.0 mm tall, so the glass has to clear them. The carrier holds the panel back 4.6 mm above the PCB top.

## The fold

The free tail starts at the outline, in the plane of the glass back, and enters the bend there. There is no straight run on the glass. The lower leg runs left from the bend, level at 0.50 mm, into the mouth.

At a 4.6 mm gap and a 0.50 mm lower leg:

- bend diameter 4.10 mm, centerline radius 2.05 mm
- arc length π × 2.05 = 6.44 mm
- straight run from the panel edge (x = 108.46) to the mouth (x = 103.77) = 4.69 mm
- inserted length 14.40 − 6.44 − 4.69 = 3.27 mm

The outer fiber of the bend bulges about R + 0.15 = 2.20 mm past the panel edge, to board x ≈ 110.66. The shell pocket on that side is 4 mm deep, out to x = 112.46.

Raising the panel lengthens only the semicircle. The pull-out is π/2 ≈ 1.57 mm of tail per millimetre of extra gap, and it does not depend on the lower-leg height. `scripts/case_fit.py` prints the rows below and fails unless the 0.50 mm leg stays at or above 3.0 mm both at 4.6 mm and at a carrier printed 0.10 mm tall (gap 4.70 mm, inserted 3.11 mm). The other legs are printed and are not all gated.

| Lower leg | Gap 4.50 mm | Gap 4.60 mm | Gap 4.70 mm |
|---|---|---|---|
| 0.30 mm | 3.11 mm | 2.96 mm | 2.80 mm |
| 0.50 mm | 3.43 mm | 3.27 mm | 3.11 mm |
| 0.70 mm | 3.74 mm | 3.58 mm | 3.43 mm |

4.6 mm is the design gap, not 4.7 mm. At 4.7 mm the nominal 0.50 mm leg inserts 3.11 mm, and a carrier that prints 0.10 mm tall drops that to 2.96 mm, under the drawing's 3.0 mm. At 4.6 mm the same tall print inserts 3.11 mm. A thicker board does not change the gap: it lifts the board, the carrier, and the panel together. Carrier print height does.

A 0.30 mm leg at the design gap inserts 2.96 mm, 0.04 mm under the drawing. That is the tail lying on the soldermask. The 0.50 mm figure is an unlabeled side-view number, so this row is the open question on the fold.

Neither the GDEY drawing in the part notes nor the Jushuo drawing states which face the tail leaves, and neither states a minimum bend radius. This model starts the free length at the outline in the plane of the glass back. If the tail instead left flush with the glass front, the bend would also climb the 1.2 mm glass and the inserted length at this gap would be 1.38 mm, which is under 3.0 mm. A centerline 0.15 mm off the glass back (half the 0.30 mm tail) is inside the 0.50 mm leg at the nominal gap and fails the tall-print row. The Jushuo R0.20 is the housing fillet, not a tail radius.

The slider has to be shut before the glass comes down. The body is 2.0 mm high and the glass back is at 4.6 mm, so 2.6 mm remains above the housing once the panel is in place. The mouth is at x = 103.77 and the glass edge is at x = 108.46, so the 0.85 mm tab sits about 3.8 mm inboard of the glass, in a tunnel 4.69 mm deep. A finger or a tool does not reach it there. On the bench the panel is held up, the tail is slid in from the right, and the slider is pushed shut toward the mouth. Open it by moving the slider away from the mouth, toward the cable. The drawing's open pose is that 0.85 mm tab on the cable side. The tab is small; a small flat tool is the reliable way to move it. The EasyEDA model on the footprint is the same part at the same silk line, and it is not a measured copy of the drawing.

## Why the panel is on a carrier

The mouth is under the panel. With the glass seated, the shell wall and the glass both cover the slider, so the tail cannot be plugged and the slider cannot be moved after the shell is on. The window is smaller than the panel, so the glass cannot drop in through the front either. The carrier is the piece that makes a step where the tail is in sight: mate J4 with the panel in the hand, then press the glass onto the hooks.

The shell never touches the glass. Four hooks on the carrier hold it.

## The carrier

The carrier is one PETG piece. It sits on the PCB, holds the panel back at 4.6 mm, and locates on the board edges rather than on the standoff pins. The pins are Ø3.9 and stick through the board, and the shell's clamp posts land on the same holes, so the carrier stays clear of them.

Four hooks, two on each long edge, and none on the cable end:

- top edge, x 47.0–53.0 and x 84.6–90.6
- bottom edge, x 49.6–55.6 and x 76.0–82.0

Each hook is a 0.80 mm beam (two perimeters of a 0.4 mm nozzle), 6 mm wide, standing 0.25 mm off the glass. Only the bottom 0.40 mm of the beam, the foot, bridges to the pad. The pad is inboard of the glass edge, so it does not fill the beam. The free length is the glass front minus that foot:

L = 24.4 − (18.6 + 0.40) = 5.40 mm

From the solder mask (z = 18.6) to the glass front (z = 24.4) is 5.8 mm. A foot has to sit in that span, so a 5.9 mm free beam does not fit above the board. The ledge crosses the glass by 0.32 mm, which is the deflection, and the shelf on the glass face is 0.25 mm tall. Above the shelf a lead-in rises 0.32 mm while it steps back 0.32 mm, so the lead-in is 45°. With E = 1.8 GPa,

- strain ε = 1.5 × 0.80 × 0.32 / 5.40² = 1.32%
- force F = 3 E I δ / L³ = 2.81 N per hook, I = 6 × 0.80³ / 12

`scripts/case_fit.py` recomputes both and fails if the strain reaches 2% or the force reaches 8 N. It also probes the mesh: the beam is solid, the 0.25 mm gap beside it is air, and the foot is solid. 2.8 N is a firm finger on the lead-in. Taking the panel off is the same load, with a small flat tool under the lead-in of one edge. The shelf is 0.25 mm tall, so the tool is what finds it.

The ledges stand proud of the glass, so the shell has a pocket over each hook, 1.10 mm deep. The rest of the inner face is recessed 0.36 mm over the panel, which keeps a thick board (the +0.16 mm end of the JLCPCB tolerance) from pressing the glass into the shell. The recess stops at board x = 107.55. The button tubes are Ø8.6 at x = 112, so they occupy the last of the outline, and cutting the recess through them would leave the tube walls loose. The window chamfer starts 0.40 mm behind the inner face, so the 0.36 mm recess stays in front of it.

Under each hook a pad rises from the PCB to the panel back. The right-hand top pad also bridges across to the column at x 76–82, under the glass edge and past the ESP32 module (the module ends at y = 25.49, the pad starts at y = 25.99). Nothing in the carrier crosses the optos, L1, or the FPC bulge. The path between the top hooks and the bottom rail jogs east around C42, narrows to 0.85 mm to pass between the two C43 leads (x = 51.50 and x = 55.00), and narrows again to pass to the right of J1 pin 5 (x = 77.00).

The piece locates on two edges. A fence on the left, 0.25 mm off the board edge, and a fence on the bottom edge do the same job. Short ties join each fence to the rail on the board so a 0.4 mm nozzle does not weld the fence to the rail and eat the clearance. The bottom rail is the spine that makes the hooks, the fences, and the columns one solid. At the two bottom hooks the rail is only foot-height (0.40 mm) where it meets the beam, and the tall rail stops inboard of the glass edge, so the beam stays free.

That rail crosses the front curtain, which is the wall between the hot zone and the panel at board x 27.7–29.0. The curtain floor is 1.1 mm above the PCB. For 1 mm of y at the board's bottom edge the curtain is cut up to 1.55 mm, and the rail through that slot is 1.00 mm tall. The remaining crack is about 0.55 mm high (about 0.39 mm if the board is thick) and well under 1 mm², smaller than the ~1.3 mm² leak where the rear rib and the front partition do not line up. The rail plugs the rest of the notch, so the curtain separates the zones.

In the print the hook shelf is a horizontal lip 0.57 mm deep (the 0.25 mm air gap plus the 0.32 mm ledge). The lead-in above it is 45°. The lip is short. A profile that supports every overhang supports this lip too.

## Buttons

UP, DOWN, and MODE are at board (112, 30), (112, 40), and (112, 50). The pitch is 10 mm and the guide tubes are Ø8.6, so the front partition overlaps the tube wall and the recess in the face stops at x = 107.55.

The switch is a TS-1187A-B-A-B. Height code A is 1.5 ± 0.05 mm, which is how tall the actuator stands, not how far it moves. The drawing calls the travel 0.2 ± 0.1 mm and the spec table says 0.25 mm. Force code B is 160 gf. The actuator is Ø2.0 and the body is 5.1 mm square, with the cover at 1.2 mm. The stem is Ø1.6, so it meets the actuator and not the cover.

Each cap is captive in the face. A Ø4.70 head passes through the Ø5.0 hole with 0.15 mm to spare. Behind the hole a cone opens to Ø6.0 at 65° from horizontal and meets the inner corner of the hole, so the cap cannot leave through the front. One leaf on the partition side of the stem carries the catch. The catch sits behind a lip in the tube (the lip's hole is Ø6.3, the bore around it is Ø7.7). The FPC pocket removes the panel side of each tube, which is why the leaf faces the partition, where the lip is present. Two leaves of this length do not fit in that bore on the partition side.

The leaf is a straight vertical wall, 0.40 mm thick and 4.50 mm long. A rib clamps the first 0.15 mm, so the free length is 4.35 mm. The barb at the free end stands 0.30 mm past the lip and has to bend that far in x to pass. With the rectangular-beam formula ε = 1.5 t δ / L²,

ε = 1.5 × 0.40 × 0.30 / 4.35² = 0.95%

`scripts/case_fit.py` recomputes that and fails if the free length is under 4 mm or the strain reaches 2%. The pressed-catch check is the barb face against the lip, and that face kisses the lip (clearance 0). The seated cone kisses the hole corner.

There is no second spring. A leaf that stayed on the lip through the whole 1.20 mm of travel would be several times this strain. The cap can move the full 1.20 mm between the cone and the lip. The switch's own 160 gf is what pushes it back out, and only after the stem has crossed the gap. Off the board, the cap rattles. It cannot fall out of the front, and it cannot fall out of the back.

The shell is fixed to the backplate. The clamp posts are cylinders that start at z = Z_pcb_t + 0.1, 0.1 mm of air above a nominal board, so they do not make the shell follow the board. Board thickness stays in the stack. The snap windows run z 2.2–4.2 and the bumps are about 2.3–4.1, so a shell that is not seated in the middle of the window has about ±0.1 mm of extra play. That play is not in the numbers below.

Nominal gap (seated stem to the actuator) is 0.48 mm. Nominal travel (cone on the hole, to the catch on the lip) is 1.20 mm. Nominal plastic stroke is 0.72 mm. The switch itself only travels about 0.25 mm, and its cover is the stop a finger feels. The lip is further along, so the pressed mesh is past the cover: mashing past the click loads the cover until the lip. The fit script allows the stem to pass the actuator in that pose.

The tolerance check is a uniform Monte Carlo, N = 200000, seed 1, four terms: printer z ±0.10 mm on the gap, an independent ±0.10 mm on the travel (the cap and the shell are separate prints), standoff ±0.15 mm, and board ±0.16 mm. At these nominals it reports P(gap ≤ 0) = 0%, P(stroke < 0.20 mm) = 0%, worst-case gap 0.07 mm, worst-case stroke 0.21 mm. The script fails if either probability is over 1% / 2% or the worst stroke is under 0.20 mm. A normal model that drops the board term and treats the remaining ± as 1σ is printed beside it and is not the gate. The review's 6.6% and 29.7% do not fall out of these ± values as either uniforms or matching 1σ normals, so the gate is this four-term uniform stack.

Print the caps head-down, face on the bed. The cone, the barb underside, and the root rib are 65° from horizontal. PrusaSlicer measures its overhang threshold from the horizontal and skips faces steeper than the threshold, so those three are inside a 60° threshold. The free span of the leaf is not. It is a horizontal bridge about 4.1 mm long and 0.40 mm wide (1.64 mm²), one extrusion. That face is 0°, so every threshold from 30° to 60° supports it unless the slicer treats it as a bridge and "Don't support bridges" is on. PrusaSlicer was not re-run on this shape. The 3MF leaves supports off, which is the right setting only if the printer bridges that one wall.

## Build order

1. Screw the backplate to the wall. Four drywall-anchor screws, pan heads in the recesses at the four anchor points. The board is not on the plate yet, so the heads are reachable. The lip, the snap bumps, the ribs, and the standoffs face out of the wall.

2. Pull the cable through the centred 25 × 20 mm hole. Leave enough slack to mate the plug in the hand.

3. On the bench, board component-side up, panel held up off the carrier. Fold the tail 180° under the panel, contacts down, and slide it into J4 from the right until it stops. Push the slider shut toward the mouth with a small flat tool. The 0.85 mm tab is the part that moves. The glass is not down yet, which is the only pose in which the tab is reachable.

4. Set the carrier on the board. The left fence and the bottom fence drop onto the board edges; the pads sit on the soldermask. Press the panel down onto the four pads until the ledges click. Each hook is about 2.8 N. The panel back is then 4.6 mm above the PCB. This is the subassembly: board, carrier, panel, tail.

5. Mate the 15EDGK plug onto J1 while the board is in the hand. The plug faces the wire hole.

6. Set the subassembly onto the four locating pins. Feed the wire loop back through the hole as the board goes down. The pins stop the board in x and y. The plug and C43 are in the space the standoffs leave above the plate.

7. Snap the three button caps into the shell from the inside, before the shell goes near the board. Lay the shell face-down, push each head through its hole, and press until the cone seats and the barb clicks behind the lip. The barb bends 0.30 mm to pass. The caps stay in the shell from here on. They can rattle between the cone and the lip. Lower the shell over the stack. It slides onto the backplate lip. The clamp posts land around the four mounting holes and press the board onto the standoffs. The stems come down onto the three actuators with the 0.48 mm gap. Four bumps on the lip click into the snap windows.

The light pipe, the RESET pinhole, the window, and the vents are in the shell. The three airflow zones are the hot zone on the left (triacs and the opto drive), the quiet middle over the wire hole (fill the wall opening with putty), and the sensor duct on the right. The baffle around the SHT40 island is open to the right wall. The edge vents are about 28% open area, on all four edges, with the triac heat on the far side of the curtain from the sensor.

## Taking it apart

Pry the shell at the snap windows. A small flat tool in one window on the bottom edge, then the other bottom window, then the top edge, flexes the 2.4 mm wall out over the 0.9 mm bump. The bumps are 45° on both sides, so the wall can climb off them. Lift the shell straight off. The button caps stay in it. To take one out, press the single barb toward the stem and push the cap out the back of the hole.

Lift the subassembly off the pins. Unplug the HVAC plug in the hand.

To release the panel, get a small flat tool under the lead-in of the two hooks on one long edge and tilt the glass up. That is about 2.8 N per hook, and it is the pose that uncovers the slider. Move the slider away from the mouth and pull the tail straight out.

## Where J4 sits

J4 is the footprint in `kicad/`: silk mouth at board x = 103.77, 4.69 mm inboard of the glass edge, slider under the panel. The case mates the tail with the panel held up because that is where the connector is. A mouth to the right of x = 108.46 would put the slider in the open, and it would also change the straight run and the inserted length. This case does not move the connector.
