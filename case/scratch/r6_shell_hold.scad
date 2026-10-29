// Shell hold-down. Not the shipped case.
// The panel drops onto the carrier. Nothing locks it.
// The shell clips to the backplate and stays in the snap window.
// Four bridges printed in the shell press the border land.
// Names are prefixed so they do not override the case.
// openscad -D 'part="sh_beams"'
include <../thermostat_case.scad>

sh_root = 2.40;          // pillar along the edge, both ends
sh_L = 33;               // free span between the pillar faces
sh_t = 0.80;             // bridge thickness, 4 layers, bending in z
sh_w = 2.90;             // bridge width in y
sh_y0 = 0.20;            // outboard face of the bridge, inboard of the edge
sh_land0 = 2.20;         // land, from the panel edge
sh_land1 = 2.30;
sh_nub_h = 0.60;         // land stands this far below the bridge
sh_nub_len = 5.0;        // land length along the edge
sh_R = 22.74;            // free land face, shell centred in the window
sh_stop = 22.90;         // panel back. Carrier pads are cut to this.
sh_bite = 0.10;          // pillars enter the recess skin by this much
sh_shoe_t = 0.80;
sh_shoe_h = 1.20;
sh_shoe_pre = 0.20;      // free shoe crosses the edge by this much
sh_shoe_post = 2.0;

// Outer ends of each bridge, and 0 = top edge / 1 = bottom edge.
// y comes from panel[1] or panel[3]. The free span is sh_L.
sh_spans = [
  [29.70, 67.50, 0],
  [68.70, 106.50, 0],
  [29.70, 67.50, 1],
  [68.70, 106.50, 1]
];

// One shoe on each long edge, in a fence gap. Outer x, then the edge index.
// The posts are sh_shoe_post at each end. The bar between them is the spring.
sh_shoes = [
  [56.0, 80.0, 0],
  [31.0, 49.0, 1]
];

// Solid columns from the PCB face up to the stop. Constant section, no bridge.
// Each column reaches an existing carrier member so the print is one solid.
sh_columns = [
  [33.5, 33.0, 50.2, 34.8],
  [44.3, 26.50, 47.4, 27.70],
  [41.6, 37.2, 50.4, 43.0],
  [34.2, 48.2, 50.2, 51.2],
  [34.2, 60.90, 50.4, 61.55]
];

function sh_y_edge(which) = which == 0 ? panel[1] : panel[3];
function sh_dir(which) = which == 0 ? 1 : -1;

module sh_pillar_pair(x0, x1, y0, y1, z0, z1) {
  board_box(x0, min(y0, y1), x0 + sh_root, max(y0, y1), z0, z1);
  board_box(x1 - sh_root, min(y0, y1), x1, max(y0, y1), z0, z1);
}

module sh_bridge(x0, x1, which) {
  edge = sh_y_edge(which);
  dir = sh_dir(which);
  y0 = edge + dir * sh_y0;
  y1 = edge + dir * (sh_y0 + sh_w);
  z0 = sh_R + sh_nub_h;
  z1 = z0 + sh_t;
  board_box(x0 + sh_root - 0.12, min(y0, y1), x1 - sh_root + 0.12, max(y0, y1), z0, z1);
  xc = (x0 + x1) / 2;
  ny0 = edge + dir * sh_land0;
  ny1 = edge + dir * sh_land1;
  board_box(xc - sh_nub_len / 2, min(ny0, ny1), xc + sh_nub_len / 2, max(ny0, ny1), sh_R, z0 + 0.02);
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

module sh_shoe(x0, x1, which) {
  edge = sh_y_edge(which);
  dir = sh_dir(which);
  // Free shape. The inboard face crosses the edge; the panel pushes it back.
  y_in = edge + dir * sh_shoe_pre;
  y_out = y_in - dir * sh_shoe_t;
  z0 = 23.50;
  z1 = z0 + sh_shoe_h;
  board_box(x0 + sh_shoe_post, min(y_in, y_out), x1 - sh_shoe_post, max(y_in, y_out), z0, z1);
  ztop = Z_in + face_recess + sh_bite;
  board_box(x0, min(y_in, y_out), x0 + sh_shoe_post, max(y_in, y_out), z0, ztop);
  board_box(x1 - sh_shoe_post, min(y_in, y_out), x1, max(y_in, y_out), z0, ztop);
}

module sh_beams() {
  for (s = sh_spans) sh_bridge(s[0], s[1], s[2]);
}
module sh_pillar_all() {
  for (s = sh_spans) sh_pillars(s[0], s[1], s[2]);
}
module sh_shoe_all() {
  for (s = sh_shoes) sh_shoe(s[0], s[1], s[2]);
}
module sh_added() {
  sh_beams();
  sh_pillar_all();
  sh_shoe_all();
}

module sh_widen() {
  // The shipped recess stops 2.50 mm inboard. The bridge needs the rest of
  // the border, and it stops short of the window.
  board_box(panel[0] - 0.30, panel[1] + 2.50, panel[2] + 0.40, panel[1] + 3.25, Z_in - 0.02, Z_in + face_recess);
  board_box(panel[0] - 0.30, panel[3] - 3.25, panel[2] + 0.40, panel[3] - 2.50, Z_in - 0.02, Z_in + face_recess);
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

// Old fence face is fence_air outboard. The new face is further out so the
// shoes can centre the panel when the shell is off the carrier by the lip gap.
sh_fence_face = 1.05;
sh_fence_t = 0.70;

module sh_new_fence(x0, x1, which) {
  edge = sh_y_edge(which);
  dir = sh_dir(which);
  y_in = edge - dir * sh_fence_face;
  y_out = y_in - dir * sh_fence_t;
  board_box(x0, min(y_in, y_out), x1, max(y_in, y_out), Z_pcb_t, Z_in - 0.40);
}

module sh_carrier_case() {
  union() {
    difference() {
      union() {
        carrier();
        for (c = sh_columns)
          board_box(c[0], c[1], c[2], c[3], Z_pcb_t, sh_stop);
      }
      // Every surface the panel can sit on, cut to the stop.
      // The x-stop lips continue above this band.
      board_box(panel[0] - 0.02, panel[1] - 0.08, panel[2] + 0.02, panel[3] + 0.08,
                sh_stop, Z_panel_b + 0.25);
      // Shipped fence walls and their ribs. The low ties, below z 20.2, stay.
      board_box(41.5, panel[1] - 2.20, 53.5, panel[1] - 0.05, 20.2, 24.2);
      board_box(84.1, panel[1] - 2.20, 96.5, panel[1] - 0.05, 20.2, 24.2);
      board_box(49.1, panel[3] + 0.05, 56.1, panel[3] + 2.20, 20.2, 24.2);
      board_box(75.5, panel[3] + 0.05, 90.5, panel[3] + 2.20, 20.2, 24.2);
    }
    sh_new_fence(42.0, 53.0, 0);
    sh_new_fence(84.6, 96.0, 0);
    sh_new_fence(49.6, 55.6, 1);
    sh_new_fence(76.0, 90.0, 1);
  }
}

module sh_panel(z_back) {
  board_box(panel[0], panel[1], panel[2], panel[3], z_back, z_back + panel_t);
}

if (part == "sh_beams") sh_beams();
if (part == "sh_pillars") sh_pillar_all();
if (part == "sh_shoes") sh_shoe_all();
if (part == "sh_added") sh_added();
if (part == "sh_shell") sh_shell_case();
if (part == "sh_shell_print") translate([0, H, Z_out]) rotate([180, 0, 0]) sh_shell_case();
if (part == "sh_carrier") sh_carrier_case();
if (part == "sh_carrier_print") translate([0, 0, -Z_pcb_t]) sh_carrier_case();
if (part == "sh_stock") difference() { shell(); sh_widen(); }
if (part == "sh_stock_print") translate([0, H, Z_out]) rotate([180, 0, 0]) difference() { shell(); sh_widen(); }
if (part == "sh_off") {
  sh_carrier_case();
  sh_panel(sh_stop);
}
if (part == "sh_on") {
  sh_carrier_case();
  sh_panel(sh_stop);
  sh_added();
}
