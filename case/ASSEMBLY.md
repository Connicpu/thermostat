# Assembly

The case is a 147 × 71 × 26.8 mm PETG shell and backplate. The board, the panel carrier, and the panel are one subassembly. The shell is a cover: it clamps the board and snaps onto the backplate, and it does not hold the panel.

Coordinates below are board coordinates unless noted: origin at the board's top-left, x to the right, y down, z up from the PCB top. Case z is out from the wall, with the wall at z = 0.

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

J4 is a Jushuo AFC07-S24FCA-00 (LCSC C262567). The drawing title is "0.5Pitch H=2.0 FPC ZIF R/A Type3 Lower SMT CONN": right-angle, 2.0 mm high, bottom contact, flip latch. The latch is a separate black part and opens upward, away from the PCB. DIM_B, the body depth, is 3.90 mm. For 24 contacts the drawing's four length columns are 11.50, 12.60, 16.50, and 17.60 mm. The side view also shows 2.70, 2.20, 0.50 ± 0.05, 0.30 ± 0.03, and R0.20. The footprint's silk front face is at board x = 103.77, and the mouth faces +x, so the tail enters from the right. The recommended FPC thickness on the drawing is 0.30 ± 0.03 mm, which matches the panel tail.

The side view does not label a single "FPC centerline" height. The fold below uses 0.50 mm for the lower leg, because 0.50 ± 0.05 is the small vertical callout next to the 0.30 mm FPC thickness. A later section shows how far the insertion moves if that callout is the wrong one.

The optos under the left of the panel (U6, U7, U8, MOC3063S) are about 4 mm tall, so the glass has to clear them. The carrier holds the panel back 5.0 mm above the PCB top. The allowed band is 4.6–5.2 mm.

## The fold

The free tail starts at the outline and enters the bend there. There is no straight run on the glass. The lower leg runs left from the bend, level at 0.50 mm, into the mouth.

At a 5.0 mm gap and a 0.50 mm lower leg:

- bend diameter 4.50 mm, centerline radius 2.25 mm
- arc length π × 2.25 = 7.07 mm
- straight run from the panel edge (x = 108.46) to the mouth (x = 103.77) = 4.69 mm
- inserted length 14.40 − 7.07 − 4.69 = 2.64 mm

The outer fiber of the bend bulges about 2.4 mm past the panel edge, to board x ≈ 110.9. The shell pocket on that side is 4 mm deep, out to x = 112.46, and it is the same pocket that already clears the button tubes.

Raising the panel lengthens only the semicircle. The pull-out is π/2 ≈ 1.57 mm of tail per millimetre of extra gap, and it does not depend on the lower-leg height. Across the allowed band, with the lower leg at 0.50 mm:

| Panel gap | Radius | Inserted |
|---|---|---|
| 4.6 mm | 2.05 mm | 3.27 mm |
| 5.0 mm | 2.25 mm | 2.64 mm |
| 5.2 mm | 2.35 mm | 2.33 mm |

The contact zone is 3.66 mm long, so 2.3 mm of insertion does not bury the stiffener to its end. The terminals sit back from the mouth, and 2.3 mm still reaches them, but 5.2 mm is the tight end of the band. The carrier is built at 5.0 mm and does not stack up on top of the board tolerance: a thicker board lifts the board, the carrier, and the panel together.

If the 0.50 mm callout is not the slot, the insertion at a 5.0 mm gap is:

| Lower leg | Inserted at 5.0 mm | Inserted at 5.2 mm |
|---|---|---|
| 0.30 mm (tail lying on the soldermask) | 2.33 mm | 2.01 mm |
| 0.50 mm | 2.64 mm | 2.33 mm |
| 0.70 mm (the radius that matches the older 2.15 mm note) | 2.96 mm | 2.64 mm |

`scripts/case_fit.py` prints these and fails if any of them drops to 2.0 mm or below.

The latch has to be closed before the glass comes down. The body is 2.0 mm high and the glass back is at 5.0 mm, so about 3 mm remains above the housing once the panel is in place. That is not room to flip the latch with the panel already seated. On the bench the latch is open while the tail goes in, and it is closed before the panel is pressed onto the carrier.

## Why the panel cannot live in the shell

A shell that holds the panel, and a backplate that holds the board, leaves no step where the tail can be inserted and the latch closed.

The mouth is under the panel: the silk face is at x = 103.77 and the glass edge is at x = 108.46. Once the glass is in the shell, the shell wall and the glass both cover the latch. The tail cannot be plugged in, and the latch cannot be closed, after the shell is on.

The other order fails too. With the tail already plugged, the glass is 5 mm above the board and tied to it. The shell would have to come down over that stack. The window is smaller than the panel, so the glass cannot drop in through the front. The old shell clips were 1.2 mm thick and about 2 mm tall, with a 0.7 mm lip, two on each long edge. A beam that short and that thick does not spread under a thumb; a rough cantilever puts each clip near 350 N, on the order of 35 kg. They were meant to let the glass snap in from the open back of the shell, not to pass over a panel that is already on the board. Lowering the shell onto a plugged panel either breaks the clips or pulls the tail.

## Alternatives

Two other ways to get the tail in were set aside.

A hinged carrier, folded like a book onto the board after the tail is plugged, needs the hinge within a few millimetres of J4, because the free tail is only 14.40 mm. That hinge would sit in the button column and against the sensor duct. A PETG living hinge also cracks after a few folds, and this case has to come apart for service.

A shell that opens from the side cannot slide. The backplate ribs, the mated 15EDGK plug, and the four locating pins all stand up in the rear cavity, and a side opening would cut the hot-zone curtain or the sensor partition. Those walls are the airflow design: hot zone on the left, quiet middle zone over the wire hole, sensor duct on the right.

The carrier avoids both. The tail is mated on the bench, in the open, and the shell never touches the glass.

## The carrier

The carrier is one PETG piece. It sits on the PCB, holds the panel back at 5.0 mm, and locates on the board edges rather than on the standoff pins. The pins are Ø3.9 and stick through the board, and the shell's clamp posts land on the same holes, so the carrier stays clear of them.

Four hooks, two on each long edge, and none on the cable end:

- top edge, x 47.0–53.0 and x 84.6–90.6
- bottom edge, x 49.6–55.6 and x 76.0–82.0

Each hook is a 0.8 mm beam (two perimeters of a 0.4 mm nozzle), 0.25 mm off the glass, about 6 mm tall. A 0.42 mm ledge sits on the glass face, 0.30 mm proud of it, with a ramp on the outside so the glass spreads the beam as it is pressed in. A beam this tall and this thin is a fingernail job, not a press: on the order of 4 N per hook to spread the ledge. The same ledge is what a fingernail lifts to take the panel off.

The ledges stand proud of the glass, so the shell has a pocket over each hook, 0.85 mm deep. The rest of the inner face is recessed 0.36 mm over the panel, which keeps a thick board (the +0.16 mm end of the JLCPCB tolerance) from pressing the glass into the shell. The recess stops at board x = 107.55. The button tubes are Ø8.6 at x = 112, so they already occupy the last 0.9 mm of the outline, and cutting the recess through them would leave the tube walls loose. The window chamfer starts 0.40 mm behind the inner face, so the 0.36 mm recess stays in front of it.

Under each hook a pad rises from the PCB to the panel back. The right-hand top pad also bridges across to the column at x 76–82, under the glass edge and past the ESP32 module (the module ends at y = 25.49, the pad starts at y = 25.99). Nothing in the carrier crosses the optos, L1, or the FPC bulge. The path between the top hooks and the bottom rail jogs east around C42, narrows to 0.85 mm to pass between the two C43 leads (x = 51.50 and x = 55.00), and narrows again to pass to the right of J1 pin 5 (x = 77.00).

The piece locates on two edges. A fence on the left, 0.25 mm off the board edge, and a fence on the bottom edge do the same job. Short ties join each fence to the rail on the board so a 0.4 mm nozzle does not weld the fence to the rail and eat the clearance. The bottom rail is the spine that makes the hooks, the fences, and the columns one solid.

That rail has to cross the front curtain, which is the wall between the hot zone and the panel at board x 27.7–29.0. The curtain floor is 1.1 mm above the PCB. For 1 mm of y at the board's bottom edge the curtain is cut up to 1.55 mm, and the rail through that slot is 1.00 mm tall. The remaining crack is about 0.55 mm high (about 0.39 mm if the board is thick) and well under 1 mm², smaller than the ~1.3 mm² leak already left where the rear rib and the front partition do not line up. The rail plugs the rest of the notch, so the curtain still separates the zones.

## Build order

Every step is done with the connector, the pins, or the snaps in sight. Nothing is inserted blind.

1. Screw the backplate to the wall. Four drywall-anchor screws, pan heads in the recesses at the four anchor points. The board is not on the plate yet, so the heads are reachable. The lip, the snap bumps, the ribs, and the standoffs face out of the wall.

2. Pull the cable through the centred 25 × 20 mm hole. Leave enough slack to mate the plug in the hand.

3. On the bench, board component-side up, latch open. Fold the tail 180° under the panel, contacts down, and slide it into J4 from the right until it stops. Close the latch with a fingernail. The panel is not down yet, so the latch is clear of the glass.

4. Set the carrier on the board. The left fence and the bottom fence drop onto the board edges; the pads sit on the soldermask. Press the panel down onto the four pads until the ledges click. The panel back is then 5.0 mm above the PCB. This is the subassembly: board, carrier, panel, tail.

5. Mate the 15EDGK plug onto J1 while the board is still in the hand. The plug faces the wire hole. Do this in sight, not against the wall.

6. Set the subassembly onto the four locating pins. Feed the wire loop back through the hole as the board goes down. The pins stop the board in x and y. The plug and C43 are in the space the standoffs leave above the plate.

7. Drop the three button caps into the shell from the inside. The flange is Ø7.4 mm, the face hole is Ø5.0 mm, and the counterbore behind the face is Ø7.9 mm, so the cap goes in from the back and does not fall out the front. Lower the shell over the stack. It slides onto the backplate lip. The clamp posts land around the four mounting holes and press the board onto the standoffs. Four bumps on the lip click into the snap windows. There are no panel clips to clear.

The light pipe, the RESET pinhole, the window, and the vents are all in the shell and do not depend on the carrier. The three airflow zones are unchanged: hot zone on the left (triacs and the opto drive), quiet middle over the wire hole (fill the wall opening with putty), sensor duct on the right. The baffle around the SHT40 island is still open to the right wall. The edge vents are still about 28% open area, on all four edges, with the triac heat kept on the far side of the curtain from the sensor.

## Taking it apart

Pry the shell at the snap windows, not by pulling the face. Pulling the face loads all four snaps at once. A small flat tool in one window on the bottom edge, then the other bottom window, then the top edge, flexes the 2.4 mm wall out over the 0.9 mm bump. The bumps are 45° on both sides, so the wall can climb off them. Lift the shell straight off. The caps stay in the shell.

Lift the subassembly off the pins. Unplug the HVAC plug in the hand.

To release the panel, lift the two ledges on one long edge with a fingernail and tilt the glass up. That unfolds the tail enough to reach the latch. Flip the latch up and pull the tail straight out of the mouth.

## A PCB change that would make this easier

Not done here. The board is unchanged.

J4's mouth is 4.7 mm inboard of the panel edge, and the latch opens toward the glass. Moving the connector so the mouth is to the right of x = 108.46, or turning it so the actuator faces a free edge instead of the panel, would let the latch be closed after the glass is down. The carrier sequence does not need that. It would also want a new look at the tail length, because the straight run would change.
