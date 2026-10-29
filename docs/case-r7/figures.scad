// Docs figures only. Does not change the case.
// OPENSCADPATH=case openscad --preview --imgsize=1400,900 --projection=perspective
//   -D 'part="none"' -D 'fig="bridge"' --camera=76,46,32,49,39,23
// fig: bridge | pillar | u7 | j4 | j4close | exploded
// Labels are drawn on the PNG after export.
include <thermostat_case.scad>

fig = "bridge";

module tag(pos, words, ang=[22, 62, 8]) {
  // Labels are drawn on the exported PNG so they stay readable.
}

module nub_only() {
  for (s = sh_spans) {
    edge = sh_y_edge(s[2]);
    dir = sh_dir(s[2]);
    ny0 = edge + dir * sh_land0;
    ny1 = edge + dir * sh_land1;
    z0 = sh_R + sh_nub_h;
    board_box(s[3] - sh_nub_len / 2, min(ny0, ny1), s[3] + sh_nub_len / 2, max(ny0, ny1), sh_R, z0 + 0.02);
  }
}

module dyed() {
  color([0.72, 0.73, 0.76]) backplate();
  pcb();
  color([0.76, 0.62, 0.40]) carrier();
  epd();
  fpc();
  color([0.45, 0.62, 0.82, 0.38]) shell_plain(false);
  color([0.55, 0.34, 0.14]) sh_beams();
  color([0.95, 0.46, 0.08]) nub_only();
  color([0.12, 0.32, 0.52]) sh_pillar_all();
  color([0.70, 0.82, 0.90]) sh_loc_all();
}

module window(a, b) {
  intersection() {
    children();
    translate([min(a[0], b[0]), min(a[1], b[1]), min(a[2], b[2])])
      cube([abs(b[0] - a[0]), abs(b[1] - a[1]), abs(b[2] - a[2])]);
  }
}

module drop_x(x) {
  difference() {
    children();
    translate([x, -20, -10]) cube([220, 120, 90]);
  }
}

module drop_y(y) {
  difference() {
    children();
    translate([-20, y, -10]) cube([220, 120, 90]);
  }
}

module bridge_fig() {
  // Cut through the left nub. The face points toward +X.
  // The pillar stays in the piece, on the far side of the span.
  drop_x(cx(49.4))
    window([cx(29.2), cy(33.2), 19.8], [cx(51.5), cy(23.6), 27.2])
      dyed();
  tag([cx(43.5), cy(31.0), 26.8], "display panel");
  tag([cx(44.0), cy(24.2), 23.4], "fence");
  tag([cx(47.8), cy(28.6), 25.4], "land");
  tag([cx(39.5), cy(27.0), 26.6], "bridge");
  tag([cx(33.5), cy(27.6), 26.8], "pillar");
}

module pillar_fig() {
  // Cut through the left pillar. Face points toward -X, so the nub stays in view.
  difference() {
    window([cx(29.4), cy(32.5), 21.2], [cx(44), cy(23.8), 27.0])
      dyed();
    translate([-30, -20, -10]) cube([cx(31.15) + 30, 120, 90]);
  }
  tag([cx(34.2), cy(31.2), 24.4], "display panel");
  tag([cx(34.0), cy(24.6), 22.4], "fence");
  tag([cx(40.0), cy(28.2), 24.7], "land");
  tag([cx(38.0), cy(27.4), 25.7], "bridge");
  tag([cx(30.6), cy(27.2), 26.2], "pillar");
}

module u7_fig() {
  // Cut across the top-left stop. The face points up the wall, toward the camera.
  drop_y(cy(29.2))
    window([cx(27.6), cy(33.5), 17.2], [cx(44), cy(23.8), 27.2])
      dyed();
  tag([cx(36.5), cy(28.6), 24.6], "display panel");
  tag([cx(37.2), cy(25.0), 22.8], "fence");
  tag([cx(35.2), cy(27.6), 25.5], "bridge");
  tag([cx(31.2), cy(30.0), 21.4], "U7 stop");
}

module j4_fig() {
  // Cut through the connector. Face points up the wall.
  drop_y(cy(43.2))
    window([cx(92), cy(52), 16.4], [cx(116), cy(36), 27.0])
      dyed();
  tag([cx(99), cy(40), 25.0], "display panel");
  tag([cx(108), cy(45), 20.6], "tail");
  tag([cx(101), cy(43.5), 19.8], "J4");
}

module j4_close() {
  // Shell on. Cropped to the cable end so the tail and the mouth read.
  window([cx(96), cy(54), 16.2], [cx(118), cy(34), 27.6])
    dyed();
  tag([cx(107), cy(38), 25.4], "display panel");
  tag([cx(109.5), cy(46), 20.6], "tail");
  tag([cx(102.2), cy(43), 19.8], "J4");
  tag([cx(100), cy(36), 26.2], "shell");
}

module exploded() {
  color([0.72, 0.73, 0.76]) backplate();
  translate([0, 0, 22]) pcb();
  translate([0, 0, 46]) color([0.76, 0.62, 0.40]) carrier();
  translate([0, 0, 68]) epd();
  translate([0, 0, 118]) {
    color([0.43, 0.60, 0.78]) shell_plain(false);
    color([0.58, 0.38, 0.16]) sh_beams();
    color([0.86, 0.52, 0.12]) nub_only();
    color([0.28, 0.42, 0.58]) sh_pillar_all();
    color([0.62, 0.76, 0.86]) sh_loc_all();
  }
  tag([-2, 48, 6], "1  back plate", [68, 0, 55]);
  tag([-2, 48, 22], "2  board", [68, 0, 55]);
  tag([-2, 48, 38], "3  carrier", [68, 0, 55]);
  tag([-2, 48, 52], "4  display panel", [68, 0, 55]);
  tag([-2, 48, 72], "5  shell", [68, 0, 55]);
}

if (fig == "bridge") bridge_fig();
if (fig == "pillar") pillar_fig();
if (fig == "u7") u7_fig();
if (fig == "j4") j4_fig();
if (fig == "j4close") j4_close();
if (fig == "exploded") exploded();
