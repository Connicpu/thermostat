// Shell hold-down. Not the shipped case.
// The panel drops onto the carrier. The shell clips to the backplate.
// Four bridges printed in the shell press the border.
// openscad -D 'part="sh_beams"'
include <../thermostat_case.scad>

sh_root = 2.40;          // pillar along the edge, both ends
sh_t = 0.80;             // bridge thickness, four layers
sh_w = 1.20;             // bridge width across the border
sh_y0 = 2.00;            // outboard face of the bridge, from the panel edge
sh_land0 = 2.50;         // land, on the bridge centreline
sh_land1 = 2.70;
sh_nub_h = 0.45;         // land stands this far below the bridge
sh_nub_len = 4.0;
sh_drop = 0.15;          // carrier stop is this far under Z_panel_b
sh_stop = Z_panel_b - sh_drop;
sh_R_below = 0.38;       // free land under the nominal panel front on the stop
sh_R = sh_stop + panel_t - sh_R_below;
sh_bite = 0.10;          // pillars enter the recess skin by this much
sh_loc_gap = 0.15;       // shell rib to the fence outer face
sh_loc_t = 0.80;

// x0, x1 outer ends, edge 0 top / 1 bottom, nub centre x.
// Left bridges: nub on the span centre. Right bridges: nub toward the cable end.
sh_spans = [
  [29.70, 67.50, 0, 48.60],
  [71.60, 106.40, 0, 100.00],
  [29.70, 67.50, 1, 48.60],
  [71.60, 106.40, 1, 100.00]
];

// Stops added on the carrier. Full height, from the PCB face to sh_stop.
sh_columns = [
  [29.60, 29.30, 33.40, 30.40],
  [33.50, 33.00, 50.20, 34.80],
  [44.30, 26.50, 47.40, 27.70],
  [41.60, 37.20, 50.40, 43.00],
  [34.20, 48.20, 50.20, 51.20],
  [34.20, 60.90, 50.40, 61.55]
];
// Low ties that join a column to the spine. They are feet, not stops.
sh_ties = [
  [33.40, 29.40, 49.50, 30.05]
];

// Shell ribs beside the fence outer faces. They set the shell in y.
sh_locs = [
  [42.4, 52.6, 0],
  [85.0, 95.6, 0],
  [50.0, 55.2, 1],
  [76.4, 89.6, 1]
];

function sh_y_edge(which) = which == 0 ? panel[1] : panel[3];
function sh_dir(which) = which == 0 ? 1 : -1;

module sh_pillar_pair(x0, x1, y0, y1, z0, z1) {
  board_box(x0, min(y0, y1), x0 + sh_root, max(y0, y1), z0, z1);
  board_box(x1 - sh_root, min(y0, y1), x1, max(y0, y1), z0, z1);
}

module sh_bridge(x0, x1, which, nubx) {
  edge = sh_y_edge(which);
  dir = sh_dir(which);
  y0 = edge + dir * sh_y0;
  y1 = edge + dir * (sh_y0 + sh_w);
  z0 = sh_R + sh_nub_h;
  z1 = z0 + sh_t;
  board_box(x0 + sh_root - 0.12, min(y0, y1), x1 - sh_root + 0.12, max(y0, y1), z0, z1);
  ny0 = edge + dir * sh_land0;
  ny1 = edge + dir * sh_land1;
  board_box(nubx - sh_nub_len / 2, min(ny0, ny1), nubx + sh_nub_len / 2, max(ny0, ny1), sh_R, z0 + 0.02);
}

module sh_pillars(x0, x1, which) {
  edge = sh_y_edge(which);
  dir = sh_dir(which);
  y0 = edge + dir * sh_y0;
  y1 = edge + dir * (sh_y0 + sh_w);
  z0 = sh_R + sh_nub_h;
  z1 = Z_in + face_recess + sh_bite;
  sh_pillar_pair(x0, x1, y0, y1, z0, z1);
}

module sh_loc(x0, x1, which) {
  edge = sh_y_edge(which);
  dir = sh_dir(which);
  // Fence outer face is fence_air + fence_t outboard of the edge.
  y_fence = edge - dir * (fence_air + fence_t);
  y_near = y_fence - dir * sh_loc_gap;
  y_far = y_near - dir * sh_loc_t;
  z0 = Z_in - 1.05;
  z1 = Z_in + face_recess + sh_bite;
  board_box(x0, min(y_near, y_far), x1, max(y_near, y_far), z0, z1);
}

module sh_beams() {
  for (s = sh_spans) sh_bridge(s[0], s[1], s[2], s[3]);
}
module sh_pillar_all() {
  for (s = sh_spans) sh_pillars(s[0], s[1], s[2]);
}
module sh_loc_all() {
  for (s = sh_locs) sh_loc(s[0], s[1], s[2]);
}
module sh_added() {
  sh_beams();
  sh_pillar_all();
  sh_loc_all();
}

module sh_widen() {
  // Recess over the bridge, short of the window.
  board_box(panel[0] - 0.30, panel[1] + 2.50, panel[2] + 0.40, panel[1] + 3.30, Z_in - 0.02, Z_in + face_recess);
  board_box(panel[0] - 0.30, panel[3] - 3.30, panel[2] + 0.40, panel[3] - 2.50, Z_in - 0.02, Z_in + face_recess);
}

module sh_shell_case() {
  union() {
    difference() {
      shell();
      sh_widen();
    }
    sh_added();
  }
}

module sh_carrier_case() {
  difference() {
    union() {
      carrier();
      for (c = sh_columns)
        board_box(c[0], c[1], c[2], c[3], Z_pcb_t, sh_stop);
      for (c = sh_ties)
        board_box(c[0], c[1], c[2], c[3], Z_pcb_t, Z_pcb_t + 1.20);
    }
    // Rest surfaces under the outline, brought to the stop.
    // Fences stand outboard of this cut.
    board_box(panel[0] - 0.02, panel[1] - 0.05, panel[2] + 0.02, panel[3] + 0.05,
              sh_stop, Z_panel_b + 0.25);
  }
}

module sh_panel(z_back) {
  board_box(panel[0], panel[1], panel[2], panel[3], z_back, z_back + panel_t);
}

if (part == "sh_beams") sh_beams();
if (part == "sh_pillars") sh_pillar_all();
if (part == "sh_locs") sh_loc_all();
if (part == "sh_added") sh_added();
if (part == "sh_shell") sh_shell_case();
if (part == "sh_shell_print") translate([0, H, Z_out]) rotate([180, 0, 0]) sh_shell_case();
if (part == "sh_carrier") sh_carrier_case();
if (part == "sh_carrier_print") translate([0, 0, -Z_pcb_t]) sh_carrier_case();
if (part == "sh_stock") difference() { shell(); sh_widen(); }
if (part == "sh_stock_print") translate([0, H, Z_out]) rotate([180, 0, 0]) difference() { shell(); sh_widen(); }
if (part == "sh_off") { sh_carrier_case(); sh_panel(sh_stop); }
if (part == "sh_on") { sh_carrier_case(); sh_panel(sh_stop); sh_added(); }
