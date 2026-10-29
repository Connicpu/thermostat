// Shell hold-down. Not the shipped case.
// Panel drops onto the carrier. Shell clips only to the backplate.
// Names are prefixed so they do not override the button leaves in the case file.
// openscad -D 'part="sh_leaf"' 
include <../thermostat_case.scad>

sh_L = 10;             // clamp face to nub centre
sh_root = 4;
sh_t = 2.60;           // 13 layers
sh_nub_h = 0.80;
sh_nub_len = 2.80;
sh_land0 = 2.05;
sh_land1 = 2.45;
sh_free = 23.05;       // panel back, shell off
sh_stop = 22.90;       // backstop, panel back when the shell has pushed it down

// root x, edge y, +1 top / -1 bottom, width
sh_leaves = [
  [48, 26.24,  1, 2.80],
  [86, 26.24,  1, 2.80],
  [58, 62.94, -1, 2.50],
  [93, 62.94, -1, 2.50]
];

sh_stops = [
  [41.2, 27.6, 48.5, 30.2],   // just right of U7, tied into the top support pad
  [78.0, 27.2, 82.5, 30.0],   // top edge, right of U3's y
  [50.0, 60.7, 55.0, 62.6],   // bottom, clear of C1 and the tails
  [76.0, 60.8, 80.5, 62.7],   // bottom, left of D7
  [43.5, 37.5, 51.2, 41.5]    // right of U8, tied into the carrier spine
];

module sh_leaf_local(w) {
  cube([sh_root + sh_L + sh_nub_len / 2, w, sh_t]);
  translate([sh_root + sh_L - sh_nub_len / 2, sh_land0, sh_t])
    cube([sh_nub_len, sh_land1 - sh_land0, sh_nub_h]);
}

module sh_leaf_board(root_x, edge, dir, w) {
  y0 = dir > 0 ? edge : edge - w;
  y1 = dir > 0 ? edge + w : edge;
  z0 = sh_free - sh_nub_h - sh_t;
  x1 = root_x + sh_root + sh_L + sh_nub_len / 2;
  board_box(root_x, min(y0, y1), x1, max(y0, y1), z0, z0 + sh_t);
  nx0 = root_x + sh_root + sh_L - sh_nub_len / 2;
  ny0 = edge + dir * sh_land0;
  ny1 = edge + dir * sh_land1;
  board_box(nx0, min(ny0, ny1), nx0 + sh_nub_len, max(ny0, ny1), sh_free - sh_nub_h, sh_free);
}

module sh_backstops() {
  for (s = sh_stops)
    board_box(s[0], s[1], s[2], s[3], Z_pcb_t + 0.80, sh_stop);
}

module sh_ribs() {
  tip = sh_stop + 1.22;   // design tip is stop+1.20; 0.02 mm mesh parting
  for (L = sh_leaves) {
    nx = L[0] + sh_root + sh_L - sh_nub_len / 2;
    ny0 = L[1] + L[2] * sh_land0;
    ny1 = L[1] + L[2] * sh_land1;
    board_box(nx, min(ny0, ny1), nx + sh_nub_len, max(ny0, ny1), tip, Z_in + face_recess);
  }
}

module sh_leaves_board() {
  for (L = sh_leaves) sh_leaf_board(L[0], L[1], L[2], L[3]);
}

module sh_leaves_print() {
  for (i = [0:3])
    translate([0, i * 8, 0]) sh_leaf_local(sh_leaves[i][3]);
}

module sh_rib_coupon() {
  // Local shell skin, 0.80 mm, face-down. One land rib stands up from it.
  cube([18, 8, 0.80]);
  translate([7, 3.6, 0.80]) cube([sh_nub_len, sh_land1 - sh_land0, 1.20]);
}

if (part == "sh_leaf") sh_leaf_local(2.80);
if (part == "sh_rib") sh_rib_coupon();
if (part == "sh_leaves") sh_leaves_print();
if (part == "sh_carrier") { carrier(); sh_backstops(); }
if (part == "sh_shell") { shell(); sh_ribs(); }
if (part == "sh_seat") {
  // Shell off. Panel on the leaves, 0.02 mm above the nub.
  sh_leaves_board();
  sh_backstops();
  board_box(panel[0], panel[1], panel[2], panel[3], sh_free + 0.02, sh_free + 1.22);
}
if (part == "sh_on") {
  // Nominal 1.2 mm panel pressed onto the backstop. Leaves are shortened by the 0.15 mm compression.
  translate([0, 0, sh_stop - sh_free]) sh_leaves_board();
  sh_ribs();
  sh_backstops();
  board_box(panel[0], panel[1], panel[2], panel[3], sh_stop + 0.04, sh_stop + 1.24);
}
