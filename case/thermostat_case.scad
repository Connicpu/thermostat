// Connie's Super Awesome Thermostat V3 - parametric case (OpenSCAD)
// Units mm. Board coords: origin top-left, x right, y down (same as KiCad, board-relative).
// Case coords: origin outer bottom-left of backplate, X right, Y up, Z out of wall (wall surface Z=0).
// Three air zones: hot, middle, and sensor duct.
part = "assembly";   // backplate | shell | caps | assembly | exploded | section | section_airflow
explode = 0;         // used by "exploded"
$fn = 48;

// ---------------- parameters ----------------
W = 147; H = 71;            // outer footprint (drywall-anchor version); 1.1 mm board-edge gap all round
wall = 2.4;                 // shell side walls (>=2 mm)
clr = 0.3;                  // fit clearance
plate_t = 3.0;              // backplate thickness
standoff_h = 14.0;          // backplate face -> PCB back, so the 11.1 mm 15EDGK plug and 9.5 mm C43 clear a flat plate and lip
pcb_t = 1.6;
panel_gap = 5.0;            // PCB top -> panel back (allowed 4.6..5.2). The FPC fold radius is about 2.15 mm, and L1 sits beside the panel
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
vslot_z = [6.5, 23.8];               // above the 5.5 mm lip, 1.0 mm short of the front skin
ribL_bx = [26.8, 28.4];              // rear divider, hot zone | middle
// Rib S is at board x 119.0 so the Ø0.65 J2 peg at board x 118.11 keeps >= 0.5 mm.
// A notch over the peg would open the duct at the inlet.
// The locating pins are Ø3.9 in Ø4.3 holes, so the board can still shift ±0.2 mm.
// Rib S (board x119.0) does not line up with the front partition, which ends at board x117.8.
// That 1.2 mm offset times the 1.1 mm board-edge gap is an ~1.3 mm² leak around the edge,
// between the sensor duct and the middle zone.
ribS_bx = [119.0, 120.6];            // rear divider, middle | sensor duct
rib_gap = 0.4;                       // rib top -> PCB back. Never let the rib touch the board.
rib_end_y = 2.7;                     // lip outer face; caps the rib out to here above the lip
curtain_bx = [27.7, 29.0];           // front curtain. Overlaps the panel's left rib (fuses a 0.26 mm sliver).
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
      // panel locating ribs (3 sides; FPC side open)
      translate([cx(panel[0])-1.2-clr,cy(panel[3])-clr,Z_in-1]) cube([1.2,panel[3]-panel[1]+2*clr,1.01]);
      for(y=[panel[1]-clr-1.2,panel[3]+clr]) translate([cx(panel[0])-clr-0.6, cy(y)-1.2, Z_in-1]) cube([panel[2]-panel[0]-2+0.6,1.2,1.01]);  // overlaps end rib (no edge-only contact)
      // panel retaining hooks (2 per long edge) - flex 1.2 mm beams, 0.7 mm lip
      for(x=[50,88]) for(e=[0,1]) {
        yy = e==0 ? panel[1]-clr : panel[3]+clr;
        translate([cx(x),cy(yy),0]) rotate([0,0,e==0?0:180]) translate([-3,0,0]) {
          translate([0,0,Z_panel_b-0.9]) cube([6,1.2,Z_in-Z_panel_b+0.9+0.01]);
          translate([0,0,Z_panel_b-0.9]) rotate([0,90,0]) linear_extrude(6) polygon([[0,0],[-0.9,0],[0,-0.7]]); }
      }
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
      }
      // front partition, buttons | sensor compartment. Notched over D3 and J2.
      difference(){
        translate([cx(part_bx[0]), wall-0.01, Z_pcb_t+part_z0])
          cube([part_bx[1]-part_bx[0], H-2*wall+0.02, Z_in-Z_pcb_t-part_z0+0.01]);
        translate([cx(part_bx[0])-1, cy(48.5), Z_pcb_t-1]) cube([5, 48.5-44.5, 1+part_d3_z]);
        translate([cx(part_bx[0])-1, cy(65), Z_pcb_t-1]) cube([5, 65-54.5, 1+part_j2_z]);
      }
      // button guide tubes
      for(b=btn) translate([cx(b[0]),cy(b[1]),Z_pcb_t+3.5]) cylinder(d=8.6,h=Z_in-Z_pcb_t-3.5);
    }
    // e-paper window with 45deg outward chamfer
    translate([cx(aa[0])-win_m, cy(aa[3])-win_m, Z_in-0.5]) cube([aa[2]-aa[0]+2*win_m, aa[3]-aa[1]+2*win_m, 10]);
    hull(){
      translate([cx(aa[0])-win_m, cy(aa[3])-win_m, Z_in+0.4]) cube([aa[2]-aa[0]+2*win_m, aa[3]-aa[1]+2*win_m, 0.01]);
      translate([cx(aa[0])-win_m-1.6, cy(aa[3])-win_m-1.6, Z_out]) cube([aa[2]-aa[0]+2*win_m+3.2, aa[3]-aa[1]+2*win_m+3.2, 0.01]);
    }
    // panel pocket clearance (FPC loop side)
    translate([cx(panel[2]),cy(panel[3])+1,Z_pcb_t+0.5]) cube([4,panel[3]-panel[1]-2,Z_in-Z_pcb_t-0.5]);
    // buttons
    for(b=btn) translate([cx(b[0]),cy(b[1]),Z_pcb_t]) { cylinder(d=5.0,h=50); cylinder(d=7.9,h=Z_in-Z_pcb_t-0.01); }
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

// ---------------- button caps (print 3, flange down) ----------------
sw_top = Z_pcb_t + 1.5;   // TS-1187A H1.5 actuator
module cap(){ cylinder(d=7.4,h=Z_in-0.3-sw_top); cylinder(d=4.6,h=Z_out+0.8-sw_top); }

// ---------------- reference geometry (for renders) ----------------
module pcb(){ color("darkgreen") translate([OX,cy(BH),Z_pcb_b]) difference(){ cube([BW,BH,pcb_t]); for(h=holes) translate([h[0],BH-h[1],-1]) cylinder(d=4.3,h=5);} 
  color("silver") translate([cx(66),cy(25.5),Z_pcb_t]) cube([18,25.5,3.1]);                 // ESP32-C6
  color("black") for(b=btn) translate([cx(b[0])-3,cy(b[1])-3,Z_pcb_t]) cube([6,6,1.5]);
  color("green") translate([cx(term[0])-9.15,cy(term[1])-1.2,Z_pcb_b-7.6]) cube([18.3,9.2,7.6]);           // rear RA header 15EDGRC
  color("limegreen") translate([cx(term[0])-plug_w/2,cy(plug_y1),Z_pcb_b-plug_h]) cube([plug_w,plug_y1-plug_y0,plug_h]);      // 15EDGK plug (wire entry faces the hole)
  color("darkred") for(i=[0:4]) hull(){ translate([cx(term[0])-7+i*3.5,cy(plug_y0)+3,Z_pcb_b-5.5]) sphere(d=1.2); translate([W/2-8+i*4,H/2,-2]) sphere(d=1.2);} // wires
  color("gray") translate([cx(usb_x)-4.5,cy(BH),Z_pcb_t]) cube([9,7.3,3.2]);                    // USB-C flush with board edge
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
else if(part=="caps") for(i=[0:2]) translate([i*10,0,0]) cap();
else if(part=="assembly"){ color("lightgray") backplate(); pcb(); epd(); color("white") shell(false); color("black") inlay_black(pv_e); color("orange") inlay_orange(pv_e); color("hotpink") light_pipe(); color("hotpink") inlay_pink(pv_e); for(b=btn) color("orange") translate([cx(b[0]),cy(b[1]),sw_top]) cap(); }
else if(part=="exploded"){ color("lightgray") backplate(); translate([0,0,25]){ pcb(); } translate([0,0,40]) epd(); translate([0,0,75]){ color("white") shell(false); color("black") inlay_black(pv_e); color("orange") inlay_orange(pv_e); color("hotpink") light_pipe(); color("hotpink") inlay_pink(pv_e); } for(b=btn) color("orange") translate([cx(b[0]),cy(b[1]),sw_top+60]) cap(); }
else if(part=="section") difference(){ union(){ color("lightgray") backplate(); pcb(); epd(); color("white") shell(false); color("black") inlay_black(pv_e); color("orange") inlay_orange(pv_e); color("hotpink") light_pipe(); color("hotpink") inlay_pink(pv_e); for(b=btn) color("orange") translate([cx(b[0]),cy(b[1]),sw_top]) cap(); }
  translate([-1,-1,-30]) cube([W+2, cy(44)+1, 80]); }   // cut at board y=44 (through panel, FPC, DOWN button)
else if(part=="section_j1") difference(){ union(){ color("lightgray") backplate(); pcb(); epd(); color("white") shell(false); }
  translate([-1,-1,-30]) cube([W+2, cy(58)+1, 80]); }   // cut along the J1 pin row / C43 centre (board y=58)
else if(part=="section_j1x") difference(){ union(){ color("lightgray") backplate(); pcb(); epd(); color("white") shell(false); }
  translate([cx(term[0]),-1,-30]) cube([W, H+2, 80]); }   // cut across J1 + plug + wire bend (board x=70)
else if(part=="section_airflow") difference(){ union(){ color("lightgray") backplate(); pcb(); epd(); color("white") shell(false); color("black") inlay_black(pv_e); color("orange") inlay_orange(pv_e); color("hotpink") light_pipe(); color("hotpink") inlay_pink(pv_e); for(b=btn) color("orange") translate([cx(b[0]),cy(b[1]),sw_top]) cap(); }
  translate([-1, cy(32), -30]) cube([W+2, H, 80]); }   // cut at board y=32, through the SHT40 and both zone ribs
// rear clearance check: tallest rear parts must clear the backplate top (plate_t)
assert(Z_pcb_b-bulk_h>plate_t+1, "C43 hits backplate");
assert(Z_pcb_b-plug_h>plate_t-plug_pocket+1, "J1 plug hits backplate");
// all parts laid out for a 256x256 bed (Bambu P2S), print orientation, no supports
if(part=="plate"){
  translate([5,5,0]) translate([0,H,Z_out]) rotate([180,0,0]) shell();
  translate([5,H+12-(wall+clr),0]) backplate();
  for(i=[0:2]) translate([200+ (i*12), 30, 0]) cap();
}
