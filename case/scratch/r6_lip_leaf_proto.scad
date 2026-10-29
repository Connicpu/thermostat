// Revised retainer concept. Not the case.
// Fence-parallel leaf. In x the shelf (up), the lip (down), and the bump (up)
// are in that order, so the preload loop closes. The latch only locks the slide.
// Print pose of part="clip": retention face up, bump 1.00 x 1.00 x 0.40,
// pillars 0.80 mm square. part="seat" adds the fence, lip, stop, and shell wall.
part = "clip";
$fn = 16;

module pillars() {
  module one(x, y) {
    translate([x, y, 0]) cube([0.80, 0.80, 3.42]);
    translate([x + 0.15, y + 0.15, 3.40]) cube([0.50, 0.50, 0.15]);
  }
  // Beside the bump, not on its contact face. Bump is x 7.00–8.00.
  one(4.10, 0.10);
  one(5.70, 0.10);
}

module clip_solid() {
  difference() {
    union() {
      // Anchor plate, 0.80 thick. Top z=4.00 is the face under the lip.
    translate([0, 0, 3.20]) cube([2.80, 1.20, 0.80]);
    // Leaf, 0.60 thick, 1.20 wide. Free edge of the lip is x=2.80.
    // Bump centre is x=7.50, so the free length is 4.70.
    translate([2.70, 0, 3.40]) cube([4.80, 1.20, 0.60]);
    // Bump 1.20 x 1.20 x 0.40. Centre x=7.50.
    translate([6.90, 0, 3.00]) cube([1.20, 1.20, 0.40]);
      // Position tooth under the lip's downstream edge. Square top at z=4.00.
      // It stops at x=2.75 so the lip, which starts at x=1.20 and ends at 2.80, keeps 0.05 mm of x play.
      translate([2.40, 0.25, 3.70]) cube([0.35, 0.50, 0.30]);
      // Release tab on the leaf tip, inside the leaf's y.
      translate([7.90, 0.05, 3.40]) cube([1.70, 0.90, 0.50]);
    }
    // Slot for the carrier stop. Outboard play is 0.08 mm (peg starts at y=0.28).
    translate([0.25, 0.20, 3.10]) cube([0.50, 0.70, 1.00]);
  }
}

module seat_carrier() {
  union() {
    // Shelf. Top is z=3.20, flush with the heel bottom. x 0–1.00.
    translate([-0.30, -0.20, 0]) cube([1.50, 1.40, 3.20]);
    // Lip post and lip. Lip underside z=4.00, x 1.20–2.80.
    translate([1.20, -0.70, 0]) cube([1.60, 0.70, 4.80]);
    translate([1.20, -0.10, 4.00]) cube([1.60, 1.20, 0.70]);
    // Stop peg. Slot is y 0.20–0.90. Peg y 0.28–0.82, so 0.08 outboard play.
    translate([0.35, 0.28, 3.30]) cube([0.30, 0.54, 0.70]);
    // The lip end at x=2.80 is the x stop for the tooth. No second hook.
  }
}

module shell_wall() {
  // Inboard face at y=-0.30. Seated heel face is y=0, so the air is 0.30 mm.
  // Against the stop the clip moves 0.08 and the air is 0.22 mm.
  translate([-0.40, -0.60, 0]) cube([3.2, 0.30, 5.3]);
}

if (part == "seat") {
  color("burlywood") seat_carrier();
  color("khaki") clip_solid();
  color("lightblue") shell_wall();
} else {
  clip_solid();
  pillars();
  translate([14, 0, 0]) cube([0.70, 0.60, 4.20]);
}
