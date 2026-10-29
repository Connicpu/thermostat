# Retainer concepts for review

These are concepts. The case geometry on `cursor/case-r5-408c` is unchanged. Nothing here is a claim that the case already works this way.

Shared rules for every concept:

- PETG only. Fingers, or fingers and standard screws.
- The display panel rests on the carrier pads. The front shell clips only to the backplate. The shell is air around the clip and is not in the load path.
- Panel thickness is the datasheet value, 1.2 ± 0.1 mm. A 1.1 mm panel still has a positive deflection.
- E = 1800 N/mm². In-plane flexural gate 2.4%. Interlayer bending gate 1.0%.
- Tongue formula, root treated as a rigid clamp: I = w t³ / 12, k = 3 E I / a³, F = k δ, ε = 1.5 t δ / a². δ is 0.10 / 0.20 / 0.30 mm at panel thickness 1.1 / 1.2 / 1.3 when the nominal interference is 0.20 mm.
- Half the force is what remains after 50% relaxation. A 5 g shock on the 5.5 g panel is 0.27 N, so the relaxed sum has to stay above that at 1.1 mm.
- Install and removal of a clip happen with the shell already off. While the shell is on, no clip needs the shell as a stop.

Sketches, dimensioned, with print direction and the load path:

- `docs/case-r6/concept-a-square-hook.png`
- `docs/case-r6/concept-b-undercut.png`
- `docs/case-r6/concept-c-screw-bracket.png`

## Concept A — square hook, root on the fence

Section: `concept-a-square-hook.png`. Slice prototype: `case/scratch/r6_square_hook_proto.scad` (one clip, not wired into the case).

Retention. The rib underside is a flat horizontal face. The tooth top is the same flat, at the same z, so the free rise is 0.00 mm. The two faces overlap in y by 0.35 mm. A 45° chamfer exists only on the install side: the rib's top-outer corner and the tooth's bottom-inner corner. The retention pair has no chamfer, so the reaction is vertical. Cam-out force is zero, and the hold does not depend on friction. The top clip's 0.56 mm of outboard room and the bottom clip's 0.22 mm of outboard room stay empty air. The shell is not the backstop.

The tooth occupies the last 1.6 mm of the arm. On the 6.0 mm fence the anchor is 1.5 mm, the free arm is 4.3 mm, and the tooth centre is 3.5 mm from the anchor face. The other three fences are longer, so their arms are softer. To pass the rib the free end moves 0.35 mm in y.

Arm, in-plane (thickness in y, layers in z, tooth on the bed):

- t = 0.48 mm, h = 1.20 mm, L = 3.50 mm, δ = 0.35 mm
- ε = 1.5 × 0.48 × 0.35 / 3.50² = 2.06%
- F ≈ 0.49 N at the fingertip

That is under the 2.4% flexural gate. It is a one-time bend during install and removal, with the shell off.

Preload path. The root block, 0.80 mm thick, sits on the fence top with a 0.00 mm gap. The bump force is carried as compression into the fence and as a hook force into the rib. The thin vertical column is gone, so it is not in series with the tongue and it does not bend across layers. Interlayer bending strain in the anchor is 0. The tongue bends about x with the fibre stress along y, in the layer plane.

Tongue, t = 0.40 mm, a = 2.95 mm to the bump centre. The bump is 0.40 mm long in y, so its root-side edge is at 2.75 mm. Three clips are 4.00 mm wide. The 6 mm fence clip is 2.20 mm wide.

k (4.00 mm) = 3 × 1800 × (4 × 0.40³ / 12) / 2.95³ = 4.49 N/mm.
k (2.20 mm) = 2.47 N/mm.

| Panel | δ | In-plane strain at a = 2.95 | Force, 4.00 mm | Force, 2.20 mm |
|---|---|---|---|---|
| 1.1 | 0.10 | 0.69% | 0.45 N | 0.25 N |
| 1.2 | 0.20 | 1.38% | 0.90 N | 0.49 N |
| 1.3 | 0.30 | 2.07% | 1.35 N | 0.74 N |

At the bump's root-side edge (a = 2.75 mm) and δ = 0.30 mm the strain is 2.38%, still under 2.4%. A longer bump would push that edge past the gate, so the bump stays 0.40 mm long.

Four clips after 50% relaxation: 0.80 N at 1.1 mm, 1.59 N at 1.2 mm, 2.39 N at 1.3 mm. The 1.1 mm case is above 0.27 N. If a 0.02 mm seating gap closes before the tongue bends, the 1.1 mm deflection becomes 0.08 mm and the relaxed sum is 0.64 N, still above 0.27 N.

The nominal tip is 3.00 mm onto the border. The gate for the later build has to use the worst panel width (36.50–36.90 in the 37.10 opening) and the clip's y play, and keep both the tip and the bump at least 0.50 mm outside the active area. The bump's edge-side corner stays past the 1.52 mm line Ferdinand measured, including when the panel is shifted away from that fence.

Fingers. Shell off. Press the clip down. The lead-ins spring the free end out by 0.35 mm and the square faces meet. To remove it, pull that same free end out 0.35 mm and lift. The anchor does not have to flex.

Tilt. With 0.00 mm of z gap on a square face, a rigid rise of the bump tightens the hook and the root bearing. Both stops are on the carrier. The later tilt gate has to report the bump's rise, and fail if the shell is reached while the bump is still moving.

Print. The arm bend stays in-plane, so the layers stack along case z. Two 0.80 mm pillars can hold a tongue that is up in the air, with a 0.50 mm neck at the break, and the slice below shows those pillars survive. The 0.20 mm bump does not: in that pose it is one extrusion point. The preload face therefore goes on the bed (the clip flipped so the bump is down, and the tooth is the upper feature). The bump stays off any pillar.

## Concept B — 8° undercut

Section: `concept-b-undercut.png`.

Same tongue, same root bearing, same bump, same preload table as A. The retention face tilts 8° so the outer lip is the low point. The hook reaction gains an inboard component, toward the fence and away from the shell. Free z is still 0.00 mm. Lead-ins stay on the install side only.

The extra overlap is about 0.40 × tan 8° = 0.06 mm, so the free end moves 0.41 mm to pass. On the 6 mm span the arm is 0.45 mm thick in y and the in-plane install strain is 2.26% at L = 3.50 mm. A 0.48 mm arm at that deflection is 2.41%, over the gate, so B uses 0.45 mm there.

Fingers, same sequence as A, with a slightly harder squeeze. Print is the same tooth-down orientation. The undercut on the carrier is a horizontal lip and prints feet-down without support.

## Concept C — screw-stopped leaf

Section: `concept-c-screw-bracket.png`.

An M2 screw and a trapped square nut in the carrier. The screw is the retention. There is no ramp and no friction requirement. The head bottoms on a printed shoulder when the leaf has deflected 0.25 mm against a 1.2 mm panel. Further turns do not add deflection. Panel thickness moves that deflection by the datasheet ±0.1 mm.

Leaf: t = 0.55 mm, w = 8 mm, L = 5.5 mm, bend in the layer plane (the leaf prints flat). k = 3.60 N/mm. The boss and the nut are in compression. Interlayer bending strain is 0.

| Panel | δ | In-plane strain | Force per bracket |
|---|---|---|---|
| 1.1 | 0.15 | 0.41% | 0.54 N |
| 1.2 | 0.25 | 0.68% | 0.90 N |
| 1.3 | 0.35 | 0.96% | 1.26 N |

After 50% relaxation, four brackets sum to 1.08 N at 1.1 mm. Two brackets sum to 0.54 N. Four brackets, one at each fence, keep the panel from rocking.

Fingers plus a screwdriver. Shell off. Drop the bracket on, run the screw in until the head stops. Removal is unscrewing. The shell stays ≥ 0.30 mm off the bracket, in the face recess, and is not in the load path.

The leaf has no pillars. Its thickness, which sets the preload, is a bed face.

## Slice of concept A

GitHub's PrusaSlicer 2.9.2 release ships Windows and macOS only. The slice here is PrusaSlicer 2.8.1, the newest Linux AppImage on that project's releases (`PrusaSlicer-2.8.1+linux-x64-newer-distros-GTK3`). Settings follow the case 3MF: 0.40 mm nozzle, 0.20 mm layers, Generic PETG, Arachne, supports off unless noted.

Plate: `case/scratch/r6_square_hook_proto.stl`, generated from the scratch scad. The clip has two 0.80 mm pillars. A 0.70 × 0.60 × 4.2 mm tower sits beside it.

Supports off. The log says `Detected print stability issues`, then `Low bed adhesion, Loose extrusions`, and suggests supports and a brim. Filament used is 10.25 mm. Support length is 0.

- The 0.70 × 0.60 × 4.2 mm tower contributes one first-layer point and no extrusion above z = 0.2. It does not print. That matches the 2.9.2 result that this cross-section is an empty object.
- Each 0.80 mm pillar has extrusion through the shaft and through the 0.50 mm neck.
- The 0.20 mm bump, hanging under the raised plate, is a single extrusion point. A preload pad of that size is not a dependable layer in this orientation.

Supports on, overhang threshold 45°. Filament used is 31.55 mm, so the support length is 21.3 mm. The 0.70 mm tower is still absent. The 0.80 mm pillars are still present.

The case gate, when this is built, has to measure the pillars on the print mesh. They are 0.80 mm square. The bump face has to be a bed face, or at least two full layers, so the preload surface is not a one-point overhang.

## Recommendation

Build concept A.

It keeps clip-on install with the shell off, gives a square face with no free z and no cam into the shell, puts the tooth only on the free end, and takes the anchor column out of the preload chain by seating the root on the fence. The 1.1 mm panel still has preload after 50% relaxation. The arm's install bend is in-plane and under 2.4%. Pillars, where a bridge remains, are 0.80 mm, which is the size the slicer kept. The bump prints against the bed.

Concept B is the same mechanism with a stronger hook. Use it if the square face is judged too willing to slide outboard under a side load. It costs a thinner arm on the short fence.

Concept C is the one to switch to if a snap is not acceptable. The screw stop makes the deflection independent of torque, and the leaf prints on the bed. It adds an M2 screw and a nut at each clip, and a screwdriver.
