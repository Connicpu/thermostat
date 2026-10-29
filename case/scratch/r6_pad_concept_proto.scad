// One retainer station. Not the shipped case.
// x = 0 at the nub, y = 0 at the panel edge (+y inboard), z = case z.
// Printed parts are flat: pad, plate, clamp bar, lip cap. No bridge on a contact face.
// Mesh partings are 0.02–0.05 mm so the bodies stay separate.
part = "seat"; // seat | coupon | carrier | pad | plate | cap | bar

module pad() {
  // Free beam height everywhere. The seat nub is drawn at the seated height
  // (23.08) so it does not occupy the 1.2 mm panel. Free nub top is 23.55.
  translate([-22, -0.50, 19.55]) cube([4.0, 1.60, 3.00]);           // root
  translate([-18, -0.50, 19.55]) cube([2.4, 1.70, 3.00]);           // neck
  translate([-18, 1.00, 19.55]) cube([18, 2.30, 3.00]);             // beam y 1.00–3.30
  translate([-2.0, 1.90, 22.55]) cube([4.0, 1.20, 0.53]);           // seated nub, top 23.08
}

module bar() {
  // 1.20 mm, entirely outboard of the edge, 0.02 mm above the root.
  translate([-21.7, -0.45, 22.57]) cube([3.4, 0.40, 1.20]);
}

module plate() {
  difference() {
    union() {
      translate([-16, -1.00, 23.90]) cube([22, 0.85, 0.60]);
      translate([-16, -0.40, 24.25]) cube([22, 0.38, 0.25]);
      translate([-16, -0.08, 24.45]) cube([22, 0.28, 0.20]);
      translate([-16, 0.00, 24.50]) cube([22, 2.05, 0.60]);
      translate([-16, 2.00, 24.36]) cube([22, 1.20, 0.40]);
      translate([4.6, -1.86, 23.90]) cube([1.20, 1.10, 0.60]);
    }
    // Clearance notches around the lip posts.
    translate([-15.0, -2.10, 23.70]) cube([2.20, 1.15, 1.10]);
    translate([-3.6, -2.10, 23.70]) cube([2.20, 1.15, 1.10]);
  }
}

module carrier() {
  // Base ties the posts, the clamp wall, the backstop and the detent.
  translate([-23.2, -1.80, 18.60]) cube([30, 3.4, 0.50]);
  // Root floor, 0.02 mm under the beam.
  translate([-22.4, -1.70, 18.60]) cube([5.2, 2.4, 0.93]);
  // Clamp wall, 1.20 mm. Outer face y=-1.80, shell at -2.16.
  translate([-22.6, -1.90, 18.60]) cube([5.6, 1.20, 5.20]);
  // Lip posts, 1.20 mm, tops at 24.50.
  translate([-14.6, -1.95, 18.60]) cube([1.20, 0.80, 5.90]);
  translate([-3.2, -1.95, 18.60]) cube([1.20, 0.80, 5.90]);
  // Backstop. Top 22.75. Downstream of the beam.
  translate([2.6, 0.35, 18.60]) cube([2.4, 1.20, 4.15]);
  // Shelf under the outboard edge of the plate bar. Top 23.85, bar bottom 23.90.
  translate([-16, -1.70, 18.60]) cube([22, 0.80, 5.15]);
  // Detent, 1.20 mm, clear of the arm.
  translate([3.0, -1.95, 18.60]) cube([1.20, 0.80, 6.20]);
}

module cap() {
  // 0.60 mm. Stop face 0.02 mm above the posts and the plate bar.
  translate([-14.8, -1.90, 24.56]) cube([13.0, 1.70, 0.60]);
}

module panel() {
  translate([-24, 0, 23.10]) cube([34, 8, 1.20]);
}

module pcb() {
  translate([-26, -2.4, 18.15]) cube([36, 12, 0.40]);
}

module shell() {
  translate([-28, -4.36, 20.4]) cube([42, 2.20, 6.0]);             // inner face y=-2.16
  translate([-28, -2.20, 25.92]) cube([42, 4.40, 0.33]);           // pocket, skin 0.33 over the cap
  translate([-28, 2.15, 25.51]) cube([42, 1.25, 0.74]);
}

module seat() {
  pcb();
  carrier();
  pad();
  bar();
  plate();
  cap();
  backstop_note();
  panel();
  shell();
}

module backstop_note() {
  // named so the seat list stays obvious; the rib is inside carrier()
}

module pad_print() {
  cube([22, 2.40, 3.00]);
  translate([18, 0.60, 3.00]) cube([4.0, 1.20, 1.00]);
  translate([0, -2.2, 0]) cube([4.0, 2.2, 3.00]);
}

module plate_print() {
  // Every region starts on the bed. No stepped overhang.
  // Land is 0.40 mm. Frame is 0.60 mm, its extra height sitting on its own first layers.
  cube([22, 1.20, 0.40]);
  translate([0, 1.20, 0]) cube([22, 0.80, 0.40]);
  translate([0, 2.00, 0]) cube([22, 3.20, 0.60]);
  translate([22.2, 2.4, 0]) cube([1.20, 4.0, 0.60]);
}

module bar_print() {
  cube([3.2, 1.20, 1.20]);
}

module cap_print() {
  cube([14.6, 2.40, 0.60]);
}

module carrier_print() {
  translate([0, 0, -18.60]) carrier();
}

module coupon() {
  pad_print();
  translate([0, 6, 0]) plate_print();
  translate([0, 16, 0]) bar_print();
  translate([6, 16, 0]) cap_print();
}

if (part == "seat") { pcb(); carrier(); pad(); bar(); plate(); cap(); panel(); shell(); }
if (part == "s_pcb") pcb();
if (part == "s_carrier") carrier();
if (part == "s_pad") pad();
if (part == "s_bar") bar();
if (part == "s_plate") plate();
if (part == "s_cap") cap();
if (part == "s_panel") panel();
if (part == "s_shell") shell();
if (part == "coupon") coupon();
if (part == "carrier") carrier_print();
if (part == "pad") pad_print();
if (part == "plate") plate_print();
if (part == "cap") cap_print();
if (part == "bar") bar_print();
