# Revised retainer concept

The square hook and the 8° undercut, as drawn in `concept-a-square-hook.png` and `concept-b-undercut.png`, do not close. The panel pushes the bump up. That hook can only push down, and it sits outboard of the bump and 4.25 mm away along the fence. The root-on-fence contact can only push up, and it sits on the same side of the hook as the bump. There is no pair of reactions that balances the clip, so it tips about the tooth. About 1.7–5.1° swallows 0.10–0.30 mm of interference, and the shell is reached near 4.5–5°. Preload is about 0 N. The 8° undercut adds about 0.49 N·mm per N of bump force, and the couple the loop needs is about 3.4 N·mm per N, so B does not fix it. Those two drawings stay in the folder as the rejected free bodies.

Connie's rules for this case: PETG only, apart from the four drywall-anchor screws already in the backplate. No tools. The display panel rests on the carrier pads. The shell clips only to the backplate, stays off the panel, and is not a stop for a clip.

## The revised clip

Sketches:

- `docs/case-r6/concept-rev-section.png` — section along the leaf, free body
- `docs/case-r6/concept-rev-plan.png` — plan, stop, tab

Scratch, not the case:

- `case/scratch/r6_lip_leaf_proto.scad`
- `case/scratch/r6_lip_leaf_clip.stl` — the print coupon (clip, pillars, and a 0.70 × 0.60 control tower)
- `case/scratch/r6_lip_leaf_seat.stl` — the same clip on the shelf, lip, stop peg, and a shell wall, so the seat gap and the stop play are in a mesh

The leaf runs along the fence. In x the order is shelf, lip, bump.

- The shelf pushes the heel **up** (bearing). Shelf centre x = 0.50 mm.
- The lip pushes the plate **down** (hold-down). Lip centre x = 2.00 mm. Its underside is square. A 45° chamfer is only on the upstream lower corner, the side the plate enters.
- The bump pushes **up**. Bump centre x = 7.50 mm. Free length from the lip's downstream edge (x = 2.80) to the bump centre is L = 4.70 mm.

With the bearing 1.50 mm upstream of the lip centre and the bump 5.50 mm downstream of it:

H = F × 5.50 / 1.50 = 3.67 F, R = H − F = 2.67 F.

Both are positive. The same three forces share one y, the leaf centreline, so there is no moment out of that plane. The lip covers the anchor, which is the bump's side of the shelf, so a tooth at the far end is not what holds the panel down. A 0.35 mm tooth under the lip's downstream edge only stops the plate sliding back out. It has 0.05 mm of x play in the seat mesh.

The outboard stop is a peg in a slot in the heel, not the shell. Outboard play is 0.08 mm. The plate is 1.20 mm wide under the lip, so the y overlap is 1.20 mm and 0.08 < 1.20 − 0.10. The seated outer face is 0.30 mm from the shell wall. At the stop the air is 0.22 mm. The release tab stays inside the leaf's y, so it does not enter that gap. It clears the recess ceiling by being below the leaf top.

## Preload

E = 1800 N/mm². Leaf t = 0.60 mm, w = 1.20 mm, L = 4.70 mm.

I = 1.20 × 0.60³ / 12 = 0.0216 mm⁴.
k = 3 × 1800 × 0.0216 / 4.70³ = 1.12 N/mm.

The lip, 1.60 mm along x, 1.00 mm thick, and 0.70 mm of overhang, has k ≈ 3 × 1800 × (1.60 / 12) / 0.70³ ≈ 3900 N/mm. In series with the leaf the stiffness falls by about 0.03%. The table is the leaf.

Nominal interference at a 1.2 mm panel is 0.24 mm. A 1.1 mm panel is 0.14 mm. A 1.3 mm panel is 0.34 mm. The datasheet band is 1.2 ± 0.1 mm.

| Panel | δ | Strain at L = 4.70 | Force |
|---|---|---|---|
| 1.1 | 0.14 | 0.57% | 0.16 N |
| 1.2 | 0.24 | 0.98% | 0.27 N |
| 1.3 | 0.34 | 1.39% | 0.38 N |

Strain is 1.5 × t × δ / L², stress along x, in the layer plane. Four leaves after 50% relaxation are 0.31 N at 1.1 mm, above a 5 g shock on the 5.5 g panel (0.27 N).

The warm-wall case is the 1.2 mm row, 0.98%. The tolerance case is a 1.3 mm panel, another 0.05 mm of bump or seat error, and contact on the bump's root-side edge (L = 4.10 mm): δ = 0.39 mm, strain 2.09%, under the 2.4% install gate.

At F = 0.27 N the lip root moment is H × 0.70 = 0.69 N·mm with H = 3.67 F. For a 1.60 mm wide, 1.00 mm thick lip, σ = M (t/2) / I = 2.6 MPa. That is the bending stress in the ledge, across the layers it was built in. As a fraction of E it is 0.14%. It is not zero. The clip has no vertical post in this loop. A post that did carry the hook force would be gated with σ = M c / I from its own section. The old 1.6 mm post was about 6.2 MPa per N of hook force at a 4.25 mm lever. This layout does not use that post.

The leaf is 1.20 mm wide, two beads at a 0.40 mm nozzle. It sits on the border with its outer edge 1.70 mm from the nominal panel edge and its inner edge 2.90 mm from that edge. The active area starts 3.82 mm in. The gate has to repeat this at the worst panel width (36.50–36.90 in the 37.10 opening) and the clip's y play, and fail if the outer edge crosses the 1.52 mm line or the inner edge comes within 0.50 mm of the active area. The 6.0 mm fence segment is too short for a 4.70 mm leaf plus the anchor. The four leaves go on the spans that are at least 8 mm. The long bottom span holds two.

## Print

One pose. Retention face up: the plate top and the latch tooth top are horizontal top surfaces. Layers stack in +z, which is case z, so the leaf stress along x and a y-direction latch stress along x are in the layer plane. The bump is 1.20 × 1.20 × 0.40 mm under the leaf, two model layers, not a bed face. The arm and the tooth of the earlier clip sat about 3 mm below the bump, so the bump cannot be the lowest point of that one-piece shape. Pillars are 0.80 mm square with a 0.50 mm neck, and they are in the clip mesh.

## Slice (PrusaSlicer 2.9.2)

The binary is the Debian package `prusa-slicer 2.9.2+dfsg-1`, reported as `PrusaSlicer-2.9.2+UNKNOWN`. This Ubuntu image's apt candidate is 2.7.2, so 2.9.2 was installed from the Debian snapshot of that package. The package has no Bambu P2S profile. The slice uses a 0.40 mm nozzle, 0.20 mm layers, Arachne, PETG at 240 °C / 80 °C bed, supports off, which is the case 3MF's PETG and support switch.

File: `case/scratch/r6_lip_leaf_clip.stl`. Supports off. Filament 6.70 mm.

The log line is `Floating object part, Low bed adhesion, Loose extrusions`.

- The 0.70 × 0.60 × 4.2 mm tower has 2 extrusion points. It does not build.
- Each 0.80 mm pillar has extrusion from z = 0.4 through z = 3.2.
- The bump has extrusion at z = 3.2 and z = 3.4 (two layers). The toolpath span is 0.75 × 0.75 mm inside the 1.20 × 1.20 mm mesh.
- The retention face has extrusion at z = 4.0.

## Fingers

The shell is off for both steps.

1. Set the panel on the pads. Hold the release tab and slide the plate under the lip from the lead-in end until the tooth passes the lip edge.
2. To remove it, lift the tab about 0.4 mm and slide the plate back. That bend is 1.6% and about 0.45 N, once, with the shell off. The tab is the end of the leaf, in the leaf's own y, so the pull is not on a 0.48 mm arm buried against the rib.

## Gates to add

All of these read the assembled mesh, at the worst panel thickness, the worst panel width, and the clip's y play.

- Hook overlap in y, and the z gap between the plate top and the lip.
- Seat gap between the heel and the shelf.
- Outboard-stop play, which has to be less than the overlap minus 0.10 mm.
- Shell clearance at least 0.21 mm with the clip against that stop, under a side load, under ±5° of tilt, and under the bump-rise probe.
- Shell removed, push up on each bump: rigid rise stays inside the seat gap, and the clip does not rotate as a rigid body.
- The 0.80 mm pillars are in the fit mesh, not only in a print file the fit never opens.
- Strain from the mesh t, L, and δ, including the 1.3 mm panel, +0.05 mm, and contact on the bump's root-side edge. Interlayer stress in any vertical member that carries hook force, from its section.
- Bump and tip against the 1.52 mm line and the active area at the worst offset.
- A 2.9.2 slice of the print mesh: the bump is two layers, and the retention face is a top surface.

## Concept C

`concept-c-screw-bracket.png` stays as the screw-stopped leaf. It is not developed further here. It is not ruled out. It adds an M2 screw and a nut at each bracket and it needs a screwdriver, which is outside PETG-only and outside no-tools. It waits on Connie's explicit choice.

## Optional, not in this concept

- A double-ended clip, anchor in the middle, leaf each way, if one end's latch is awkward to reach.
- A longer leaf where a span allows it, if a warm-wall coupon relaxes past the 50% used above.
- A printed coupon held at 40 °C for a week, to replace that 50% with a measurement.
