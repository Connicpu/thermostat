// Slice prototype of retainer concept A. Not part of the case.
// One clip, tooth on the bed. Layers stack in +z.
// The arm is 0.48 mm in y, so its snap bends in the layer plane.
// The tooth is only on the free end. Pillars are 0.80 mm square.
// A 0.70 x 0.60 tower sits beside it as the control PrusaSlicer drops.
$fn = 24;

module neck_pillar(x, y) {
  translate([x, y, 0]) cube([0.80, 0.80, 3.60]);
  translate([x + 0.15, y + 0.15, 3.60]) cube([0.50, 0.50, 0.55]);
}

module clip() {
  union() {
    // Anchor post. 1.60 mm in y, not the 0.50 mm column.
    cube([1.60, 1.60, 4.40]);
    // Cantilever arm. No tooth on this stretch.
    translate([1.50, 0, 0]) cube([2.90, 0.48, 1.20]);
    // Tooth on the free end only. Top face is the square retention face.
    // The bottom-inner corner is cut back: that is the install lead-in.
    translate([4.30, 0.20, 0.55]) cube([1.50, 0.70, 0.45]);
    translate([4.30, 0.55, 0.30]) cube([1.50, 0.35, 0.25]);
    // Tongue plate, 0.40 mm thick.
    translate([0, 1.40, 4.00]) cube([1.60, 4.00, 0.40]);
    // Bump, 0.40 mm in y, 0.20 mm below the plate. No pillar under it.
    translate([0.40, 4.60, 3.80]) cube([0.80, 0.40, 0.20]);
    // 0.80 mm pillars under the plate, clear of the bump.
    neck_pillar(0.40, 2.10);
    neck_pillar(0.40, 3.30);
  }
}

clip();
// Control. 0.70 x 0.60. PrusaSlicer 2.9.2 reported this as an empty print.
translate([14, 0, 0]) cube([0.70, 0.60, 4.20]);
