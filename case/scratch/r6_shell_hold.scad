// Shell hold-down. Not the shipped case.
// The panel drops onto the carrier backstop. Nothing locks it.
// The shell rim lands on the backplate lip. The clips stay pulled down
// onto that seat, so the shell no longer floats through the old 0.51 mm stack.
// Four PETG fingers in the shell press the border land.
// Names are prefixed so they do not override the button leaves in the case.
// openscad -D 'part="sh_finger"'
include <../thermostat_case.scad>

sh_L = 28;             // clamp face to nub centre, along the edge
sh_root = 4;
sh_t = 2.40;           // bending thickness, 12 layers
sh_w = 2.40;           // outboard of the edge, inside a 3.0 mm recess
sh_gap = 0.40;         // beam stays this far outboard of the nominal edge
sh_tab = 0.60;         // land tab thickness, the top of the beam only
sh_nub_len = 2.80;
sh_land0 = 2.05;
sh_land1 = 2.45;
sh_stop = 22.90;       // panel back. Existing support pads move to this when built.
sh_R = 22.40;          // free finger contact, shell seated, before the panel
sh_dz = 0.10;          // print tolerance left after the hard seat, each way

// root x, edge y, +1 top / -1 bottom
sh_fingers = [
  [42, 26.24,  1],
  [76, 26.24,  1],
  [42, 62.94, -1],
  [76, 62.94, -1]
];

sh_stops = [
  [41.2, 27.6, 48.5, 30.2],
  [78.0, 27.2, 82.5, 30.0],
  [50.0, 60.7, 55.0, 62.6],
  [76.0, 60.8, 80.5, 62.7],
  [43.5, 37.5, 51.2, 41.5]
];

module sh_finger_print() {
  // Print frame, on edge. x along the edge, y is the bending thickness
  // (0 toward the panel, sh_t toward the shell), z grows inboard.
  // This is the orientation Bambu Studio slices with no floating region.
  nx = sh_root + sh_L - sh_nub_len;
  edge_z = sh_w + sh_gap;
  land_z = edge_z + sh_land0;
  cube([sh_root + sh_L, sh_t, edge_z]);
  // Shell-side bar, 0.60 mm thick, out to the land. Panel side of it is 0.60
  // short of the beam's panel face, so the bar stays off the glass.
  translate([nx, sh_t - 0.60, edge_z])
    cube([sh_nub_len, 0.60, sh_land0]);
  // Land face, 0.20 mm proud of that bar.
  translate([nx, sh_t - 0.80, land_z])
    cube([sh_nub_len, 0.80, sh_land1 - sh_land0]);
}

module sh_one_finger(root_x, edge, dir) {
  // Installed, nominal: contact face on a 1.2 mm panel that is on the backstop.
  // That face is sh_stop + 1.20. The tab is the shell side of it.
  y_beam0 = edge - dir * (sh_gap + sh_w);
  y_beam1 = edge - dir * sh_gap;
  z_tab0 = sh_stop + 1.20;
  z_beam0 = z_tab0 + sh_tab - sh_t;
  board_box(root_x, min(y_beam0, y_beam1), root_x + sh_root + sh_L,
            max(y_beam0, y_beam1), z_beam0, z_beam0 + sh_t);
  nx = root_x + sh_root + sh_L - sh_nub_len;
  // Bridge, 0.20 mm clear of the panel, from the edge out to the land.
  by0 = edge;
  by1 = edge + dir * sh_land0;
  board_box(nx, min(by0, by1), nx + sh_nub_len, max(by0, by1), z_tab0 + 0.20, z_tab0 + sh_tab);
  ny0 = edge + dir * sh_land0;
  ny1 = edge + dir * sh_land1;
  board_box(nx, min(ny0, ny1), nx + sh_nub_len, max(ny0, ny1), z_tab0, z_tab0 + sh_tab);
}

module sh_fingers_board() {
  for (f = sh_fingers) sh_one_finger(f[0], f[1], f[2]);
}

module sh_backstops() {
  for (s = sh_stops)
    board_box(s[0], s[1], s[2], s[3], Z_pcb_t + 0.80, sh_stop);
}

module sh_fingers_print() {
  for (i = [0:3])
    translate([i * 40, 0, 0]) sh_finger_print();
}

if (part == "sh_finger") sh_finger_print();
if (part == "sh_fingers") sh_fingers_print();
if (part == "sh_carrier") { carrier(); sh_backstops(); }
if (part == "sh_seat") {
  // Shell off. Panel on the backstop, 0.02 mm above it. No finger, no rise.
  sh_backstops();
  board_box(panel[0], panel[1], panel[2], panel[3], sh_stop + 0.02, sh_stop + 1.22);
}
if (part == "sh_on") {
  sh_fingers_board();
  sh_backstops();
  board_box(panel[0], panel[1], panel[2], panel[3], sh_stop + 0.02, sh_stop + 1.22);
}
