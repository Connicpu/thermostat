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

J4 is a Jushuo AFC07-S24FCA-00 (LCSC C262567). It is a right-angle, 2.0 mm high, bottom-contact FPC connector with a horizontal slide lock, the "drawer" style. The drawing title says "0.5Pitch H=2.0 FPC ZIF R/A Type3 Lower SMT CONN". The mechanism view is a slider, not a flip latch. On section A-A the 0.85 mm callout is the rear solder-tab foot, and it scales with the 0.50 mm pitch on the same sheet. The 1.40 mm callout next to it is the fitting nail. The length table fills DIM E as 5.20 mm and DIM F as 6.45 mm. E minus the 3.90 mm in the DIM B cell is 1.30 mm. The wording on that DIM B cell does not establish 3.90 mm as the body depth, and that cell is not a stroke. The drawing states no slider stroke. The STEP model leaves 0.85 mm of room behind the bar, and that room bounds neither the pull-out nor the closing travel. The stroke used here is an assumption, from 0.85 mm to 1.30 mm. Both ends sit inside the free pull measured below. The mouth faces +x, so open is +x and shut is −x. The housing solid in the fit runs 3.90 mm back from the STEP front face at board x = 102.675. For 24 contacts the drawing's four length columns are 11.50, 12.60, 16.50, and 17.60 mm. The side view also shows 2.70, 2.20, 0.50 ± 0.05, 0.30 ± 0.03, and R0.20. R0.20 is a housing fillet. The footprint's silk front face is at board x = 103.77, and the mouth faces +x, so the tail enters from the right. The recommended FPC thickness on the drawing is 0.30 ± 0.03 mm, which matches the panel tail. The same drawing recommends a contact length of 3.0 mm minimum.

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

The slider is shut before the glass comes down for assembly. Shut is −x. Open is +x. The stroke is the assumed range, 0.85 mm to 1.30 mm. The body is 2.0 mm high and the glass back is at 4.6 mm, so 2.6 mm remains above the housing with the panel seated. The housing top is at z = 20.6. The fold wall sits at board x ≈ 110 for y 37.75–50.25, and the carrier posts at y 33.20–34.30 and y 60.40–61.70 run out to x = 109.26 under the glass. The openings left on the +x side of the fold are 10.15 mm (y 50.25–60.40) and 3.45 mm (y 34.30–37.75).

The tool is one piece of 0.45 mm feeler-gauge stock, 0.80 mm wide. The shank is that strip, from z 20.75 to 21.20. It runs 9 mm from the ear to an elbow and leans 50° off +x. The head is the bent end, filed to 0.34 mm long in x and 0.56 mm wide in y, and it stays above the housing, from z 20.62 to 21.06. Under the head a tongue is filed to 0.08 mm in x, over y 52.25–52.80 and z 20.35–20.60, so it fits the 0.10 mm slot in front of the housing and leaves 0.02 mm of air. In the seated pose the tongue's +x face is on the upper ear's −x face, at board x = 102.775, y = 52.525. The housing's +x face is at x = 102.675. The block rear and both end ears are the −x faces at that same x; the upper ear runs y 51.8–53.0, and the tongue covers y 52.25–52.80 of it. The elbow is at (108.56, 59.42). The handle is the next 5.0 mm of the same strip, along +x, ending at x = 113.56, and it is held in the fingers. A push on the shank closes the slider. Opening it is a pull on the tongue's +x face, which bears on the ear's −x face.

Service has the shell off and the glass seated. Hold the hook 0.60 mm above the seated pose and rotate it about the elbow from −28° to the 50° lean. Then lower it 0.60 mm. The tongue drops into the slot, on the −x side of the ear, and its +x face meets the ear. Pull +x across the assumed stroke, from 0.85 mm through 1.30 mm, with that face still on the ear. Sliding the seated hook straight along +x, out past the stroke, runs the handle into J2. The rotation is how the hook gets into the slot.

`case-section-j4.png` is the seated hook seen from above, shell off, glass seated. The crimson part is the feeler strip, tongue on the upper ear, handle stopping before J2. The blue frame is the glass outline. `scripts/case_fit.py` sweeps the raised rotation, the drop, and the pull at 0, 0.85, 1.075, and 1.30 mm. It fails if the carrier, the fold, the glass, or J2 comes within 0.20 mm, if the housing is penetrated, or if the tongue's +x face leaves the ear's −x face. On the current mesh that sweep is 0.58 mm from the carrier, 4.68 mm from the fold, 1.40 mm from the glass, 1.72 mm from J2, and 0.02 mm from the housing front. The housing solid stops at x = 102.675, and the 0.02 mm is the air between that face and the 0.08 mm tongue. The same sweep stays 1.74 mm off the installed shell. The shell is off in service, and that distance is printed and is not a gate. A 55° lean comes within 0.10 mm of the carrier. A 90° lean enters the carrier, clearance −0.65 mm. The fit fails if either of those leans stays 0.20 mm clear. The straight +x continuation of the seated hook meets J2 at −1.11 mm. The free +x run before a neighbour comes within 0.20 mm is 2.82 mm. The assumed stroke, 0.85 mm to 1.30 mm, fits in that run. The fit fails if 1.30 mm is longer than the run, and a probe 0.40 mm past the limit is at −0.20 mm. The carrier opening the hook uses is 10.15 mm and the hook needs 9.77 mm of it, so a 6.35 mm opening fails. With the lock shut, each millimetre of lift pulls 1.57 mm of tail out, and the 3.11 mm tall-print insertion has 0.07 mm of lift before it is under 3.0 mm. A sideways slide with the lock shut shears the tail. Service opens the slider before the glass moves. On the bench the panel is held up, the tail is slid in from the right, and the shank pushes the slider shut, which is −x. The EasyEDA model on the footprint is the same part at the same silk line, and it is not a measured copy of the drawing.

With the slider open the tail can shift sideways in the connector. The drawing's applicable-width column is 12.60 ± 0.10 mm and the tail is 12.50 ± 0.10 mm. The play is 0.10 mm at the nominal widths, 0.20 mm on a narrow tail in a nominal guide, and none on a wide tail in a nominal guide. The two tolerances together give 0.30 mm at the loose extreme and −0.10 mm at the tight extreme. A negative play means that tail does not enter that guide. The release does not slide sideways, so the sideways travel is 0, and that fits every non-negative play, including the wide tail's 0.00 mm. The −0.10 mm pair is a parts mismatch. That stack is printed and is not a gate, because a wide tail in a narrow guide does not assemble. `case_fit.py` reads the tail width from the fold geometry and fails if the modeled tail is wider than the guide, or if the sideways travel exceeds the smallest non-negative play.

## Why the panel is on a carrier

The mouth is under the panel. With the shell on, the shell wall and the button tubes cover the slider. With the shell off, the hook reaches the mouth while the glass is seated. The window is smaller than the panel, so the glass cannot drop in through the front. Assembly mates J4 with the panel in the hand, then presses the glass onto the hooks.

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

`scripts/case_fit.py` recomputes both and fails if the strain reaches 2.4% or the force reaches 8 N. It also probes the mesh: the beam is solid, the 0.25 mm gap beside it is air, and the foot is solid. Holding one hook out 0.32 mm takes that same 0.41 N. The lead-in is the top face of the lip. The underside of the shelf is the glass front, so a tool has no gap under the ledge.

The relaxed shelf tips are 36.06 mm apart: the 36.70 mm outline minus a 0.32 mm ledge on each edge. The beams stand 0.25 mm outside the outline, so the inner faces are 37.20 mm apart. The datasheet gives the outline as 36.7 mm only. Glass in the check is 36.50, 36.70, and 36.90 mm wide, and that band is an assumption around the one drawing number. Each of those still fits between the beams, with 0.70, 0.50, and 0.30 mm to spare. Release holds both long edges out and lifts the glass straight up. Sideways travel is 0. Shoved against one beam, the near lip still covers 0.57 mm, the 0.32 mm ledge plus the 0.25 mm gap. The hold-out is 0.595 mm, which leaves 0.025 mm of air on that shelf. The strain is 1.29% and the force is 0.76 N per hook. E times that strain is 23 MPa. Ultimaker's PETG datasheet lists tensile strength in Z as 19 ± 6.4 MPa at 1.8% strain and flexural strength as 50 MPa at 3.6% strain. The beam bends about the board's x axis. The outer fiber is in tension along z, across the print layers, so the limit in the fit is the flexural strain divided by 1.5, which is 2.4%. That 2.4% sits above the PETG interlayer elongation at break, 1.8 ± 0.8%. The 1.29% hold is under that 1.8% mean and above 1.0%. If the near lip prints 0.10 mm long, the hold-out grows to 0.695 mm, which is 1.50%, 0.88 N, and 27 MPa, and that row passes the 2.4% gate. The 1.50% row is also under the 1.8% mean and above 1.0%. `case_fit.py` prints it. The script fails if the strain reaches 2.4% or the force reaches 8 N, or if the near shelf has under 0.02 mm of air, for each of those three widths.

Each hook has a head outboard of the beam. A window 4.0 mm along the hook and 1.30 mm across, open at the top, sits between the beam and an 0.80 mm wall. The wall's inboard face faces the glass. A 1.0 mm blade in the window bears on that face and pushes the beam outward. The underside of the head is 50° from horizontal. The root of the beam stays the full 0.42 mm, which is the section in the strain formula. The top pair is 37.6 mm apart (centers at x 50.0 and 87.6). The bottom pair is 26.4 mm apart (centers at x 52.6 and 79.0). `case-section-release.png` is the cut through the first top window, at board x = 50, shell off, with the blade against that face. `case-section-release-out.png` is the same cut with the top edge held out 0.595 mm. The glass stays in place in y. The other edge is held out the same way; this cut shows the top one. The glass is blue in that view and the pads are a darker tan, so the two do not read as one mass. The glass then lifts straight up.

The ledges stand proud of the glass, so the shell has a pocket over each hook, 1.10 mm deep. The rest of the inner face is recessed 0.70 mm over the panel. A thick board (+0.16 mm), a carrier that prints 0.10 mm tall, a shell sitting 0.10 mm low in the snap window, and a standoff 0.15 mm tall all lift the glass toward the face, 0.51 mm together. The recess leaves 0.19 mm past that stack. It stops at board x = 107.55. The button tubes are Ø8.6 at x = 112, so they occupy the last of the outline, and cutting the recess through them would leave the tube walls loose. The window chamfer starts 0.85 mm behind the inner face, so the recess stays in front of it.

Under each hook a pad rises from the PCB to the panel back. The right-hand top pad also bridges across to the column at x 76–82, under the glass edge and past the ESP32 module (the module ends at y = 25.49, the pad starts at y = 25.99). Nothing in the carrier crosses the optos, L1, or the FPC bulge. The path between the top hooks and the bottom rail jogs east around C42, narrows to 0.85 mm to pass between the two C43 leads (x = 51.50 and x = 55.00), and narrows again to pass to the right of J1 pin 5 (x = 77.00).

The glass can also slide in x, and +x lengthens the straight run into J4 by the same amount. The 3.11 mm tall-print insertion has 0.11 mm before it is under 3.0 mm. Two posts on the cable side of the outline stop that slide. They bite 0.05 mm onto the glass edge, at board y 33.20–34.30 and y 60.40–61.70, clear of the button tubes and of the bend bulge. Each post's footprint continues down to the carrier bottom, so the slicer does not tower under it. A carrier that prints 0.10 mm short of the glass then leaves 0.05 mm of slide, and the tall-print insertion is 3.06 mm, above 3.0 mm. With the glass against the stop that insertion is the 3.11 mm row. The other way, −x, runs into the front curtain 0.46 mm away, and that direction shortens the straight run, so the tail goes further in rather than out.

The piece locates on two edges. A fence on the left, 0.25 mm off the board edge, and a fence on the bottom edge do the same job. Short ties join each fence to the rail on the board so a 0.4 mm nozzle does not weld the fence to the rail and eat the clearance. The bottom rail is the spine that makes the hooks, the fences, and the columns one solid. At the two bottom hooks the rail is only foot-height (0.40 mm) where it meets the beam, and the tall rail stops inboard of the glass edge, so the beam stays free.

That rail crosses the front curtain, which is the wall between the hot zone and the panel at board x 27.7–29.0. The curtain floor is 1.1 mm above the PCB. For 1 mm of y at the board's bottom edge the curtain is cut up to 1.55 mm, and the rail through that slot is 1.00 mm tall. The remaining crack is about 0.55 mm high (about 0.39 mm if the board is thick) and well under 1 mm², smaller than the ~1.3 mm² leak where the rear rib and the front partition do not line up. The rail plugs the rest of the notch, so the curtain separates the zones.

In the print the hook shelf is a horizontal lip 0.57 mm deep (the 0.25 mm air gap plus the 0.32 mm ledge). The lead-in above it is 45°. The lip is a cantilever, not a bridge. PrusaSlicer 2.9.2 (gneiss15's build of 2.9.2), 0.20 mm layers, a 0.4 mm nozzle, supports enabled, grid style, sheath on, three interface layers, 2.5 mm support spacing, 50% XY separation, 0.2 mm contact distance, and "Don't support bridges" on, builds a tower under each of the four shelves. At a 30° overhang threshold, with those three interface layers, the carrier's support extrusion is 488 mm of filament. At 45° it is 555 mm, and at 60° it is 562 mm; those two thresholds also leave a small contact beside one head. Those three lengths are extruded filament for the support and the three interface layers together, not a toolpath centreline. The same slice with the interface layers off is 428 mm at 30°. Pull the towers off before the glass goes on. The carrier in `thermostat-case-P2S.3mf` has supports off (`enable_support` is 0). The lengths above are a separate slicer run with supports turned on.

## Buttons

UP, DOWN, and MODE are at board (112, 30), (112, 40), and (112, 50). The pitch is 10 mm and the guide tubes are Ø8.6, so the front partition overlaps the tube wall and the recess in the face stops at x = 107.55.

The switch is a TS-1187A-B-A-B. Height code A is 1.5 ± 0.05 mm, which is how tall the actuator stands, not how far it moves. The drawing calls the travel 0.2 ± 0.1 mm and the spec table says 0.25 mm. Force code B is 160 gf. The actuator is Ø2.0 and the body is 5.1 mm square, with the cover at 1.2 mm. The stem is Ø1.6, so it meets the actuator and not the cover.

Each cap is captive in the face. A Ø4.70 head passes through the Ø5.0 hole with 0.15 mm to spare. Behind the hole a cone opens to Ø6.0 at 65° from horizontal and meets the inner corner of the hole, so the cap cannot leave through the front. One leaf on the partition side of the stem carries the catch. The catch sits behind a lip in the tube (the lip's hole is Ø6.3, the bore around it is Ø7.7). The FPC pocket removes the panel side of each tube, which is why the leaf faces the partition, where the lip is present. Two leaves of this length do not fit in that bore on the partition side.

The leaf is a straight vertical wall, 0.40 mm thick and 4.50 mm long. A rib clamps the first 0.15 mm, so the free length is 4.35 mm. The barb at the free end stands 0.30 mm past the lip and has to bend that far in x to pass. With the rectangular-beam formula ε = 1.5 t δ / L²,

ε = 1.5 × 0.40 × 0.30 / 4.35² = 0.95%

`scripts/case_fit.py` recomputes that and fails if the free length is under 4 mm or the strain reaches 2.4%, the same flexural limit as the hooks. The pressed-catch check is the barb face against the lip, and that face kisses the lip (clearance 0). The seated cone kisses the hole corner.

There is no second spring. A leaf that stayed on the lip through the whole 1.50 mm of travel would be several times this strain. The cap can move the full 1.50 mm between the cone and the lip. The switch's own 160 gf is what pushes it back out, and only after the stem has crossed the gap. Off the board, the cap rattles. It cannot fall out of the front, and it cannot fall out of the back.

The shell is fixed to the backplate. The clamp posts are cylinders that start at z = Z_pcb_t + 0.1, 0.1 mm of air above a nominal board, so they do not make the shell follow the board. Board thickness stays in the stack. The snap windows run z 2.2–4.2 and the bumps are about 2.3–4.1, so a shell that is not seated in the middle of the window has about ±0.10 mm of extra play. Moving the whole shell changes the gap and not the travel, because the cone and the lip move together. That play is in the stack below.

Nominal gap (seated stem to the actuator) is 0.63 mm. Nominal travel (cone on the hole, to the catch on the lip) is 1.50 mm. Nominal plastic stroke is 0.87 mm. The switch spec table travels 0.25 mm, and the drawing says 0.2 ± 0.1 mm. The cover is the stop a finger feels. The lip is 0.62 mm past the spec-table bottom-out (1.50 − 0.63 − 0.25). At the drawing's 0.3 mm maximum travel the lip is 0.57 mm past bottom-out. The switch sheet does not give an over-travel rating, so this is a plastic stop beyond the stated travel, not a rated over-travel. The pressed mesh is past the cover: mashing past the click loads the cover until the lip. The fit script allows the stem to pass the actuator in that pose.

The tolerance check is a uniform Monte Carlo, N = 200000, seed 1. Six terms: printer z ±0.10 mm on the gap, an independent ±0.10 mm on the travel (the cap and the shell are separate prints), standoff ±0.15 mm, board ±0.16 mm, switch height ±0.05 mm, and snap-seat play ±0.10 mm. It reports P(gap ≤ 0) = 0%, P(stroke < 0.20 mm) = 0%, worst-case gap 0.07 mm, worst-case stroke 0.21 mm. The script fails if either probability is over 1% / 2% or the worst stroke is under 0.20 mm. A normal model that drops the board term and treats the remaining ± as 1σ is printed beside it and is not the gate.

In the finished cap the leaf is a cantilever. It is anchored at the rib, 0.40 mm thick, free length 4.35 mm, and it is not a bridge between two walls. Print the caps head-down, face on the bed. The cone and the barb underside are 65° from horizontal. The root rib is 72°, so a 60° threshold does not support it. In the print frame the cone's wide end is at z = 3.87. The tie stands 0.34 mm above that face and 0.06 mm into the leaf, so the leaf bottom bridges from the root rib onto the nub. One corner of the tie also meets the bed outside the Ø4.70 head. PrusaSlicer 2.9.2, with the settings in the carrier paragraph, extrudes no support on the three caps at overhang thresholds 30°, 40°, 50°, and 60°. Snap the nub off before the cap goes into the tube. The same slicer on the face-down shell, with the three interface layers, extrudes 1.40 m of support filament at 30°, 1.77 m at 45°, and 1.72 m at 60°, including towers in the button bores. The carrier paragraph uses the same settings. Those come out the open back before the caps are installed. The 3MF has supports off; the lengths above are a separate slicer run with supports turned on.

## Build order

1. Screw the backplate to the wall. Four drywall-anchor screws, pan heads in the recesses at the four anchor points. The board is not on the plate yet, so the heads are reachable. The lip, the snap bumps, the ribs, and the standoffs face out of the wall.

2. Pull the cable through the centred 25 × 20 mm hole. Leave enough slack to mate the plug in the hand.

3. On the bench, board component-side up, panel held up off the carrier. Fold the tail 180° under the panel, contacts down, and slide it into J4 from the right until it stops. Push the slider shut (−x) with the shank of the hook. The part that moves travels the assumed stroke, 0.85 mm to 1.30 mm. Open is a pull on the upper ear's −x face, at x = 102.775.

4. Set the carrier on the board. Pull the support towers off the four hook shelves first. The left fence and the bottom fence drop onto the board edges; the pads sit on the soldermask. Press the panel down onto the four pads until the ledges click. Each hook is 0.41 N. The panel back is then 4.6 mm above the PCB. This is the subassembly: board, carrier, panel, tail.

5. Mate the 15EDGK plug onto J1 while the board is in the hand. The plug faces the wire hole.

6. Set the subassembly onto the four locating pins. Feed the wire loop back through the hole as the board goes down. The pins stop the board in x and y. The plug and C43 are in the space the standoffs leave above the plate.

7. Snap the three button caps into the shell from the inside, before the shell goes near the board. Snap the tie off each cap first. Lay the shell face-down, push each head through its hole, and press until the cone seats and the barb clicks behind the lip. The barb bends 0.30 mm to pass. The caps stay in the shell from here on. They can rattle between the cone and the lip. Lower the shell over the stack. It slides onto the backplate lip. The clamp posts land around the four mounting holes and press the board onto the standoffs. The stems come down onto the three actuators with the 0.63 mm gap. Four bumps on the lip click into the snap windows.

The light pipe, the RESET pinhole, the window, and the vents are in the shell. The three airflow zones are the hot zone on the left (triacs and the opto drive), the quiet middle over the wire hole (fill the wall opening with putty), and the sensor duct on the right. The baffle around the SHT40 island is open to the right wall. The edge vents are about 28% open area, on all four edges, with the triac heat on the far side of the curtain from the sensor.

## Taking it apart

Pry the shell at the snap windows. A small flat tool in one window on the bottom edge, then the other bottom window, then the top edge, flexes the 2.4 mm wall out over the 0.9 mm bump. The bumps are 45° on both sides, so the wall can climb off them. Lift the shell straight off. The button caps stay in it. To take one out, press the single barb toward the stem and push the cap out the back of the hole.

Lift the subassembly off the pins. Unplug the HVAC plug in the hand.

To release the panel, the shell is off and the glass is seated. Open the J4 slider first. Hold the hook in `case-section-j4.png` 0.60 mm high and rotate it about the elbow from −28° to the 50° lean, so the tongue swings in above the housing. Lower it into the slot on the upper ear, y 52.25–52.80, with the tongue's +x face on the ear's −x face at x = 102.775. The housing front is at x = 102.675. Pull +x across 0.85 mm to 1.30 mm. That opens the slider. The handle is the 5.0 mm of the same strip past the elbow, held in the fingers. Shut is a push, −x. With the lock shut, a sideways slide shears the tail, and a lift of 0.07 mm on the 3.11 mm row pulls the insertion under 3.0 mm (1.57 mm of tail per millimetre of lift). Then release the glass. One fixture holds a 1.0 mm strip in the windows of each long edge at the same time. Separate strips need a hand on each edge and another on the glass. Each strip reaches both beams of its edge: about 33.4–42.0 mm on the top edge, or 22.2–30.7 mm on the bottom. Hold every beam out 0.595 mm. That is 1.29% strain, 23 MPa, and 0.76 N per hook, and it leaves 0.025 mm of air on the near shelf when the glass is against that beam. A lip that prints 0.10 mm long makes the hold-out 0.695 mm, 1.50%, and 0.88 N, which passes the 2.4% gate. The top pair is 37.6 mm apart and the bottom pair is 26.4 mm. There is no sideways slide. The sideways play in the 12.60 mm guide is 0.10 mm at a nominal tail, 0.20 mm on a narrow tail, none on a wide tail, and −0.10 mm at the tight stack of the ±0.10 mm tolerances. A 0 mm slide fits the non-negative plays. The −0.10 mm pair does not enter the guide. Lift the glass straight up and draw the tail out of the open connector. The shelf underside is the glass front, so the glass stays down until both shelves are clear.

## Where J4 sits

J4 is the footprint in `kicad/`: silk mouth at board x = 103.77, 4.69 mm inboard of the glass edge, slider under the panel. The housing front on the STEP is at x = 102.675, and the ear's −x face is at x = 102.775. Assembly mates the tail with the panel held up and pushes the slider shut (−x). Service, shell off and glass seated, hooks that ear and pulls the slider open (+x) across the assumed 0.85 mm to 1.30 mm, then holds both long edges out and lifts the glass, then draws the tail. The connector stays on the footprint. A mouth to the right of x = 108.46 would put the slider in the open, and it would also change the straight run and the inserted length.