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
- contact length 3.66 mm
- stiffener length 6.20 ± 0.3 mm
- the drawing marks the contact side

Pin 1 is 23.5 mm from the tail edge that is nearest pin 24, and that edge is 11.50 mm from the outline. That lands pad 1 at board y = 49.74, which is where the footprint puts it. The contacts are on the display-face side of the tail. Folded 180° under the panel, they face the board.

J4 is a Jushuo AFC07-S24FCA-00 (LCSC C262567). It is a right-angle, 2.0 mm high, bottom-contact FPC connector with a horizontal slide lock, the "drawer" style. The drawing title says "0.5Pitch H=2.0 FPC ZIF R/A Type3 Lower SMT CONN". The mechanism view is a slider, not a flip latch: the slider-open dimension is 0.85 mm, sticking out on the cable side of the housing. The mouth faces +x, so open is +x and shut is −x. DIM_B, the body depth, is 3.90 mm. For 24 contacts the drawing's four length columns are 11.50, 12.60, 16.50, and 17.60 mm. The side view also shows 2.70, 2.20, 0.50 ± 0.05, 0.30 ± 0.03, and R0.20. R0.20 is a housing fillet. The footprint's silk front face is at board x = 103.77, and the mouth faces +x, so the tail enters from the right. The recommended FPC thickness on the drawing is 0.30 ± 0.03 mm, which matches the panel tail. The same drawing recommends a contact length of 3.0 mm minimum.

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

The slider is shut before the glass comes down for assembly. Shut is −x. Open is +x, the 0.85 mm tab on the cable side. The body is 2.0 mm high and the glass back is at 4.6 mm, so 2.6 mm remains above the housing with the panel seated. A straight blade along +x does not reach the mouth. The fold wall sits at board x ≈ 110 for y 37.75–50.25, and the carrier posts at y 33.20–34.30 and y 60.40–61.70 run out to x = 109.26 under the glass. The openings left on the +x side of the fold are 10.15 mm (y 50.25–60.40) and 3.45 mm (y 34.30–37.75). The pick is a 1.40 mm by 0.80 mm blade leaning 66° off +x, which is inside the 60–78° band a straight blade needs to clear the fold and those posts. The tip is at the mouth, board (103.55, 48.60), centered in the gap at z = 21.9. The blade runs to an elbow at y = 55.60 and the handle continues along +x to x = 128, through the J2 notch in the button partition. Service holds the glass down and translates the whole tool along +x by 0.85 mm. The lean does not change. That translation opens the slider. `case-section-j4.png` is that gap seen from above, shell off, glass seated. The crimson part is the tool, leaning through the gap, with the handle running along +x. The blue frame is the glass outline. `scripts/case_fit.py` sweeps the tool from shut to open and fails if the lean leaves 60–78° or if the sweep comes within 0.20 mm of the carrier, the fold, the glass, or the shell. On the current mesh that sweep is 4.10 mm from the carrier, 2.47 mm from the fold, 0.90 mm from the glass, and 0.30 mm from the shell. A straight shaft through the fold is the control, and that one intersects the fold, so the path test can fail. With the lock shut, each millimetre of lift pulls 1.57 mm of tail out, and the 3.11 mm tall-print insertion has 0.07 mm of lift before it is under 3.0 mm. A sideways slide with the lock shut shears the tail. Service opens the slider before the glass moves. On the bench the panel is held up, the tail is slid in from the right, and the slider is pushed shut, which is −x. The tab is small; this flat tool is the reliable way to move it. The EasyEDA model on the footprint is the same part at the same silk line, and it is not a measured copy of the drawing.

With the slider open the tail can shift sideways in the connector. The drawing's applicable-width column is 12.60 mm and the tail is 12.50 ± 0.1 mm, so the play is 0.10 mm at the nominal width, 0.20 mm on a narrow tail, and none on a wide tail. The glass slide, once the hooks are open, is 0.10–0.60 mm (below). The 12.60 mm guide covers the small end of that slide at a nominal tail and does not cover 0.60 mm. `case_fit.py` prints both numbers.

## Why the panel is on a carrier

The mouth is under the panel. With the shell on, the shell wall covers the slider. With the shell off, the leaning pick reaches the mouth while the glass is seated. The same sweep also clears the shell, through the J2 notch, by 0.30 mm. The window is smaller than the panel, so the glass cannot drop in through the front. Assembly mates J4 with the panel in the hand, then presses the glass onto the hooks.

The shell never touches the glass. Four hooks on the carrier hold it.

## The carrier

The carrier is one PETG piece. It sits on the PCB, holds the panel back at 4.6 mm, and locates on the board edges rather than on the standoff pins. The pins are Ø3.9 and stick through the board, and the shell's clamp posts land on the same holes, so the carrier stays clear of them.

Four hooks, two on each long edge, and none on the cable end:

- top edge, x 47.0–53.0 and x 84.6–90.6
- bottom edge, x 49.6–55.6 and x 76.0–82.0

Each hook is a 0.42 mm beam (one perimeter of a 0.4 mm nozzle), 6 mm wide, standing 0.25 mm off the glass. Only the bottom 0.40 mm of the beam, the foot, bridges to the pad. The pad is inboard of the glass edge, so it does not fill the beam. The free length is the glass front minus that foot:

L = 24.4 − (18.6 + 0.40) = 5.40 mm

From the solder mask (z = 18.6) to the glass front (z = 24.4) is 5.8 mm. A foot has to sit in that span, so a 5.9 mm free beam does not fit above the board. The ledge crosses the glass by 0.32 mm. Snapping the glass on deflects each beam by that 0.32 mm. The shelf on the glass face is 0.25 mm tall. Above the shelf a lead-in rises 0.32 mm while it steps back 0.32 mm, so the lead-in is 45°. With E = 1.8 GPa,

- strain ε = 1.5 × 0.42 × 0.32 / 5.40² = 0.69%
- force F = 3 E I δ / L³ = 0.41 N per hook, I = 6 × 0.42³ / 12

`scripts/case_fit.py` recomputes both and fails if the strain reaches 2% or the force reaches 8 N. It also probes the mesh: the beam is solid, the 0.25 mm gap beside it is air, and the foot is solid. Holding one hook out 0.32 mm takes that same 0.41 N. The lead-in is the top face of the lip. The underside of the shelf is the glass front, so a tool has no gap under the ledge.

The relaxed shelf tips are 36.06 mm apart: the 36.70 mm outline minus a 0.32 mm ledge on each edge. The datasheet gives that outline as 36.7 mm only. Glass in the check is 36.50, 36.70, and 36.90 mm wide, and that band is an assumption around the one drawing number, so the opening that lets it slide out is 0.44, 0.64, and 0.84 mm. Release holds both hooks of one long edge out by that amount plus 0.05 mm, then slides the glass toward that edge until it sits in the opened gap. Both shelves then clear the glass by 0.025 mm. On a 36.70 mm glass the hold-out is 0.69 mm. From a glass sitting in the middle of its play the slide is 0.345 mm. The glass can already be against either beam, 0.25 mm off that middle, and the lip still holds it, so the slide that is left is 0.095–0.595 mm, the 0.10–0.60 mm range. At 36.90 mm the service hold-out is 0.89 mm, the strain is 1.92%, and the force is 1.13 N per hook, 2.26 N for the pair. At the 0.84 mm opening, before the 0.05 mm of air, the strain is 1.81% and the force is 1.07 N. The bending stress at 1.92% is E × 0.0192 ≈ 35 MPa. The beam stands up off the bed, so that stress is across the layers, which is the weak direction of PETG, and 35 MPa is about what that direction holds. If each lip prints 0.10 mm long, the 36.90 mm hold-out grows by 0.20 mm to 1.04 mm, which is 2.25% and 1.32 N per hook, over the 2% gate. `case_fit.py` prints that row. The nominal gate stays the 36.90 mm service case, which is the 1.92% row. The script fails if either shelf overlaps the glass after the slide, or if the strain reaches 2% or the force reaches 8 N, for either long edge and for each of those three widths.

Each hook has a head outboard of the beam. A window 4.0 mm along the hook and 1.30 mm across, open at the top, sits between the beam and an 0.80 mm wall. The wall's inboard face faces the glass. A 1.0 mm blade in the window bears on that face and pushes the beam outward. The underside of the head is 50° from horizontal. The root of the beam stays the full 0.42 mm, which is the section in the strain formula. The top pair is 37.6 mm apart (centers at x 50.0 and 87.6). The bottom pair is 26.4 mm apart (centers at x 52.6 and 79.0). `case-section-release.png` is the cut through the first top window, at board x = 50, shell off, with the blade against that face. `case-section-release-out.png` is the same cut with those two hooks held out 0.69 mm and the glass slid the centred 0.345 mm toward them. The glass is blue in that view and the pads are a darker tan, so the two do not read as one mass. The real slide is the 0.10–0.60 mm range above, depending on which beam the glass was already against.

The ledges stand proud of the glass, so the shell has a pocket over each hook, 1.10 mm deep. The rest of the inner face is recessed 0.70 mm over the panel. A thick board (+0.16 mm), a carrier that prints 0.10 mm tall, a shell sitting 0.10 mm low in the snap window, and a standoff 0.15 mm tall all lift the glass toward the face, 0.51 mm together. The recess leaves 0.19 mm past that stack. It stops at board x = 107.55. The button tubes are Ø8.6 at x = 112, so they occupy the last of the outline, and cutting the recess through them would leave the tube walls loose. The window chamfer starts 0.85 mm behind the inner face, so the recess stays in front of it.

Under each hook a pad rises from the PCB to the panel back. The right-hand top pad also bridges across to the column at x 76–82, under the glass edge and past the ESP32 module (the module ends at y = 25.49, the pad starts at y = 25.99). Nothing in the carrier crosses the optos, L1, or the FPC bulge. The path between the top hooks and the bottom rail jogs east around C42, narrows to 0.85 mm to pass between the two C43 leads (x = 51.50 and x = 55.00), and narrows again to pass to the right of J1 pin 5 (x = 77.00).

The glass can also slide in x, and +x lengthens the straight run into J4 by the same amount. The 3.11 mm tall-print insertion has 0.11 mm before it is under 3.0 mm. Two posts on the cable side of the outline stop that slide. They bite 0.05 mm onto the glass edge, at board y 33.20–34.30 and y 60.40–61.70, clear of the button tubes and of the bend bulge. Each post's footprint continues down to the carrier bottom, so the slicer does not tower under it. A carrier that prints 0.10 mm short of the glass then leaves 0.05 mm of slide, and the tall-print insertion is 3.06 mm, above 3.0 mm. With the glass against the stop that insertion is the 3.11 mm row. The other way, −x, runs into the front curtain 0.46 mm away, and that direction shortens the straight run, so the tail goes further in rather than out.

The piece locates on two edges. A fence on the left, 0.25 mm off the board edge, and a fence on the bottom edge do the same job. Short ties join each fence to the rail on the board so a 0.4 mm nozzle does not weld the fence to the rail and eat the clearance. The bottom rail is the spine that makes the hooks, the fences, and the columns one solid. At the two bottom hooks the rail is only foot-height (0.40 mm) where it meets the beam, and the tall rail stops inboard of the glass edge, so the beam stays free.

That rail crosses the front curtain, which is the wall between the hot zone and the panel at board x 27.7–29.0. The curtain floor is 1.1 mm above the PCB. For 1 mm of y at the board's bottom edge the curtain is cut up to 1.55 mm, and the rail through that slot is 1.00 mm tall. The remaining crack is about 0.55 mm high (about 0.39 mm if the board is thick) and well under 1 mm², smaller than the ~1.3 mm² leak where the rear rib and the front partition do not line up. The rail plugs the rest of the notch, so the curtain separates the zones.

In the print the hook shelf is a horizontal lip 0.57 mm deep (the 0.25 mm air gap plus the 0.32 mm ledge). The lead-in above it is 45°. The lip is a cantilever, not a bridge. PrusaSlicer 2.9.2 (gneiss15's build of 2.9.2), 0.20 mm layers, a 0.4 mm nozzle, supports enabled, grid style, sheath on, three interface layers, 2.5 mm support spacing, 50% XY separation, 0.2 mm contact distance, and "Don't support bridges" on, builds a tower under each of the four shelves. At a 30° overhang threshold the carrier's support-plus-interface extrusion is 536 mm of filament. At 45° it is 606 mm, and at 60° it is 613 mm; those two thresholds also leave a small contact beside one head. Those three lengths are extruded filament for the support and the three interface layers together, not a toolpath centreline. The same slice with the interface layers left out is 488 mm, 555 mm, and 562 mm. Pull the towers off before the glass goes on. The carrier in `thermostat-case-P2S.3mf` has supports off (`enable_support` is 0). The lengths above are a separate slicer run with supports turned on.

## Buttons

UP, DOWN, and MODE are at board (112, 30), (112, 40), and (112, 50). The pitch is 10 mm and the guide tubes are Ø8.6, so the front partition overlaps the tube wall and the recess in the face stops at x = 107.55.

The switch is a TS-1187A-B-A-B. Height code A is 1.5 ± 0.05 mm, which is how tall the actuator stands, not how far it moves. The drawing calls the travel 0.2 ± 0.1 mm and the spec table says 0.25 mm. Force code B is 160 gf. The actuator is Ø2.0 and the body is 5.1 mm square, with the cover at 1.2 mm. The stem is Ø1.6, so it meets the actuator and not the cover.

Each cap is captive in the face. A Ø4.70 head passes through the Ø5.0 hole with 0.15 mm to spare. Behind the hole a cone opens to Ø6.0 at 65° from horizontal and meets the inner corner of the hole, so the cap cannot leave through the front. One leaf on the partition side of the stem carries the catch. The catch sits behind a lip in the tube (the lip's hole is Ø6.3, the bore around it is Ø7.7). The FPC pocket removes the panel side of each tube, which is why the leaf faces the partition, where the lip is present. Two leaves of this length do not fit in that bore on the partition side.

The leaf is a straight vertical wall, 0.40 mm thick and 4.50 mm long. A rib clamps the first 0.15 mm, so the free length is 4.35 mm. The barb at the free end stands 0.30 mm past the lip and has to bend that far in x to pass. With the rectangular-beam formula ε = 1.5 t δ / L²,

ε = 1.5 × 0.40 × 0.30 / 4.35² = 0.95%

`scripts/case_fit.py` recomputes that and fails if the free length is under 4 mm or the strain reaches 2%. The pressed-catch check is the barb face against the lip, and that face kisses the lip (clearance 0). The seated cone kisses the hole corner.

There is no second spring. A leaf that stayed on the lip through the whole 1.50 mm of travel would be several times this strain. The cap can move the full 1.50 mm between the cone and the lip. The switch's own 160 gf is what pushes it back out, and only after the stem has crossed the gap. Off the board, the cap rattles. It cannot fall out of the front, and it cannot fall out of the back.

The shell is fixed to the backplate. The clamp posts are cylinders that start at z = Z_pcb_t + 0.1, 0.1 mm of air above a nominal board, so they do not make the shell follow the board. Board thickness stays in the stack. The snap windows run z 2.2–4.2 and the bumps are about 2.3–4.1, so a shell that is not seated in the middle of the window has about ±0.10 mm of extra play. Moving the whole shell changes the gap and not the travel, because the cone and the lip move together. That play is in the stack below.

Nominal gap (seated stem to the actuator) is 0.63 mm. Nominal travel (cone on the hole, to the catch on the lip) is 1.50 mm. Nominal plastic stroke is 0.87 mm. The switch spec table travels 0.25 mm, and the drawing says 0.2 ± 0.1 mm. The cover is the stop a finger feels. The lip is 0.62 mm past the spec-table bottom-out (1.50 − 0.63 − 0.25). At the drawing's 0.3 mm maximum travel the lip is 0.57 mm past bottom-out. The switch sheet does not give an over-travel rating, so this is a plastic stop beyond the stated travel, not a rated over-travel. The pressed mesh is past the cover: mashing past the click loads the cover until the lip. The fit script allows the stem to pass the actuator in that pose.

The tolerance check is a uniform Monte Carlo, N = 200000, seed 1. Six terms: printer z ±0.10 mm on the gap, an independent ±0.10 mm on the travel (the cap and the shell are separate prints), standoff ±0.15 mm, board ±0.16 mm, switch height ±0.05 mm, and snap-seat play ±0.10 mm. It reports P(gap ≤ 0) = 0%, P(stroke < 0.20 mm) = 0%, worst-case gap 0.07 mm, worst-case stroke 0.21 mm. The script fails if either probability is over 1% / 2% or the worst stroke is under 0.20 mm. A normal model that drops the board term and treats the remaining ± as 1σ is printed beside it and is not the gate.

In the finished cap the leaf is a cantilever. It is anchored at the rib, 0.40 mm thick, free length 4.35 mm, and it is not a bridge between two walls. Print the caps head-down, face on the bed. The cone and the barb underside are 65° from horizontal. The root rib is 72°, so a 60° threshold does not support it. In the print frame the cone's wide end is at z = 3.87. The tie stands 0.34 mm above that face and 0.06 mm into the leaf, so the leaf bottom bridges from the root rib onto the nub. One corner of the tie also meets the bed outside the Ø4.70 head. PrusaSlicer 2.9.2, with the settings in the carrier paragraph, extrudes no support on the three caps at overhang thresholds 30°, 40°, 50°, and 60°. Snap the nub off before the cap goes into the tube. The same slicer on the face-down shell extrudes 1.54 m of support filament at 30° (support plus the interface), 1.91 m at 45°, and 1.86 m at 60°, including towers in the button bores. Support material alone, interface not added, is 1.40 m, 1.77 m, and 1.72 m. The carrier paragraph is the same split. Those come out the open back before the caps are installed. The 3MF has supports off; the lengths above are a separate slicer run with supports turned on.

## Build order

1. Screw the backplate to the wall. Four drywall-anchor screws, pan heads in the recesses at the four anchor points. The board is not on the plate yet, so the heads are reachable. The lip, the snap bumps, the ribs, and the standoffs face out of the wall.

2. Pull the cable through the centred 25 × 20 mm hole. Leave enough slack to mate the plug in the hand.

3. On the bench, board component-side up, panel held up off the carrier. Fold the tail 180° under the panel, contacts down, and slide it into J4 from the right until it stops. Push the slider shut (−x) with the flat pick. The 0.85 mm tab is the part that moves, and open would be the other way, +x.

4. Set the carrier on the board. Pull the support towers off the four hook shelves first. The left fence and the bottom fence drop onto the board edges; the pads sit on the soldermask. Press the panel down onto the four pads until the ledges click. Each hook is 0.41 N. The panel back is then 4.6 mm above the PCB. This is the subassembly: board, carrier, panel, tail.

5. Mate the 15EDGK plug onto J1 while the board is in the hand. The plug faces the wire hole.

6. Set the subassembly onto the four locating pins. Feed the wire loop back through the hole as the board goes down. The pins stop the board in x and y. The plug and C43 are in the space the standoffs leave above the plate.

7. Snap the three button caps into the shell from the inside, before the shell goes near the board. Snap the tie off each cap first. Lay the shell face-down, push each head through its hole, and press until the cone seats and the barb clicks behind the lip. The barb bends 0.30 mm to pass. The caps stay in the shell from here on. They can rattle between the cone and the lip. Lower the shell over the stack. It slides onto the backplate lip. The clamp posts land around the four mounting holes and press the board onto the standoffs. The stems come down onto the three actuators with the 0.63 mm gap. Four bumps on the lip click into the snap windows.

The light pipe, the RESET pinhole, the window, and the vents are in the shell. The three airflow zones are the hot zone on the left (triacs and the opto drive), the quiet middle over the wire hole (fill the wall opening with putty), and the sensor duct on the right. The baffle around the SHT40 island is open to the right wall. The edge vents are about 28% open area, on all four edges, with the triac heat on the far side of the curtain from the sensor.

## Taking it apart

Pry the shell at the snap windows. A small flat tool in one window on the bottom edge, then the other bottom window, then the top edge, flexes the 2.4 mm wall out over the 0.9 mm bump. The bumps are 45° on both sides, so the wall can climb off them. Lift the shell straight off. The button caps stay in it. To take one out, press the single barb toward the stem and push the cap out the back of the hole.

Lift the subassembly off the pins. Unplug the HVAC plug in the hand.

To release the panel, the shell is off and the glass is seated. Open the J4 slider first. The pick in `case-section-j4.png` reaches the mouth through the 10.15 mm gap, leaning 66° off +x, and the whole tool then moves along +x by 0.85 mm while the glass stays down. That opens the slider. Shut would be −x. With the lock shut, a sideways slide shears the tail, and a lift of 0.07 mm on the 3.11 mm row pulls the insertion under 3.0 mm (1.57 mm of tail per millimetre of lift). The release takes two hands. One hand holds a single rigid 1.0 mm strip in the windows of one edge, and that strip reaches both beams of the edge: about 33.4–42.0 mm on the top edge, or 22.2–30.7 mm on the bottom. The other hand slides the glass. Hold both beams out by (glass width − 36.06 mm) + 0.05 mm. On a 36.70 mm glass that is 0.69 mm and 0.88 N on each hook, 1.76 N for the pair. At 36.90 mm it is 0.89 mm, 1.92% strain, about 35 MPa across the layers, and 1.13 N per hook. A lip that prints 0.10 mm long on each edge makes that hold-out 1.04 mm, 2.25%, and 1.32 N, which is over the 2% gate. The top pair is 37.6 mm apart and the bottom pair is 26.4 mm. Slide the glass toward that edge until both shelves are clear. From the middle of the side play that is 0.345 mm; if the glass is already against one beam it is 0.10–0.60 mm. Either way it leaves 0.025 mm of air on each shelf once the glass is in the opened gap. With the slider open, a nominal tail has 0.10 mm of play in the 12.60 mm guide, which covers the small end of that slide and not the 0.60 mm end. Then lift the glass and draw the tail out of the open connector. The shelf underside is the glass front, so the glass stays down until the shelves are clear.

## Where J4 sits

J4 is the footprint in `kicad/`: silk mouth at board x = 103.77, 4.69 mm inboard of the glass edge, slider under the panel. Assembly mates the tail with the panel held up and pushes the slider shut (−x). Service opens the slider (+x) with the leaning pick while the glass is held down and the shell is off, then releases the hooks, then lifts the glass and draws the tail. The connector stays on the footprint. A mouth to the right of x = 108.46 would put the slider in the open, and it would also change the straight run and the inserted length.
