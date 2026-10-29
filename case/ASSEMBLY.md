# Assembly

The case is a 147 × 71 × 26.25 mm PETG shell, backplate, and panel carrier. The board, the carrier, and the display panel are one subassembly. The shell clips to the backplate. Four bridges printed in the shell press the display panel onto the carrier. The shell does not press the board.

Coordinates below are board coordinates unless noted: origin at the board's top-left, x to the right, y down, z up from the PCB top. Case z is out from the wall, with the wall at z = 0. The outer depth, 26.25 mm, is the shell front (`Z_out`).

## What has to fit

The panel is a Good Display GDEY029T94. The datasheet table gives the outline as 36.7 (H) × 79.0 (V) × 1.2 (D) mm and the active area as 29.056 × 66.896 mm. On the board that outline is x 29.46–108.46, y 26.24–62.94. The active area sits inside it with a 3.0 mm border on the left, 3.82 mm on the top and bottom, and 9.1 mm on the right, which is the end the cable leaves.

The mechanical drawing (datasheet page 7) dimensions the tail:

- free length past the outline, 14.40 mm
- width 12.50 ± 0.1 mm
- thickness 0.30 ± 0.03 mm
- 24 contacts at 0.5 mm pitch, span P0.5 × 23 = 11.50 mm
- contact length 3.66 mm
- stiffener length 6.20 ± 0.3 mm
- the drawing marks the contact side

Pin 1 is 23.5 mm from the tail edge that is nearest pin 24, and that edge is 11.50 mm from the outline. That lands pad 1 at board y = 49.74, which is where the footprint puts it. The contacts are on the display-face side of the tail. Folded 180° under the panel, they face the board.

J4 is a Jushuo AFC07-S24FCA-00 (LCSC C262567). It is a right-angle, 2.0 mm high, bottom-contact FPC connector with a horizontal slide lock, the "drawer" style. The drawing title says "0.5Pitch H=2.0 FPC ZIF R/A Type3 Lower SMT CONN". The mechanism view is a slider. On section A-A the 0.85 mm callout is the rear solder-tab foot, and it scales with the 0.50 mm pitch on the same sheet. The 1.40 mm callout next to it is the fitting nail. The length table fills DIM E as 5.20 mm and DIM F as 6.45 mm. For 24 contacts the drawing's four length columns are 11.50, 12.60, 16.50, and 17.60 mm. DIM B is 12.60, the applicable FPC-width column (`fpc_guide`). The overall length, ears included, is 17.60 mm, centered on the tail at board y 35.2–52.8. The housing body itself is board y 36.0–52.0. The 3.90 mm figure is an unlabelled side-view run back from the housing face. It is not a body depth and it is not a stroke. The drawing states no slider stroke. A stroke of 0.85 mm to 1.30 mm would need the vendor to confirm it. The STEP model puts a block 1.10 mm in front of the housing face: the silk mouth is at board x = 103.77 and the housing face is at board x = 102.675. The titled housing height is 2.00 mm, which is `j4_h`, and that is the height of the STEP and the WRL in this repo. Those files do not carry a 2.50 mm or a 2.70 mm callout. DIM E is 5.20 mm, and 5.20 − 3.90 = 1.30 mm, which lines up with the left block on the side view. The STEP block in front of the housing face is 1.10 mm, which is the silk mouth minus that face, and it is not that 1.30 mm. The mouth faces +x, so the tail enters from the right. Shut is a fingernail push toward −x on the high ear's +x face, with the display panel seated. Open toward +x is a finger on the printed push on the carrier, with the display panel seated. The recommended FPC thickness on the drawing is 0.30 ± 0.03 mm, which matches the panel tail. The same drawing recommends a contact length of 3.0 mm minimum.

The side view does not label a single "FPC centerline" height. The fold below uses 0.50 mm for the lower leg, because 0.50 ± 0.05 is the small vertical callout next to the 0.30 mm FPC thickness. That callout is not labeled as the centerline. A later section shows how far the insertion moves if the leg is 0.30 mm or 0.70 mm instead.

The optos under the left of the panel (U6, U7, U8, MOC3063S) are modeled at 4.0 mm tall, so the display panel has to clear them. The carrier holds the panel back 4.45 mm above the PCB top.

## The fold

The free tail starts at the outline, in the plane of the display panel back, and enters the bend there. There is no straight run on the panel. The lower leg runs left from the bend, level at 0.50 mm, into the mouth.

At a 4.45 mm gap and a 0.50 mm lower leg:

- bend diameter 3.95 mm, centerline radius 1.975 mm
- arc length π × 1.975 = 6.20 mm
- straight run from the panel edge (x = 108.46) to the mouth (x = 103.77) = 4.69 mm
- inserted length 14.40 − 6.20 − 4.69 = 3.51 mm

The outer fiber of the bend bulges about R + 0.15 = 2.13 mm past the panel edge, to board x ≈ 110.59. The shell pocket on that side is 4 mm deep, out to x = 112.46.

Raising the panel lengthens only the semicircle. The pull-out is π/2 ≈ 1.57 mm of tail per millimetre of extra gap, and it does not depend on the lower-leg height. `scripts/case_fit.py` prints the rows below and fails unless the 0.50 mm leg stays at or above 3.0 mm both at 4.45 mm and at a carrier printed 0.10 mm tall (gap 4.55 mm, inserted 3.35 mm). The other legs are printed and are not all gated.

| Lower leg | Gap 4.35 mm (short print) | Gap 4.45 mm | Gap 4.55 mm (tall print) |
|---|---|---|---|
| 0.30 mm | 3.35 mm | 3.19 mm | 3.03 mm |
| 0.50 mm | 3.66 mm | 3.51 mm | 3.35 mm |
| 0.70 mm | 3.98 mm | 3.82 mm | 3.66 mm |

4.45 mm is the nominal gap from the PCB top to `Z_panel_b`. The panel sits on the stop, 0.15 mm under that. A carrier that prints 0.10 mm tall opens the seated gap, and a short print closes it. A thicker board does not change the gap: it lifts the board, the carrier, and the panel together. Carrier print height does. The cable can still lift a thin corner out of J4. That reseat is in the bridge section below.

A 0.30 mm leg at the design gap inserts 2.96 mm, 0.04 mm under the drawing. That is the tail lying on the soldermask. The 0.50 mm figure is an unlabeled side-view number, so this row is the open question on the fold.

Neither the GDEY drawing in the part notes nor the Jushuo drawing states which face the tail leaves, and neither states a minimum bend radius. This model starts the free length at the outline in the plane of the display panel back. If the tail instead left flush with the display panel front, the bend would also climb the 1.2 mm panel and the inserted length at this gap would be 1.62 mm, which is under 3.0 mm. A centerline 0.15 mm off the display panel back (half the 0.30 mm tail) is inside the 0.50 mm leg at the nominal gap and fails the tall-print row. The Jushuo R0.20 is the housing fillet, not a tail radius.

The slider is shut after the display panel is seated, not while the panel is held up. Shut is a fingernail push toward −x on the high ear's +x face. That face, from the WRL slider, is at board x = 103.77, y 52.70–52.80, and from the PCB top up to 1.85 mm. The low ear has the same kind of face at y 35.20–35.30. The housing body is 2.0 mm high and the panel back is at 4.45 mm, so 2.60 mm remains above the bar (the bar top is 1.85 mm). A fingernail fits in that slot and reaches the face from the cable side. An 11 mm fingertip does not: the slot is 2.60 mm, the corridor beside the fold (y 50.25 to the post at y 60.40) is 10.15 mm, and a tilt about the fold cannot open 11 mm at the ear. The ear is 4.69 mm from the hinge, so even a 90° tilt leaves 2.60 + 4.69 = 7.29 mm there.

Lifting the seated panel with the slider locked pulls the tail out at π/2 mm per millimetre. Clearing the fences takes 0.80 mm of lift (the panel is 1.2 mm and the fence tops sit 0.40 mm under the face), which pulls 1.26 mm and leaves 2.25 mm inserted, under 3.0 mm. A finger box whose top sits at case z 28.9 mm (8 mm of finger starting 0.30 mm above the housing) needs the panel 5.9 mm up, about 9.2 mm of tail. The tail is 3.51 mm in. That pose is not the assembly pose.

The close pose the fit gates is the seated panel. The nail box runs from the high-ear face out past the panel edge (board x 103.75–108.97, y 51.40–54.20, z 0.40–1.50 mm above the PCB). It meets the face, stays off the panel, and stays off the carrier. The same box raised so its bottom is 1.10 mm above the bar does not meet the face, and the fit fails if that box is treated as a reach. An 11 mm fingertip on the face enters the panel, and the fit fails if that box is treated as a reach. The seated fold is the only bend in this pose: centerline radius 1.98 mm, tail 0.30 mm, outer-fiber strain 0.30 / (2 × 1.975) = 7.6%. Neither drawing states a minimum bend radius. The pose does not add a second kink.

The ear rear is not a finger face. It sits at board x = 100.767. The low rear runs y 36.13–37.75 (1.62 mm) and z 0.80–1.20 (0.40 mm tall). The carrier post ends at y 34.30 and the ear starts at y 35.20, so 0.90 mm is all that is beside the post. A nail cannot hook that. The finger face is the printed push, described with the carrier. Tilting the panel does not open 11 mm at the ear.

With the slider open the tail can shift sideways in the connector. The drawing's applicable-width column is 12.60 ± 0.10 mm and the tail is 12.50 ± 0.10 mm. The play is 0.10 mm at the nominal widths, 0.20 mm on a narrow tail in a nominal guide, and none on a wide tail in a nominal guide. The two tolerances together give 0.30 mm at the loose extreme and −0.10 mm at the tight extreme. A negative play means that tail does not enter that guide. The shell does not slide sideways, so the sideways travel is 0, and that fits every non-negative play, including the wide tail's 0.00 mm. The −0.10 mm pair is a parts mismatch. That stack is printed and is not a gate, because a wide tail in a narrow guide does not assemble. `case_fit.py` reads the tail width from the fold geometry and fails if the modeled tail is wider than the guide, or if the sideways travel exceeds the smallest non-negative play.

## Why the panel sits on the carrier

The mouth is under the panel. The window is smaller than the panel, so the panel cannot drop in through the front. Assembly mates J4 with the panel in the hand, then sets the panel down on the carrier pads. The fences and the cable-end posts locate it. Nothing hooks it, and there is no release step.

The shell clips are the four bumps on the backplate lip. The bridges on the long edges of the window press the display panel. The shell does not press the board.

## The carrier

The carrier is one PETG piece. It sits on the PCB and holds the panel back at 4.45 mm. It locates on the board edges. The locating pins are Ø3.9 and stick through the board by 1.4 mm (`pcb_t − 0.2`), which is more than the snap-window play, so the board stays on the pins while the shell is on and while the shell comes off. The carrier stays clear of those pins.

The display panel rests on four pads:

- top edge, x 47.0–53.0 and x 76.0–90.6
- bottom edge, x 49.6–55.6 and x 76.0–82.0

Each pad rises from the PCB to the panel back. The right-hand top pad also bridges across to the column at x 76–82, under the panel edge and past the ESP32 module (the module ends at y = 25.49, the pad starts at y = 25.99). Nothing in the carrier crosses the optos, L1, or the FPC bulge. The path between the top pads and the bottom rail jogs east around C42, narrows to 0.85 mm to pass between the two C43 leads (x = 51.50 and x = 55.00), and narrows again to pass to the right of J1 pin 5 (x = 77.00). On the mesh the pads kiss the panel (clearance −0.05 mm). The fit fails if that kiss is under −0.08 mm or over 0.12 mm.

Fences stand beside the long edges and stop 0.40 mm short of the shell face. They do not cross onto the panel face. Each fence is 0.80 mm thick and stands 0.20 mm off the outline. A 1.50 mm tie crosses under the outline so the fence joins the pad, and that tie stays below the panel, so the 0.20 mm air at panel height stays open. The mesh reads solid in the wall and air in the gap.

- top edge, x 42.0–53.0 and x 84.6–96.0, clear of U7 (which ends at x 40.2) and of the ESP32 (x 66–84.05)
- bottom edge, x 49.6–55.6 and x 76.0–90.0

The datasheet gives the outline width as 36.7 mm only. The check also tries 36.50 mm and 36.90 mm, and that band is an assumption around the one drawing number. If the panel sits on the bottom fence, the top gap is `2 × fence_air + (36.70 − width)`: 0.60 mm, 0.40 mm, and 0.20 mm for those three widths at 0.20 mm of air. A negative top gap fails. The travel between the fences on the nominal width is `2 × fence_air`, 0.40 mm. That is the coarse stop in y. The locked tail is the fine stop (0.10 mm at the nominal guide). `fence_air` of 0.60 mm is 1.20 mm of travel and the fit rejects it. Nothing else, including the shell, stops the panel in y.

A fence on the left edge of the panel does not fit. The front curtain ends at board x = 29.0 and the outline starts at x = 29.46, which is 0.46 mm, too little for a 0.4 mm wall plus clearance. Sliding the panel toward −x lengthens the tail in the connector. The seated tail is already against the mouth, so that direction does not unload J4. The curtain is clearance, not a locator.

The panel can also slide in +x, and +x shortens the tail in J4 by the same amount. The 3.35 mm tall-print insertion has 0.35 mm before it is under 3.0 mm. Two posts on the cable side of the outline stop that slide. They bite 0.05 mm onto the panel edge, at board y 33.20–34.30 and y 60.40–61.70, clear of the button tubes and of the bend bulge, and they stop 0.45 mm under the shell face. Each post's footprint continues down to the carrier bottom, so the slicer does not tower under it. A carrier that prints 0.10 mm short of the panel then leaves 0.05 mm of slide, and the inserted length is 3.30 mm, above 3.0 mm. A fin just outboard of the same edge, at y 32.75–33.25 and y 59.70–60.45, joins the post and is the insertion-depth mark. The seated edge lines up with the fin. The fin does not cross the face and does not add bite. The posts are the stop. The fit reads the fin solid and the inboard side air, and it fails if the mark is missing.

The piece locates on two board edges. A fence on the left, 0.25 mm off the board edge, and a fence on the bottom edge do that job. Short ties join each fence to the rail on the board so a 0.4 mm nozzle does not weld the fence to the rail and eat the clearance. The bottom rail is one solid from the curtain crossing to the right column. The curtain crossing stays 1.00 mm tall, at board x 27.15–29.55.

That rail crosses the front curtain, which is the wall between the hot zone and the panel at board x 27.7–29.0. The curtain floor is 1.1 mm above the PCB. For 1 mm of y at the board's bottom edge the curtain is cut up to 1.55 mm, and the rail through that slot is 1.00 mm tall. The remaining crack is about 0.55 mm high (about 0.39 mm if the board is thick) and well under 1 mm², smaller than the ~1.3 mm² leak where the rear rib and the front partition do not line up. The rail plugs the rest of the notch, so the curtain separates the zones.

The carrier in `thermostat-case-P2S.3mf` has supports off (`enable_support` is 0). The fences and the posts stand on columns that run down to the bed. A slice of this orientation is 0 mm of support at the open threshold and about 10 mm at the two tighter ones. The numbers for every part are with the button caps below.

## The shell and the backplate clips

The shell drops over the subassembly and snaps to the backplate. Four bumps on the lip, two on the bottom edge and two on the top, are the only clips. The polygon that draws each bump has its apex 0.80 mm outside the lip face. The ±0.90 mm of that same polygon is the bump's z half-span (the bump runs about z 2.3–4.1), not the climb. The shell's inner face sits 0.30 mm off the lip, so the wall moves 0.80 − 0.30 = 0.50 mm to pass the apex. The free length is that wall from the bump (z = 3.2) up to the front face (z = 24.25), which ties the four walls: L = 21.05 mm. The local width is the 11 mm snap window. With E = 1.8 GPa,

- strain ε = 1.5 × 2.4 × 0.50 / 21.05² = 0.41%
- force on one window, as a cantilever, F = 3 E I δ / L³ = 3.67 N, I = 11 × 2.4³ / 12
- the same section as a fixed-guided beam is 12 E I δ / L³ = 14.7 N
- both long walls, each counted once as that guided beam, are 29.3 N

3.67 N is one snap window, a cantilever of the 11 mm local width. Four separate cantilevers would be 14.7 N. The push that seats all four clips, if each long wall bends as one fixed-guided beam of that same 11 mm section, is 29.3 N. That is the figure to quote next to the 3.67 N. It is an assumption about the wall between the two bumps, not a second measurement.

The ramp from the lip up to the apex is atan(0.80 / 0.90) = 41.6° from horizontal. The polygon's ±0.90 mm is the z half-span the ramp runs along.

The shell prints face-down, so the wall's length (case z, the 21.05 mm) runs along the build axis. The outer fiber of the bend is across the layers. 0.41% is under the 1.0% interlayer lower bound (the elongation band whose lower end is 1.0%).

The 2.4% gate is the Ultimaker PETG flexural strain, 3.6%, divided by 1.5. `scripts/case_fit.py` recomputes both and fails if the climb is 0.20 mm or less, if L is 11 mm or less, if the strain reaches 2.4%, or if the force reaches 8 N. A wall of L = 8 mm at this same climb is 2.81% and the script asserts that case fails. The mesh probe finds the bump solid 0.25 mm inside the apex, the window air at the apex (signed distance −1.00 mm), and shell solid 8 mm beside the window. A box that fills the window is solid at the apex, and the script asserts that filled window fails.

The active area and the fold stay clear of the shell while it sits, and while it is lifted 1 mm and 2 mm off the clips. The fit fails if that clearance is 0.10 mm or less. The same samples cover the rest of the display panel. The four land boxes are the only exemption. Outside those boxes the shell may meet the panel front and must not enter it, so a bump on the border between the spans fails. A shell pushed 1 mm onto the panel enters it, and the script asserts that case fails. The shell mesh is one body. The board slab, including a thick board, stays 0.50 mm off the shell. A clamp post added on a mounting hole enters the board, and the script asserts that case fails.

The inner face is recessed 1.26 mm over the panel border. The bridges and their pillars stand in that recess. The recess is deep enough for the 0.80 mm beam and the 0.45 mm land, and it leaves skin under the face.

Putting the shell on, or taking it off, does not shift the display panel and does not load the tail. The panel is already on the pads, between the fences and against the +x posts. The shell's clearance stays positive through the 2 mm lift. The locating pins keep the board. The shell's sideways travel is 0, which fits the smallest non-negative guide play.

## The shell bridges

The display panel drops straight onto the carrier. The pads and the added columns top out at z 22.90, 0.15 mm under the nominal panel back. Fences stand 0.20 mm off the long edges. Ribs on the shell stand 0.10 mm off the outer face of each fence, and the rib bottoms reach `Z_in − 1.85`, so the rib and the fence overlap at every stack corner.

Each bridge is 0.80 mm thick and 1.20 mm wide, with a land 0.50 mm wide and 0.45 mm tall. The lands stay between 1.92 mm and 3.32 mm from the edge. The right-hand land is 6 mm from its pillar. At the worst corner the pillars stay 0.185 mm off the panel, and the beam stays 0.135 mm under the recess ceiling. The beam underside is flush with the nominal panel front and 0.15 mm above the panel on the stop. The thick-corner root is 1.18%.

With the panel on the stop, the tail inserts 3.98 / 3.74 / 3.51 mm at carrier −0.15 / 0 / +0.15. The target is 3.15 mm. A thin corner has a gap under the land, and the cable can lift that end out of J4 from day one. After the bridges relax, more corners do the same. Take the shell off, press the tail back in, and close the slider.

Under a 10 N press with the preload gone, U7 keeps 0.04 mm and U8 keeps 0.12 mm. The top-left column is what holds U7 off the panel.

## The slider push

The push is PETG on the carrier. A finger presses its −x face toward +x. The display panel stays on the pads. The shoe at the other end sits in the high-ear window, 0.22 mm before the ear rear at board x = 100.767, and the press drives that shoe toward +x.

The contact face is board x = 114.80, y 53.70–64.70 (11.0 mm), z 3.70–4.80 mm above the PCB. The bay beside the seated panel, inboard of the airflow partition, is 5.30 mm deep, so the fingertip in the fit is 11 mm wide, 5.30 mm deep, and 8 mm thick. The same check with a 16 mm width also reaches. A pad that stops 1.10 mm short of the face does not, and a 10 mm face does not. The shell is off for this press. The beam at rest stays clear of the shell.

The rod that reaches the shoe stands on a spine down to the bed. The shoe itself bridges the last few millimetres, over the connector, where a foot would hit the housing. The beam is a cantilever along y, 0.80 mm thick in x and 1.10 mm tall, free length 8.80 mm, travel 0.75 mm at the free end. The bend is in the plane of the layers. With E = 1.8 GPa,

- strain ε = 1.5 × 0.80 × 0.75 / 8.80² = 1.16%
- force F = 3 E I δ / L³ = 0.28 N, I = 1.10 × 0.80³ / 12

1.16% is under the 2.4% flexural gate. 0.28 N is the force to deflect the push through its travel. The drawing does not state the slider's actuation force. The shoe travel is 0.75 mm; about 0.22 mm of that is the air in front of the ear, and the rest is push on the slider. The fit fails if the strain reaches 2.4% or the force reaches 8 N.

## Buttons

The captive caps stay. The shell is positioned by the backplate snaps, so the leaf only has to keep the cap in the face. It does not locate the display panel and it does not locate the board.

UP, DOWN, and MODE are at board (112, 30), (112, 40), and (112, 50). The pitch is 10 mm and the guide tubes are Ø8.6. The front partition overlaps the tube wall.

The switch is a TS-1187A-B-A-B. Height code A is 1.5 ± 0.05 mm, which is how tall the actuator stands, not how far it moves. The drawing calls the travel 0.2 ± 0.1 mm and the spec table says 0.25 mm. Force code B is 160 gf. The actuator is Ø2.0 and the body is 5.1 mm square, with the cover at 1.2 mm. The stem is Ø1.6, so it meets the actuator and not the cover.

Each cap is captive in the face. A Ø4.70 head passes through the Ø5.0 hole with 0.15 mm to spare. Behind the hole a cone opens to Ø6.0 at 65° from horizontal and meets the inner corner of the hole, so the cap cannot leave through the front. One leaf on the partition side of the stem carries the catch. The catch sits behind a lip in the tube (the lip's hole is Ø6.3, the bore around it is Ø7.7). The panel clearance removes the panel side of each tube, which is why the leaf faces the partition, where the lip is present. Two leaves of this length do not fit in that bore on the partition side.

The leaf is a straight vertical wall, 0.40 mm thick and 4.50 mm long. A rib clamps the first 0.15 mm, so the free length is 4.35 mm. The barb at the free end stands 0.30 mm past the lip and has to bend that far in x to pass. With the rectangular-beam formula ε = 1.5 t δ / L²,

ε = 1.5 × 0.40 × 0.30 / 4.35² = 0.95%

`scripts/case_fit.py` recomputes that and fails if the free length is under 4 mm or the strain reaches 2.4%, the same flexural limit as the backplate clips. The model itself asserts the leaf under 2.0%. The pressed-catch check is the barb face against the lip, and that face kisses the lip (clearance 0). The seated cone kisses the hole corner. A seated cap stays 0.54 mm off the display panel, and a pressed cap stays 1.09 mm off it.

There is no second spring. A leaf that stayed on the lip through the whole 1.50 mm of travel would be several times this strain. The cap can move the full 1.50 mm between the cone and the lip. The switch's own 160 gf is what pushes it back out, and only after the stem has crossed the gap. Off the board, the cap rattles. It cannot fall out of the front, and it cannot fall out of the back.

The shell z comes from the backplate snaps. Board thickness stays in the stack because the actuator is on the board. The snap windows run z 2.2–4.2 and the bumps are about 2.3–4.1, so a shell that is not seated in the middle of the window has about ±0.10 mm of extra play. Moving the whole shell changes the gap and not the travel, because the cone and the lip move together. That play is in the stack below.

Nominal gap (seated stem to the actuator) is 0.63 mm. Nominal travel (cone on the hole, to the catch on the lip) is 1.50 mm. Nominal plastic stroke is 0.87 mm. The switch spec table travels 0.25 mm, and the drawing says 0.2 ± 0.1 mm. The cover is the stop a finger feels. The lip is 0.62 mm past the spec-table bottom-out (1.50 − 0.63 − 0.25). At the drawing's 0.3 mm maximum travel the lip is 0.57 mm past bottom-out. The switch sheet does not give an over-travel rating, so this is a plastic stop beyond the stated travel, not a rated over-travel. The pressed mesh is past the cover: mashing past the click loads the cover until the lip. The fit script allows the stem to pass the actuator in that pose.

The tolerance check is a uniform Monte Carlo, N = 200000, seed 1. Six terms: printer z ±0.10 mm on the gap, an independent ±0.10 mm on the travel (the cap and the shell are separate prints), standoff ±0.15 mm, board ±0.16 mm, switch height ±0.05 mm, and snap-seat play ±0.10 mm. It reports P(gap ≤ 0) = 0%, P(stroke < 0.20 mm) = 0%, worst-case gap 0.07 mm, worst-case stroke 0.21 mm. The script fails if either probability is over 1% / 2% or the worst stroke is under 0.20 mm. A normal model that drops the board term and treats the remaining ± as 1σ is printed beside it and is not the gate.

In the finished cap the leaf is a cantilever. It is anchored at the rib, 0.40 mm thick, free length 4.35 mm, and it is not a bridge between two walls. Print the caps head-down, face on the bed. The cone and the barb underside are 65° from horizontal. The root rib is 72°, so a 60° threshold does not support it. In the print frame the cone's wide end is at z = 3.87. The tie stands 0.34 mm above that face and 0.06 mm into the leaf, so the leaf bottom bridges from the root rib onto the nub. One corner of the tie also meets the bed outside the Ø4.70 head. Snap the nub off before the cap goes into the tube. The 3MF has supports off (`enable_support` is 0). A slice of these meshes needs support filament about as follows. Carrier, feet on the bed: 0 mm at the open threshold, 10 mm and 10 mm at the two tighter ones. The fence columns run down to the bed. Turning the carrier over puts the pads in the air. Backplate, plate on the bed: 0 mm, 0 mm, and 150 mm at 60°. The shallow faces are the snap bumps and the ribs. Shell, face on the bed: about 1.4–1.6 m at every threshold. That length is the button bores and the window lip, which are horizontal shelves inside vertical holes, so turning the shell does not remove it. The face-down orientation is the one that puts the outer face on the bed. It does not make the shell support-free.

## Build order

The backplate is screwed to the wall. Every step after that is fingers. The snap window is 2.0 mm tall and 11 mm wide. A fingernail in that window starts the wall. No screwdriver.

1. Screw the backplate to the wall. Four drywall-anchor screws, pan heads in the recesses at the four anchor points. The board is not on the plate yet, so the heads are reachable. The lip, the snap bumps, the ribs, and the standoffs face out of the wall.

2. Pull the cable through the centred 25 × 20 mm hole. Leave enough slack to mate the plug in the hand.

3. Set the carrier on the board. The left fence and the bottom fence drop onto the board edges, and the pads sit on the soldermask.

4. Fold the tail 180° under the panel, contacts down. Drop the display panel onto the pads. Slide the tail into J4 from the right until the panel edge meets the posts and lines up with the depth marks. With a fingernail in the slot under the panel, push the high ear's +x face toward −x. The face is at board x = 103.77, y 52.70–52.80. The fold stays the seated semicircle. There is no click. The panel back is on the stop at 22.90. This is the subassembly: board, carrier, display panel, tail.

5. Mate the 15EDGK plug onto J1 while the board is in the hand. The plug faces the wire hole.

6. Set the subassembly onto the four locating pins. Feed the wire loop back through the hole as the board goes down. The pins stop the board in x and y. The plug and C43 are in the space the standoffs leave above the plate.

7. Snap the three button caps into the shell from the inside, before the shell goes near the board. Snap the print tie off each cap first. Lay the shell face-down, push each head through its hole with a fingertip on the leaf, and press until the cone seats and the barb clicks behind the lip. The barb bends 0.30 mm to pass. The caps stay in the shell from here on. They can rattle between the cone and the lip.

8. Lower the shell over the stack and press the side walls over the backplate bumps with the fingers until they snap. The walls climb 0.50 mm. The ribs take the fences. The lands meet the border where the stack is thick, and stay clear where it is thin. The shell does not touch the board. The stems come down onto the three actuators with the 0.63 mm gap.

The light pipe, the RESET pinhole, the window, and the vents are in the shell. The three airflow zones are the hot zone on the left (triacs and the opto drive), the quiet middle over the wire hole, and the sensor duct on the right. Close the wall opening with a printed PETG plug around the cable, the same material as the case. The baffle around the SHT40 island is open to the right wall. The edge vents are about 28% open area, on all four edges, with the triac heat on the far side of the curtain from the sensor.

## Taking it apart

1. Unsnap the shell walls at the snap windows with a fingernail. The window is 2.0 mm tall and 11 mm wide, which is the purchase. The bumps are 0.80 mm outside the lip and 41.6° on both sides, so the wall climbs off them. Lift the shell straight off. The button caps stay in it. The bridges leave the border as the shell comes off. The display panel stays on the pads, between the fences and against the cable-end posts. The board stays on the locating pins. The shell does not slide sideways. To take a cap out, press the single barb toward the stem and push the cap out the back of the hole.

2. Lift the subassembly off the pins. Unplug the HVAC plug in the hand.

3. If the cable end has lifted out of J4, press the tail back in and close the slider before the shell goes back on. To take the panel out, press the carrier push toward +x with a fingertip on its contact face (board x = 114.80, y 53.70–64.70). The shoe moves the high-ear slider toward +x. Draw the tail out toward +x only after the slider is open, then lift the panel off the pads.

## Where J4 sits

J4 is the footprint in `kicad/`: silk mouth at board x = 103.77, 4.69 mm inboard of the panel edge, slider under the panel. The housing face on the STEP is at x = 102.675, and the housing body is board y 36.0–52.0. Assembly drops the panel onto the pads, slides the tail in until the edge meets the posts and the depth marks, and shuts the slider with a fingernail on the high ear's +x face. The cable end can leave J4 from day one at a thin corner. Service is the shell off, the tail pressed back in, and the slider closed. The connector stays on the footprint.
