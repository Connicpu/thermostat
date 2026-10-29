// Under-panel pad retainer, one station. Not the case.
// y = 0 at the nominal panel edge, positive inboard. z is case z.
// x = 0 at the nub.
// Service faces are parted by 0.05 mm so the bodies stay separate.
// The tooth play, 0.20 mm, is the slide clearance. It is open toward -x.
part = "seat"; // seat | coupon

module carrier() {
  // Floor under the pad. Pad bottom is 0.05 mm above z = 21.05.
  translate([-16.0, -1.80, 20.40]) cube([28.0, 6.60, 0.65]);
  // Root wall, overlapped into the floor. Stops 0.05 mm short of the beam.
  translate([-15.6, 0.40, 20.90]) cube([2.90, 3.8, 1.55]);
  // Lip 0.15 mm above the beam top (22.50). The riser stays on the wall.
  translate([-14.6, 0.70, 22.20]) cube([1.85, 3.0, 0.55]);
  translate([-14.6, 0.70, 22.65]) cube([2.30, 3.0, 0.55]);
  // Shoulder under the plate, outboard of the panel. Top z = 24.30.
  translate([-4.0, -1.45, 20.90]) cube([7.5, 1.25, 3.40]);
  // Stem through the plate hole, and the head above the plate.
  // The groove is open toward -x. Tooth ends at x = -0.35; solid stem starts at -0.15.
  difference() {
    union() {
      translate([-0.75, -0.90, 20.90]) cube([1.00, 0.65, 4.10]);
      translate([-0.85, -0.98, 24.78]) cube([1.25, 0.85, 0.22]);
    }
    translate([-1.40, -1.20, 24.78]) cube([1.25, 1.30, 0.18]);
  }
  // End stop. Leg is outboard of the panel. Bar clears the panel front.
  translate([8.20, -1.70, 20.90]) cube([1.20, 1.65, 4.00]);
  translate([8.20, -1.70, 24.40]) cube([1.20, 4.20, 0.50]);
}

module pad() {
  translate([-12.65, 0.60, 21.10]) cube([11.95, 3.30, 1.40]);
  // Nub top is 0.05 mm under the 1.2 mm panel back. Centre y = 2.25.
  translate([-2.00, 1.15, 22.40]) cube([4.00, 2.40, 0.65]);
}

module plate() {
  // Underside 0.05 mm above the shoulder and above the panel front.
  // The hole clears the stem.
  difference() {
    translate([-8.0, -1.60, 24.35]) cube([16.0, 4.05, 0.40]);
    translate([-1.05, -1.10, 24.25]) cube([1.60, 1.05, 0.60]);
  }
}

module washer() {
  union() {
    // 0.20 mm, 0.05 mm above the plate. Ear is the outboard tab.
    // The opening around the stem is wider than the stem.
    translate([-2.6, -1.90, 24.80]) cube([7.4, 0.85, 0.20]);
    translate([-2.6, -1.05, 24.80]) cube([1.70, 1.00, 0.20]);
    translate([0.50, -1.05, 24.80]) cube([4.3, 1.00, 0.20]);
    // Tooth under the head. 0.20 mm of x play before the head's closed side.
    translate([-1.15, -0.80, 24.84]) cube([0.80, 0.50, 0.08]);
  }
}

module panel() {
  translate([-10.0, 0.0, 23.10]) cube([22.0, 10.0, 1.20]);
}

module shell() {
  // Side wall. Inner face 2.16 mm outboard.
  translate([-18.0, -4.76, 21.00]) cube([36.0, 2.60, 5.25]);
  // Pocket ceiling z = 25.80, from the wall to 2.20 mm inboard.
  translate([-18.0, -2.30, 25.80]) cube([36.0, 4.50, 0.45]);
  // Original ceiling from there to the window at 3.32 mm inboard.
  translate([-18.0, 2.10, 25.51]) cube([36.0, 1.30, 0.74]);
}

module seat() {
  carrier();
  pad();
  plate();
  washer();
  panel();
  shell();
}

// Print pose, bed at z = 0.
// Pad: nub top is the preload face (top layers). Beam bottom is on the bed.
// Plate: underside is the bearing face, on the bed.
// Washer: stop face is on the bed. One 0.20 mm layer.
module coupon() {
  cube([12.7, 3.3, 1.40]);
  translate([8.7, 0.45, 1.40]) cube([4.00, 2.40, 1.00]);
  translate([18.0, 0, 0]) cube([16.0, 4.0, 0.40]);
  translate([18.0, 5.0, 0]) cube([8.0, 4.2, 0.20]);
}

if (part == "seat") seat();
if (part == "coupon") coupon();
if (part == "carrier") carrier();
if (part == "pad") pad();
if (part == "plate") plate();
if (part == "washer") washer();
if (part == "panel") panel();
if (part == "shell") shell();
