// Connie's Super Awesome Thermostat V3 - parametric case (OpenSCAD)
// Units mm. Board coords: origin top-left, x right, y down (same as KiCad, board-relative).
// Case coords: origin outer bottom-left of backplate, X right, Y up, Z out of wall (wall surface Z=0).
// Three air zones: hot, middle, and sensor duct.
part = "assembly";   // backplate | shell | carrier | caps | assembly | exploded | section | section_hooks | section_airflow
explode = 0;         // used by "exploded"
$fn = 48;

// ---------------- parameters ----------------
W = 147; H = 71;            // outer footprint (drywall-anchor version); 1.1 mm board-edge gap all round
wall = 2.4;                 // shell side walls (>=2 mm)
clr = 0.3;                  // fit clearance
plate_t = 3.0;              // backplate thickness
standoff_h = 14.0;          // backplate face -> PCB back, so the 11.1 mm 15EDGK plug and 9.5 mm C43 clear a flat plate and lip
pcb_t = 1.6;
panel_gap = 4.6;            // PCB top -> panel back. 4.6 keeps >=3.0 mm of tail in J4 even if the carrier prints 0.10 mm tall. See case/ASSEMBLY.md
panel_t = 1.2;
front_t = 2.0;              // front face thickness
BW = 140; BH = 64;          // PCB
OX = 3.5;                   // board left edge in case X
OYT = 3.5;                  // board top edge distance from case top
// derived Z
Z_pcb_b = plate_t + standoff_h;
Z_pcb_t = Z_pcb_b + pcb_t;
Z_panel_b = Z_pcb_t + panel_gap;
Z_in = Z_panel_b + panel_t;          // shell inner face = panel front
Z_out = Z_in + front_t;              // total depth
function cx(x) = OX + x;
function cy(y) = H - OYT - y;

// board features (board coords)
holes = [[6.4,6.4],[133.6,6.4],[6.4,57.6],[133.6,57.6]];
bulk = [53.25,58]; bulk_h = 9.5;          // rear C43 8x9 mm electrolytic (+0.5 lead standoff)
plug_w = 18.3; plug_h = 11.1;        // Degson 15EDGK-3.5-05P: (N-1)*3.5+4.3 wide, 11.1 high, 15.6 deep
plug_y0 = 40.45; plug_y1 = 56.05;    // board-y extent of the mated plug (wire entry at y0, toward the hole)
plug_pocket = 0;                     // no plug pocket; the standoffs clear the plug on a flat plate
term = [70,58];                      // rear RA header J1 pin row centre (pins along x, mating face toward -y)
panel = [29.46,26.24,108.46,62.94];  // GDEY029T94 outline (x0,y0,x1,y1)
aa    = [32.46,30.06,99.36,59.12];   // active area (3.0/3.82 mm margins, FPC end on right)
win_m = 0.5;                         // window margin around active area
// GDEY029T94 tail and Jushuo AFC07-S24FCA-00. Drawing numbers; the fold is derived in case/ASSEMBLY.md.
fpc_free = 14.40;                    // free length past the outline
fpc_wide = 12.50;
fpc_thick = 0.30;
fpc_z_ret = 0.50;                    // lower leg. Side view shows 0.50±0.05; it is not labeled as the centerline
mouth_x = 103.77;                    // J4 silk front face. Mouth faces +x
fpc_R = (panel_gap - fpc_z_ret) / 2;
fpc_straight = panel[2] - mouth_x;
fpc_arc = PI * fpc_R;
fpc_ins = fpc_free - fpc_arc - fpc_straight;
// A carrier that prints 0.10 mm tall opens the gap by the same amount.
fpc_ins_hi = fpc_free - PI * (panel_gap + 0.10 - fpc_z_ret) / 2 - fpc_straight;
echo(str("FPC R=", fpc_R, " arc=", fpc_arc, " straight=", fpc_straight, " inserted=", fpc_ins, " at +0.10=", fpc_ins_hi));
assert(fpc_ins > 3.0, "FPC tail insertion is short of the connector minimum");
assert(fpc_ins_hi > 3.0, "a tall carrier print drops the FPC below 3.0 mm");
assert(fpc_ins < 4.2, "FPC tail insertion is long");
// 4.0 mm opto under the glass. A short carrier print must still leave 0.40 mm.
assert(panel_gap - 0.10 - 4.0 >= 0.40, "panel gap does not clear the optos");
// carrier hooks. Two per long edge; the FPC end stays open.
// The land behind the glass has to clear a thick board (+0.16), a tall carrier
// print (+0.10), a low snap seat (+0.10), and a tall standoff (+0.15): 0.51 mm.
// 0.70 mm leaves 0.19 mm. The window chamfer starts further out, at Z_in+0.85.
face_recess = 0.70;
cl_t = 0.80;                         // hook beam, two perimeters at a 0.4 mm nozzle
cl_gap = 0.25;                       // air between the beam and the glass edge. The pad does not fill this.
cl_lip = 0.32;                       // how far the ledge crosses the glass edge. This is the beam deflection.
cl_lip_h = 0.25;                     // flat shelf on the glass face. Its underside is that face, so a tool cannot get under it.
cl_foot = 0.40;                      // only the bottom of the beam is tied to the pad
cl_pocket = 1.10;                    // shell relief: shelf + 45 deg ramp + a little air
cl_slot_w = 4.0;                     // tool slot in the outer face of each beam, centered on the 6 mm hook
cl_slot_h = 1.2;
cl_slot_d = 0.45;                    // leaves a 0.35 mm ligament. The foot section, which sets the force, stays 0.80 mm.
xstop_bite = 0.05;                   // +x posts cross the glass edge by this much, so a short print still stops the slide
// Free length is the glass front minus the foot. E = 1.8 GPa. Strain = 1.5 t δ / L^2.
cl_L = (Z_in - (Z_pcb_t + cl_foot));
cl_E = 1800;                         // N/mm^2
cl_I = 6 * cl_t * cl_t * cl_t / 12;  // 6 mm wide beam
cl_eps = 1.5 * cl_t * cl_lip / (cl_L * cl_L);
cl_F = 3 * cl_E * cl_I * cl_lip / (cl_L * cl_L * cl_L);
echo(str("HOOK L=", cl_L, " strain=", cl_eps, " force_N=", cl_F));
assert(cl_L >= 5.0, "hook beam is too short");
assert(cl_eps < 0.020, "hook strain is past the PETG snap limit");
assert(cl_F < 8, "hook takes more than a finger");
btn = [[112,30],[112,40],[112,50]];  // UP DOWN MODE
pin = [[93,5]];                      // RESET pinhole only (BOOT is only useful with USB attached)
led = [63.3,21.2];                   // D5 status LED -> light-pipe hole
usb_x = 121;                         // USB-C centre x (mouth faces board bottom edge)
island = [121,25,135,39];            // SHT40 thermal island
anchors = [[40,12],[106,12],[40,58],[106,58]]; // case XY of 4 drywall-anchor screws
anchor_d = 4.2;  anchor_head_d = 8.2; anchor_head_h = 1.6;   // #6 / M3.5 pan head into plastic anchor
wire_open = [25,20];                 // centred drywall wire hole
inlay_d = 0.6;                       // 3 layers @0.2 mm, flush inlays on the face-down surface
include <cat_logo.scad>
cat_xy = [18.5,35]; cat_s = 0.8;     // orange cat inlay left of the window
include <swirls.scad>
show_title = false;                  // no front title; the swirls use that space
swirl_style = "bold";                // "subtle" | "bold" | "none"
swirl_w = swirl_style=="bold" ? 1.8 : 1.2;   // line width (>=3 extrusion widths at 0.4 nozzle)
swirl_margin = 2.0;                  // clearance to window/buttons/holes/lettering/cat

// ---- airflow zones ----
// Front-face clearances include JLCPCB's ±0.16 mm board thickness. The board sits on the
// standoffs, so a thicker board raises the front face and a thinner one does not move the back.
pcb_tol = 0.16;
part_clr = 0.5;
vslot_w = 1.6; vslot_p = 3.2;        // edge-vent slot / web (1.6 = 4 lines at a 0.4 mm nozzle)
vslot_z = [6.5, 23.4];               // above the 5.5 mm lip, 1.0 mm short of the inner face (Z_in = 24.4)
ribL_bx = [26.8, 28.4];              // rear divider, hot zone | middle
// Rib S is at board x 119.0 so the Ø0.65 J2 peg at board x 118.11 keeps >= 0.5 mm
// (118.8 would leave only 0.37 mm). A notch over the peg would open the duct at the inlet.
// The locating pins are Ø3.9 in Ø4.3 holes, so the board can still shift ±0.2 mm.
// Rib S (board x119.0) does not line up with the front partition, which ends at board x117.8.
// That 1.2 mm offset times the 1.1 mm board-edge gap is an ~1.3 mm² leak around the edge,
// between the sensor duct and the middle zone.
ribS_bx = [119.0, 120.6];            // rear divider, middle | sensor duct
rib_gap = 0.4;                       // rib top -> PCB back. Never let the rib touch the board.
rib_end_y = 2.7;                     // lip outer face; caps the rib out to here above the lip
curtain_bx = [27.7, 29.0];           // front curtain, just left of the panel (the outline starts at x 29.46).
                                     // 0.6 mm clear of L1, so that Y-band can sit down near the board.
// C17408 (R22/R32/R42) max body is 2.15 x 1.40 x 0.65. The left edge sits 0.425 mm
// right of this wall. On a +0.16 mm board a 1.0 mm floor leaves ~0.47 mm at the corner;
// 1.1 mm leaves >=0.5 mm. These three sit between the notches, so the floor is what clears them.
curtain_z_floor = 1.1;
// [board y0, y1, height above the PCB]. Lower everywhere the parts allow; C2 holds 3.4.
curtain_notches = [
  [8.4, 11.6, 1.5],                  // R23 0805 (max ~0.65)
  [23.4, 26.6, 1.5],                 // R33
  [38.4, 41.6, 1.5],                 // R43
  [47.8, 52.2, 3.4]                  // C2 1210 at its 2.7 mm max
];
part_bx = [115.9, 117.8];            // front partition. Overlaps the button tubes (fuses a 0.30 mm sliver).
part_z0 = 1.3;                       // clears a 0.55 mm 0402 with the 0.16 mm board tolerance
part_d3_z = 3.2;                     // D3 SMA; raised past Zt+3.0 so +0.16 mm still leaves 0.5
part_j2_z = 4.0;                     // J2 USB-C body is 3.26
baffle_gap = part_clr + pcb_tol;     // 0.66: still 0.5 mm when the board is thick
baffle_x0 = island[0] - 1.25;        // outer left wall, >=0.5 mm clear of R14
baffle_xi = baffle_x0 + 1.2;         // 1.2 mm wall
L_vx = [for (i = [0:7]) 6.0 + i*vslot_p];                 // hot-zone top/bottom slot centres (case X)
S_vx = [for (i = [0:5]) 126.8 + i*vslot_p];               // sensor slots, all to the right of rib S
L_sy = [7.0, 10.2, 13.4, 16.6, 55.0, 58.2, 61.4, 64.6];   // left wall: low inlet, high exhaust (case Y)
S_sy = [for (i = [0:7]) 23.0 + i*vslot_p];                // right wall, rear, island band (case Y)

// ---------------- helpers ----------------
module rrect(w,h,r){ offset(r) square([w-2*r,h-2*r]); }
module rbox(w,h,d,r){ linear_extrude(d) translate([r,r]) rrect(w,h,r); }
module slot2d(l,w){ hull(){ translate([-l/2+w/2,0]) circle(d=w); translate([l/2-w/2,0]) circle(d=w);} }
// Axis-aligned box in board x/y and case z. cy() flips y, so the box is placed at the larger board y.
module board_box(x0, y0, x1, y1, z0, z1) {
  translate([cx(x0), cy(y1), z0]) cube([x1 - x0, y1 - y0, z1 - z0]);
}
// Rear zone wall. Full height inside the lip; above the lip it reaches the lip's outer face
// so the 2.3 x 11.5 mm end windows against the shell close. Prints upright, no supports.
module zone_rib(bx0, bx1) {
  rw = bx1 - bx0;
  ztop = Z_pcb_b - rib_gap;
  translate([cx(bx0), po + lip_t - 0.2, 0])
    cube([rw, ph - 2*lip_t + 0.4, ztop]);
  translate([cx(bx0), rib_end_y, lip_h])
    cube([rw, lip_t + 0.2, ztop - lip_h]);
  translate([cx(bx0), H - rib_end_y - lip_t - 0.2, lip_h])
    cube([rw, lip_t + 0.2, ztop - lip_h]);
}

// ---------------- backplate ----------------
pw = W-2*wall-2*clr; ph = H-2*wall-2*clr; po = wall+clr;
lip_h = 5.5; lip_t = 2;     // 5.5 mm so rear C43 clears the lip by >= 2 mm with no notch
module backplate(){
  difference(){
    union(){
      // plate (flat, no plug pocket)
      translate([po,po,0]) rbox(pw,ph,plate_t,3);
      // perimeter lip that the shell slides over
      // full height all round, no notch under C43
      translate([po,po,0]) difference(){ rbox(pw,ph,lip_h,3); translate([lip_t,lip_t,-1]) rbox(pw-2*lip_t,ph-2*lip_t,lip_h+2,1.5); }
      // snap bumps (45 deg both sides, print without supports). Base embeds 0.1 mm into the lip so the bumps fuse.
      for(s=[[W/2-35,po,0],[W/2+35,po,0],[W/2-35,H-po,180],[W/2+35,H-po,180]])
        translate([s[0],s[1],3.2]) rotate([0,0,s[2]]) rotate([0,90,0]) linear_extrude(10,center=true) polygon([[0.9,0.1],[0.9,0],[0,-0.8],[-0.9,0],[-0.9,0.1]]);
      // PCB standoffs with locating pins
      for(h=holes) translate([cx(h[0]),cy(h[1]),0]){ cylinder(d=8,h=Z_pcb_b); cylinder(d=3.9,h=Z_pcb_b+pcb_t-0.2); }
      zone_rib(ribL_bx[0], ribL_bx[1]);
      zone_rib(ribS_bx[0], ribS_bx[1]);
    }
    // centred drywall wire hole; wires bend into the rear right-angle plug
    translate([W/2,H/2,-1]) linear_extrude(plate_t+2) offset(3) square([wire_open[0]-6,wire_open[1]-6],center=true);
    // 4 drywall anchor screw holes with pan-head recesses
    for(p=anchors) translate([p[0],p[1],-1]){ cylinder(d=anchor_d,h=plate_t+2); translate([0,0,plate_t+1-anchor_head_h]) cylinder(d=anchor_head_d,h=anchor_head_h+0.01); }
    // no drywall-facing island slots: slots there would open the sensor duct onto the wall
    // label recess sits right of rib L so the rib does not cut it
    translate([W/2-26,H/2,plate_t-0.6]) linear_extrude(1) text("TOP ^  V3",size=3.5,halign="center",valign="center");
  }
}

// ---------------- front shell ----------------
module shell(pk=true){   // pk=false: preview only, pockets left solid (inlays sit 0.01 proud) to avoid OpenCSG specks
  difference(){
    union(){
      difference(){
        rbox(W,H,Z_out,4);
        translate([wall,wall,-1]) rbox(W-2*wall,H-2*wall,Z_in+1,1.2);   // r1.2: square PCB corners keep >=1 mm
      }
      // PCB clamp posts at mounting holes
      for(h=holes) translate([cx(h[0]),cy(h[1]),Z_pcb_t+0.1]) cylinder(d=6,h=Z_in-Z_pcb_t);
      // thermal baffle around SHT40 island, open to the right wall
      translate([0,0,Z_pcb_t+baffle_gap]) linear_extrude(Z_in-Z_pcb_t-baffle_gap+0.01) difference(){
        translate([cx(baffle_x0),cy(island[3]+1.6)]) square([W-wall-cx(baffle_x0)+0.5, island[3]-island[1]+3.2]);
        translate([cx(baffle_xi),cy(island[3]+0.6)]) square([W, island[3]-island[1]+1.2]);
      }
      // front curtain, hot zone | panel. Sits near the board, notched up over each part.
      difference(){
        translate([cx(curtain_bx[0]), wall-0.01, Z_pcb_t+curtain_z_floor])
          cube([curtain_bx[1]-curtain_bx[0], H-2*wall+0.02, Z_in-Z_pcb_t-curtain_z_floor+0.01]);
        for (n = curtain_notches)
          translate([cx(curtain_bx[0])-1, cy(n[1]), Z_pcb_t-1])
            cube([curtain_bx[1]-curtain_bx[0]+2, n[1]-n[0], 1+n[2]]);
        // carrier rail crosses the curtain at the board's bottom edge. The rail plugs the lower part.
        translate([cx(curtain_bx[0])-0.4, cy(63.90), Z_pcb_t-1])
          cube([curtain_bx[1]-curtain_bx[0]+0.8, 63.90-62.90, 1+1.55]);
      }
      // front partition, buttons | sensor compartment. Notched over D3 and J2.
      difference(){
        translate([cx(part_bx[0]), wall-0.01, Z_pcb_t+part_z0])
          cube([part_bx[1]-part_bx[0], H-2*wall+0.02, Z_in-Z_pcb_t-part_z0+0.01]);
        translate([cx(part_bx[0])-1, cy(48.5), Z_pcb_t-1]) cube([5, 48.5-44.5, 1+part_d3_z]);
        translate([cx(part_bx[0])-1, cy(65), Z_pcb_t-1]) cube([5, 65-54.5, 1+part_j2_z]);
      }
      // button guide tubes. OD 8.6 stays: it overlaps the partition and keeps the 10 mm pitch.
      for(b=btn) translate([cx(b[0]),cy(b[1]),btn_tube_z0]) cylinder(d=btn_tube_od,h=Z_in-btn_tube_z0);
    }
    // e-paper window with 45deg outward chamfer
    translate([cx(aa[0])-win_m, cy(aa[3])-win_m, Z_in-0.5]) cube([aa[2]-aa[0]+2*win_m, aa[3]-aa[1]+2*win_m, 10]);
    hull(){
      translate([cx(aa[0])-win_m, cy(aa[3])-win_m, Z_in+0.85]) cube([aa[2]-aa[0]+2*win_m, aa[3]-aa[1]+2*win_m, 0.01]);
      translate([cx(aa[0])-win_m-1.6, cy(aa[3])-win_m-1.6, Z_out]) cube([aa[2]-aa[0]+2*win_m+3.2, aa[3]-aa[1]+2*win_m+3.2, 0.01]);
    }
    // panel pocket clearance (FPC loop side)
    translate([cx(panel[2]),cy(panel[3])+1,Z_pcb_t+0.5]) cube([4,panel[3]-panel[1]-2,Z_in-Z_pcb_t-0.5]);
    // land behind the glass, so a thick board does not press the panel into the face.
    // Stops at x 107.55: the button tubes (Ø8.6 at x 112) already occupy the last 0.9 mm of the outline.
    board_box(panel[0]-0.25, panel[1]-0.55, 107.55, panel[3]+0.55, Z_in-0.02, Z_in+face_recess);
    hook_relief();
    button_void();
    // RESET pinhole, LED light pipe
    for(p=pin) translate([cx(p[0]),cy(p[1]),Z_in-1]) cylinder(d=1.6,h=10);
    // no USB opening: USB-C is only reachable with the board out of the case (ground-loop hazard)
    // vents: front over island (outlet), right side wall, bottom wall below island
    for(i=[0:4]) translate([cx(island[0]+1.5+i*2.75),cy((island[1]+island[3])/2),Z_in-1]) linear_extrude(10) square([1.6,11],center=true);
    for(i=[0:3]) translate([W-wall-1, cy(island[1]+2+i*3.2)-0.8, Z_pcb_t+1]) cube([wall+2,1.6,Z_in-Z_pcb_t-2]);
    // hot-zone chimney: bottom in, top out, left wall low in / high out
    for (x = L_vx) for (y = [-1, H-wall-1]) translate([x-vslot_w/2, y, vslot_z[0]]) cube([vslot_w, wall+2, vslot_z[1]-vslot_z[0]]);
    for (y = L_sy) translate([-1, y-vslot_w/2, vslot_z[0]]) cube([wall+2, vslot_w, vslot_z[1]-vslot_z[0]]);
    // sensor duct: bottom in, top out, plus right-wall rear slots at the island band
    for (x = S_vx) for (y = [-1, H-wall-1]) translate([x-vslot_w/2, y, vslot_z[0]]) cube([vslot_w, wall+2, vslot_z[1]-vslot_z[0]]);
    for (y = S_sy) translate([W-wall-1, y-vslot_w/2, vslot_z[0]]) cube([wall+2, vslot_w, Z_pcb_b-1-vslot_z[0]]);
    // snap windows matching backplate bumps
    for(x=[W/2-35,W/2+35]) for(y=[-1,H-wall-1]) translate([x-5.5,y,3.2-1.0]) cube([11,wall+2,2.0]);
    // inlay pockets (filled by black text / orange cat bodies)
    if(pk) translate([0,0,Z_out-inlay_d]) linear_extrude(inlay_d+1) { marks2d(); cat_place2d(); swirls2d(); }
    // LED light pipe bore (filled by pink body)
    translate([cx(led[0]),cy(led[1]),Z_in-2]) cylinder(d=2.0,h=10);
  }
}

// ---------------- multi-colour bodies ----------------
pv_e = 0.01;   // preview-only: inlays grow 0.01 mm (up + sideways) to avoid z-fighting; STL exports use 0
module marks2d(){
  if(show_title) translate([cx(aa[0]),cy(aa[1])+3.0]) text("Connie's Thermostat",size=3.2,font="Liberation Sans:style=Bold");
  for(i=[0:2]) translate([cx(btn[i][0])-6.2,cy(btn[i][1])]) text(["+","\u2212","M"][i],size=3.6,halign="center",valign="center",font="Liberation Sans:style=Bold");
}
module cat_place2d(){ translate(cat_xy) scale(cat_s) cat2d(); }
// keep-out zones on the front face (2D, case XY)
module face_keepout2d(){
  offset(swirl_margin){
    translate([cx(aa[0])-win_m-1.6, cy(aa[3])-win_m-1.6]) square([aa[2]-aa[0]+2*win_m+3.2, aa[3]-aa[1]+2*win_m+3.2]);  // window incl. chamfer
    for(b=btn) translate([cx(b[0]),cy(b[1])]) circle(d=5.0);
    for(p=pin) translate([cx(p[0]),cy(p[1])]) circle(d=1.6);
    translate([cx(led[0]),cy(led[1])]) circle(d=2.0);
    marks2d(); cat_place2d();
    translate([cx(island[0]+1.5)-1,cy((island[1]+island[3])/2)-6.5]) square([2.75*4+3.6,13]);   // front vents
  }
}
module swirls2d(){
  if(swirl_style!="none") difference(){
    intersection(){ swirl_lines(swirl_style,swirl_w); translate([3,2.5]) square([W-6,H-5]); }
    face_keepout2d();
  }
}
module inlay_pink(e=0){ translate([0,0,Z_out-inlay_d]) linear_extrude(inlay_d+e) offset(delta=e) swirls2d(); }
module inlay_black(e=0){ translate([0,0,Z_out-inlay_d]) linear_extrude(inlay_d+e) offset(delta=e) marks2d(); }
module inlay_orange(e=0){ translate([0,0,Z_out-inlay_d]) linear_extrude(inlay_d+e) offset(delta=e) cat_place2d(); }
module light_pipe(){ translate([cx(led[0]),cy(led[1]),Z_pcb_t+1.0]) cylinder(d=1.95,h=Z_out-Z_pcb_t-1.0); }

// ---------------- buttons ----------------
// TS-1187A-B-A-B. Height code A is 1.5±0.05 mm. The drawing says TRAVEL 0.2±0.1 mm;
// the spec table says 0.25 mm. Force code B is 160 gf. Actuator Ø2.0, body 5.1 square, cover 1.2.
// The stem is Ø1.6 and meets the actuator only. A 65° cone seats in the Ø5 hole, so the cap cannot
// leave through the face. One straight leaf on the partition side carries the catch. The leaf is a
// vertical wall, 0.40 mm thick and 4.50 mm long, so a 0.30 mm bend to pass the lip is under 1% strain.
// Print head-down. The cone, the barb underside, and the root rib are 65° from horizontal. The leaf
// is anchored only at that rib: it is a cantilever, not a bridge between two walls. The print pose
// adds a breakaway tie from the cone up to the free end, so the leaf bottom spans rib to tie and
// the slicer can treat it as a bridge. Snap the tie off before the cap goes into the tube.
// pull is unused. The seated mesh has no tie.
sw_h = 1.50;
sw_top = Z_pcb_t + sw_h;
btn_proud = 0.80;
btn_head_d = 4.70;
btn_hole_d = 5.00;
btn_gap = 0.63;                  // seated stem to the actuator on a nominal board
btn_travel = 1.50;               // cone on the hole, to the catch face on the lip
btn_stem_d = 1.60;
btn_cone_d = 6.00;
btn_cone_ang = 65;               // from horizontal. Steeper than a 60° support threshold.
btn_tube_od = 8.60;
btn_tube_z0 = Z_pcb_t + 0.80;    // wall comes down beside the switch body (half-width 2.55 < bore r 3.85)
btn_bore_d = 7.70;               // r 3.85, 0.45 mm wall inside the Ø8.6 tube
btn_lip_d = 6.30;                // r 3.15
btn_flare = 0;                   // no separate print pose; the leaf is not pre-bent
btn_tip_z = Z_out + btn_proud;
btn_stem_z = sw_top + btn_gap;
btn_shelf_z = Z_pcb_t + 1.90;    // lip face the catch reaches after btn_travel. 20.5 at the nominal stack.
leaf_t = 0.40;
leaf_L = 4.50;                   // wall length. The stem rib clamps the first leaf_root of it.
leaf_root = 0.15;
leaf_defl = 0.30;                // tip moves this far in x to clear the lip
leaf_free = leaf_L - leaf_root;
leaf_eps = 1.5 * leaf_t * leaf_defl / (leaf_free * leaf_free);
echo(str("BTN gap=", btn_gap, " travel=", btn_travel, " stroke=", btn_travel - btn_gap, " leaf_strain=", leaf_eps));
assert(btn_gap >= 0.60 && btn_gap <= 0.70, "button gap is outside 0.60..0.70");
assert(btn_travel >= 1.45 && btn_travel <= 1.55, "button travel is outside 1.45..1.55");
assert(leaf_free >= 4.0, "button leaf is shorter than 4 mm");
assert(leaf_eps < 0.020, "button leaf strain is past the PETG limit");

function zl(zw) = btn_tip_z - zw;

// Straight vertical wall, 0.40 mm in x. Root at the -y end. The rib is a 65° web from the
// stem to that root and stays inside the lip, so only the barb crosses the lip. The barb
// underside is the same 65°. tie adds a nub under the free end for the print only.
module btn_catch(tie=false) {
  x_in = 1.65;
  x_out = x_in + leaf_t;
  y0 = -2.25;
  y1 = y0 + leaf_L;
  z_face = zl(btn_shelf_z + btn_travel);   // board face of the catch. Pressed, this lands on the lip.
  z_bot = z_face - 1.05;                   // above the neck collar, so the collar does not brace the leaf
  translate([x_in, y0, z_bot]) cube([leaf_t, leaf_L, z_face - z_bot]);
  x_root = x_in + leaf_t/2;
  y_root = y0 + leaf_root/2;
  // Outer corner of the root. The rib's shallowest face is the line from that
  // corner down to a pad inside the head, at 72° from horizontal, so a 60°
  // support threshold does not touch it. The old flared hull was shallower.
  r_out = sqrt(x_out*x_out + y0*y0);
  rib_ang = 72;
  run = z_bot / tan(rib_ang);
  ux = x_root / sqrt(x_root*x_root + y_root*y_root);
  uy = y_root / sqrt(x_root*x_root + y_root*y_root);
  // Pad far enough in that the outer corner, not the centre, is the 72° edge.
  inset = run + (r_out - sqrt(x_root*x_root + y_root*y_root));
  hull() {
    translate([x_root - ux*inset, y_root - uy*inset, 0.15]) cube([0.40, 0.40, 0.30], center=true);
    translate([x_in, y0, z_bot]) cube([leaf_t, leaf_root, z_face - z_bot]);
  }
  x_tip = 2.50;                            // 0.30 mm past the lip's x at y = y1
  dz = (x_tip - x_out) * tan(btn_cone_ang);
  yb = y1 - 0.35;
  translate([0, y1, 0]) rotate([90, 0, 0]) linear_extrude(y1 - yb) polygon([
    [x_out, z_bot],
    [x_out, z_face],
    [x_tip, z_face],
    [x_out, z_face - dz]
  ]);
  // Print tie. A pillar on the bed under the free end, overlapping the whole
  // leaf bottom there, so that bottom is a bridge from the root rib to the pillar.
  // Anything of the pillar that lands inside the head or the cone just joins them.
  // Snap off the part that stands in the air before the cap goes into the tube.
  if (tie)
    translate([x_in - 0.10, y1 - 0.90, 0]) cube([leaf_t + 0.20, 1.05, z_bot + 0.06]);
}

module cap(pull=0, drop=0, tie=false) {
  // 65° from horizontal, so the cone reaches the Ø5 hole's inner corner.
  cone_seat = (btn_hole_d - btn_head_d)/2 * tan(btn_cone_ang);
  cone0 = zl(Z_in + cone_seat);
  cone_h = (btn_cone_d - btn_head_d)/2 * tan(btn_cone_ang);
  cylinder(d=btn_head_d, h=cone0);
  translate([0, 0, cone0]) cylinder(d1=btn_head_d, d2=btn_cone_d, h=cone_h);
  // Collar stays clear of the leaf. The leaf is not fused to it.
  translate([0, 0, cone0 + cone_h - 0.40]) cylinder(d=4.4, h=0.20);
  translate([0, 0, cone0 + cone_h - 0.02]) cylinder(d=btn_stem_d, h=zl(btn_stem_z) - (cone0 + cone_h - 0.02));
  btn_catch(tie);
}

// drop>0 is a press: the body moves toward the board until the catch face meets the lip.
module cap_placed(pull=0, drop=0) {
  for (b = btn)
    translate([cx(b[0]), cy(b[1]), btn_tip_z - drop])
      rotate([180, 0, 0])
        cap(pull, drop);
}

// Face hole, upper bore, lip, and the lead-in from the tube mouth. The FPC pocket
// cuts the panel side of the bore. The catch sits on the partition side, where the lip remains.
// The annulus the catch lands on runs from r 3.15 out to r 3.75 (lip + 1.2 mm).
module button_void() {
  neck_h = 0.35;
  lead_h = 0.45;
  ramp_h = 0.55;
  lip_open = btn_lip_d + 1.20;     // r 3.75 at the shelf, so the  r~3.36 catch has a face to hit
  for (b = btn) translate([cx(b[0]), cy(b[1]), 0]) {
    translate([0, 0, Z_in - 0.01]) cylinder(d=btn_hole_d, h=Z_out - Z_in + 2);
    translate([0, 0, btn_shelf_z + ramp_h]) cylinder(d=btn_bore_d, h=Z_in - btn_shelf_z - ramp_h + 0.02);
    translate([0, 0, btn_shelf_z]) cylinder(d1=lip_open, d2=btn_bore_d, h=ramp_h);
    translate([0, 0, btn_shelf_z - neck_h]) cylinder(d=btn_lip_d, h=neck_h);
    translate([0, 0, btn_shelf_z - neck_h - lead_h]) cylinder(d1=btn_bore_d, d2=btn_lip_d, h=lead_h);
    translate([0, 0, btn_tube_z0 - 0.02]) cylinder(d=btn_bore_d, h=btn_shelf_z - neck_h - lead_h - btn_tube_z0 + 0.06);
  }
}

// ---------------- panel carrier ----------------
// One PETG piece. Feet on the PCB, hooks up. The panel back lands at panel_gap.
// dir = +1 puts the beam on the small-y side of the edge (the top of the panel).
// The beam stands in the air beside the glass. Only cl_foot at the bottom bridges to the pad,
// so the free length is Z_in - (Z_pcb_t + cl_foot). The 45° lead-in is above the shelf.
// defl bends the beam outward about the foot, for the removal pose. The slot is the
// place a flat tool sits to hold that bend; it is cut only on the unbent beam.
module edge_hook(x0, x1, edge, dir, defl=0) {
  u_out = -(cl_gap + cl_t);
  u_beam = -cl_gap;
  z_foot = Z_pcb_t + cl_foot;
  z_top = Z_in + cl_lip_h + cl_lip;
  uz = [
    [u_out, Z_pcb_t],
    [u_beam, Z_pcb_t],
    [u_beam, z_foot],
    [u_beam - defl, Z_in],
    [cl_lip - defl, Z_in],
    [cl_lip - defl, Z_in + cl_lip_h],
    [-defl, z_top],
    [u_out - defl, z_top],
    [u_out, z_foot]
  ];
  // polygon x = -z, y = case y. rotate([0,90,0]) sends extrude +z to world +x.
  difference() {
    translate([cx(x0), 0, 0]) rotate([0, 90, 0]) linear_extrude(x1 - x0)
      polygon([for (p = uz) [-p[1], cy(edge) - dir * p[0]]]);
    if (defl == 0) {
      y_outer = cy(edge) - dir * u_out;
      y_in = y_outer - dir * cl_slot_d;
      y_out = y_outer + dir * 0.40;
      translate([cx((x0+x1)/2 - cl_slot_w/2), min(y_in, y_out), Z_in - cl_slot_h/2])
        cube([cl_slot_w, abs(y_out - y_in), cl_slot_h]);
    }
  }
}
module hook_foot(x0, x1, edge, dir) {
  u0 = -(cl_gap + cl_t);
  u1 = 0.50;
  y0 = edge + dir * u0;
  y1 = edge + dir * u1;
  board_box(x0, min(y0, y1), x1, max(y0, y1), Z_pcb_t, Z_pcb_t + cl_foot);
}
module support_pad(x0, x1, edge, dir, depth) {
  // Inboard of the glass edge only. The beam's y is outboard, with cl_gap of air between.
  y_far = edge + dir * depth;
  board_box(x0, min(edge, y_far), x1, max(edge, y_far), Z_pcb_t, Z_panel_b);
}
module hook_pocket(x0, x1, edge, dir) {
  y_out = edge - dir*(cl_gap + cl_t);
  y_tip = edge + dir*cl_lip;
  board_box(x0 - 0.45, min(y_out, y_tip) - 0.45, x1 + 0.45, max(y_out, y_tip) + 0.45, Z_in - 0.05, Z_in + cl_pocket);
}
module hook_relief() {
  for (c = [[47.0, 53.0], [84.6, 90.6]]) hook_pocket(c[0], c[1], panel[1], +1);
  for (c = [[49.6, 55.6], [76.0, 82.0]]) hook_pocket(c[0], c[1], panel[3], -1);
}
// Posts on the cable side of the glass. +x is the slide that shortens the tail in J4.
// Each post bites xstop_bite onto the outline and stands in the shell's FPC pocket.
// The bands miss the button tubes (they meet the edge near y 30, 40, and 50) and
// the bend bulge (y 37.75–50.25). -x runs into the curtain, 0.46 mm away, and that
// direction lengthens the inserted tail.
module glass_xstops() {
  z1 = Z_in - 0.45;   // 0.45 mm under the inner face, so a +0.16 mm board still leaves >0.15 mm
  board_box(82.0, 33.20, panel[2], 34.30, Z_pcb_t, Z_panel_b);
  board_box(panel[2] - xstop_bite, 33.20, panel[2] + 0.80, 34.30, Z_panel_b - 0.40, z1);
  board_box(76.0, 61.20, panel[2], 62.50, Z_pcb_t, Z_panel_b);
  board_box(panel[2] - xstop_bite, 60.40, panel[2] + 0.80, 61.70, Z_panel_b - 0.40, z1);
}
module carrier(open_top=0) {
  z0 = Z_pcb_t;
  // left edge fence, 0.25 mm off the board, tied to the spine so the nozzle keeps the gap
  board_box(-0.80, 42.0, -0.25, 60.0, z0, z0 + 1.15);
  board_box(0.40, 40.0, 1.80, 63.55, z0, z0 + 1.20);
  for (y = [44.0, 51.0, 57.2])
    board_box(-0.80, y, 1.70, y + 1.8, z0 + 0.55, z0 + 1.15);
  // bottom rail. The middle span is the curtain crossing, held to 1.00 mm.
  board_box(0.35, 63.15, 27.25, 63.60, z0, z0 + 1.50);
  board_box(27.15, 63.15, 29.55, 63.60, z0, z0 + 1.00);
  // Full-height rail stops at each bottom hook. The beam sits outboard of y 63.19,
  // and only the foot crosses that gap.
  board_box(29.45, 63.15, 49.45, 63.60, z0, z0 + 1.50);
  board_box(55.75, 63.15, 75.85, 63.60, z0, z0 + 1.50);
  board_box(82.15, 63.15, 97.0, 63.60, z0, z0 + 1.50);
  board_box(49.30, 63.15, 49.75, 63.60, z0, z0 + cl_foot);
  board_box(55.45, 63.15, 55.90, 63.60, z0, z0 + cl_foot);
  board_box(75.70, 63.15, 76.15, 63.60, z0, z0 + cl_foot);
  board_box(81.85, 63.15, 82.30, 63.60, z0, z0 + cl_foot);
  board_box(36.0, 64.22, 96.0, 64.78, z0, z0 + 1.15);
  for (x = [40.0, 62.0, 88.0])
    board_box(x, 63.40, x + 2.2, 64.78, z0 + 0.55, z0 + 1.15);
  // top-left hook down to the jog, then east of C42, then between the C43 leads
  board_box(49.00, 27.50, 51.40, 50.50, z0, z0 + 1.50);
  board_box(48.95, 49.50, 54.70, 52.50, z0, z0 + 1.50);
  board_box(52.20, 49.90, 54.60, 56.80, z0, z0 + 1.50);
  board_box(52.85, 56.60, 53.70, 59.40, z0, z0 + 1.50);
  board_box(49.55, 59.15, 55.65, 62.90, z0, z0 + 1.50);
  // right column, passing to the right of J1 pin 5, up under the glass edge
  board_box(76.00, 27.60, 82.00, 56.55, z0, z0 + 1.50);
  board_box(78.55, 56.35, 79.90, 59.50, z0, z0 + 1.50);
  board_box(75.95, 59.25, 82.05, 62.90, z0, z0 + 1.50);
  support_pad(47.0, 53.0, panel[1], +1, 1.65);
  hook_foot(47.0, 53.0, panel[1], +1);
  edge_hook(47.0, 53.0, panel[1], +1, open_top);
  support_pad(76.0, 90.6, panel[1], +1, 1.65);
  hook_foot(84.6, 90.6, panel[1], +1);
  edge_hook(84.6, 90.6, panel[1], +1, open_top);
  support_pad(49.6, 55.6, panel[3], -1, 1.65);
  hook_foot(49.6, 55.6, panel[3], -1);
  edge_hook(49.6, 55.6, panel[3], -1, 0);
  support_pad(76.0, 82.0, panel[3], -1, 1.65);
  hook_foot(76.0, 82.0, panel[3], -1);
  edge_hook(76.0, 82.0, panel[3], -1, 0);
  glass_xstops();
}
// Shell pockets over the four ledges. The beams and the ledges stand proud of the inner face.
module fpc() {
  zc = Z_pcb_t + fpc_z_ret + fpc_R;
  y0 = 44 - fpc_wide/2;
  y1 = 44 + fpc_wide/2;
  x_end = mouth_x - fpc_ins;
  color("goldenrod") {
    board_box(x_end, y0, panel[2], y1, Z_pcb_t + fpc_z_ret - fpc_thick/2, Z_pcb_t + fpc_z_ret + fpc_thick/2);
    // semicircle bulging to +x. rotate(-90) about X sends local +Z to case +Y and local +Y to case -Z,
    // so the circle centre is placed at local y = -zc.
    translate([0, cy(y1), 0]) rotate([-90, 0, 0]) linear_extrude(fpc_wide)
      translate([cx(panel[2]), -zc]) difference() {
        circle(r = fpc_R + fpc_thick/2);
        circle(r = max(fpc_R - fpc_thick/2, 0.2));
        translate([-fpc_R - 2, -(fpc_R + 1)]) square([fpc_R + 2, 2*fpc_R + 2]);
      }
  }
}

// ---------------- reference geometry (for renders) ----------------
module pcb(){ color("darkgreen") translate([OX,cy(BH),Z_pcb_b]) difference(){ cube([BW,BH,pcb_t]); for(h=holes) translate([h[0],BH-h[1],-1]) cylinder(d=4.3,h=5);} 
  color("silver") translate([cx(66),cy(25.5),Z_pcb_t]) cube([18,25.5,3.1]);                 // ESP32-C6
  color("black") for(b=btn) translate([cx(b[0])-3,cy(b[1])-3,Z_pcb_t]) cube([6,6,1.5]);
  color("green") translate([cx(term[0])-9.15,cy(term[1])-1.2,Z_pcb_b-7.6]) cube([18.3,9.2,7.6]);           // rear RA header 15EDGRC
  color("limegreen") translate([cx(term[0])-plug_w/2,cy(plug_y1),Z_pcb_b-plug_h]) cube([plug_w,plug_y1-plug_y0,plug_h]);      // 15EDGK plug (wire entry faces the hole)
  color("darkred") for(i=[0:4]) hull(){ translate([cx(term[0])-7+i*3.5,cy(plug_y0)+3,Z_pcb_b-5.5]) sphere(d=1.2); translate([W/2-8+i*4,H/2,-2]) sphere(d=1.2);} // wires
  color("gray") translate([cx(usb_x)-4.5,cy(BH),Z_pcb_t]) cube([9,7.3,3.2]);                    // USB-C flush with board edge
  color("black") translate([cx(98.5),cy(53),Z_pcb_t]) cube([103.9-98.5,53-34.8,2.0]);             // J4, mouth at x 103.77, H=2.0
  color("royalblue") translate([cx(bulk[0]),cy(bulk[1]),Z_pcb_b-bulk_h]) cylinder(d=8,h=bulk_h);       // C43 47uF/63V THT, back side
  color("orange") translate([cx(127),cy(32.7),Z_pcb_t]) cube([1.5,1.5,0.5]);                 // SHT40
}
module epd(){ color("white") translate([cx(panel[0]),cy(panel[3]),Z_panel_b]) cube([panel[2]-panel[0],panel[3]-panel[1],panel_t]);
  color([0.2,0.2,0.2]) translate([cx(aa[0]),cy(aa[3]),Z_in-0.05]) cube([aa[2]-aa[0],aa[3]-aa[1],0.1]); }

module flipP(){ translate([0,H,Z_out]) rotate([180,0,0]) children(); }
if(part=="backplate") backplate();
else if(part=="shell_white") flipP() shell();
else if(part=="shell_black") flipP() inlay_black();
else if(part=="shell_orange") flipP() inlay_orange();
else if(part=="shell_pink") flipP() { light_pipe(); inlay_pink(); }
else if(part=="shell") translate([0,H,Z_out]) rotate([180,0,0]) shell();   // print face-down
else if(part=="carrier") carrier();
else if(part=="caps") for(i=[0:2]) translate([i*14,0,0]) cap(0, 0, true);
else if(part=="assembly"){ color("lightgray") backplate(); pcb(); color("wheat") carrier(); epd(); fpc(); color("white") shell(false); color("black") inlay_black(pv_e); color("orange") inlay_orange(pv_e); color("hotpink") light_pipe(); color("hotpink") inlay_pink(pv_e); color("orange") cap_placed(btn_flare); }
else if(part=="exploded"){ color("lightgray") backplate(); translate([0,0,22]){ pcb(); color("wheat") carrier(); epd(); fpc(); } translate([0,0,64]){ color("white") shell(false); color("black") inlay_black(pv_e); color("orange") inlay_orange(pv_e); color("hotpink") light_pipe(); color("hotpink") inlay_pink(pv_e); color("orange") cap_placed(btn_flare); } }
else if(part=="section") difference(){ union(){ color("lightgray") backplate(); pcb(); color("wheat") carrier(); epd(); fpc(); color("white") shell(false); color("black") inlay_black(pv_e); color("orange") inlay_orange(pv_e); color("hotpink") light_pipe(); color("hotpink") inlay_pink(pv_e); color("orange") cap_placed(btn_flare); }
  translate([-1,-1,-30]) cube([W+2, cy(40), 80]); }   // cut through the DOWN button centre (board y=40), the panel, and the FPC
else if(part=="section_hooks") difference(){ union(){ color("lightgray") backplate(); pcb(); color("wheat") carrier(); epd(); fpc(); color("white") shell(false); color("orange") cap_placed(btn_flare); }
  translate([-1,-1,-30]) cube([W+2, cy(panel[1]+1.3)+1, 80]); }   // cut through the top hooks and the support pads
// Removal. The shell is already off. The cut is the middle of the first top hook (board x = 50), through the tool slot.
// section_release_out holds that edge's two hooks out by cl_lip and slides the glass toward them.
else if(part=="section_release") difference(){ union(){ color("lightgray") backplate(); pcb(); color("wheat") carrier(); epd(); fpc(); }
  translate([cx(50), -1, -30]) cube([W, H+2, 80]); }
else if(part=="section_release_out") difference(){ union(){ color("lightgray") backplate(); pcb(); color("wheat") carrier(cl_lip); translate([0, cl_lip + 0.08, 0]) epd(); fpc(); }
  translate([cx(50), -1, -30]) cube([W, H+2, 80]); }
else if(part=="section_j1") difference(){ union(){ color("lightgray") backplate(); pcb(); color("wheat") carrier(); epd(); color("white") shell(false); }
  translate([-1,-1,-30]) cube([W+2, cy(58)+1, 80]); }   // cut along the J1 pin row / C43 centre (board y=58)
else if(part=="section_j1x") difference(){ union(){ color("lightgray") backplate(); pcb(); color("wheat") carrier(); epd(); color("white") shell(false); }
  translate([cx(term[0]),-1,-30]) cube([W, H+2, 80]); }   // cut across J1 + plug + wire bend (board x=70)
else if(part=="section_airflow") difference(){ union(){ color("lightgray") backplate(); pcb(); color("wheat") carrier(); epd(); color("white") shell(false); color("black") inlay_black(pv_e); color("orange") inlay_orange(pv_e); color("hotpink") light_pipe(); color("hotpink") inlay_pink(pv_e); color("orange") cap_placed(btn_flare); }
  translate([-1, cy(32), -30]) cube([W+2, H, 80]); }   // cut at board y=32, through the SHT40 and both zone ribs
// rear clearance check: tallest rear parts must clear the backplate top (plate_t)
assert(Z_pcb_b-bulk_h>plate_t+1, "C43 hits backplate");
assert(Z_pcb_b-plug_h>plate_t-plug_pocket+1, "J1 plug hits backplate");
// all parts laid out for a 256x256 bed (Bambu P2S), print orientation, no supports
if(part=="plate"){
  translate([5,5,0]) translate([0,H,Z_out]) rotate([180,0,0]) shell();
  translate([5,H+12-(wall+clr),0]) backplate();
  for(i=[0:2]) translate([168+ (i*14), 22, 0]) cap(0, 0, true);
  translate([152, 168, -Z_pcb_t]) carrier();
}
