// Connie's Super Awesome Thermostat V3 - parametric case (OpenSCAD)
// Units mm. Board coords: origin top-left, x right, y down (same as KiCad, board-relative).
// Case coords: origin outer bottom-left of backplate, X right, Y up, Z out of wall (wall surface Z=0).
part = "assembly";   // backplate | shell | caps | assembly | exploded | section
explode = 0;         // used by "exploded"
$fn = 48;

// ---------------- parameters ----------------
W = 150; H = 96;            // outer footprint
wall = 2.4;                 // shell side walls (>=2 mm)
clr = 0.3;                  // fit clearance
plate_t = 3.0;              // backplate thickness
standoff_h = 4.0;           // backplate face -> PCB back
pcb_t = 1.6;
panel_gap = 5.0;            // PCB top -> panel back (allowed 4.6..5.2, FPC fold)
panel_t = 1.2;
front_t = 2.0;              // front face thickness
BW = 140; BH = 64;          // PCB
OX = 5;                     // board left edge in case X
OYT = 17;                   // board top edge distance from case top
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
term = [9,31];                       // rear HVAC header/plug centre
panel = [29.46,26.24,108.46,62.94];  // GDEY029T94 outline (x0,y0,x1,y1)
aa    = [32.46,30.06,99.36,59.12];   // active area (3.0/3.82 mm margins, FPC end on right)
win_m = 0.5;                         // window margin around active area
btn = [[112,30],[112,40],[112,50]];  // UP DOWN MODE
pin = [[91,5],[100,5]];              // RESET, BOOT pinholes
led = [63.3,21.2];                   // D5 status LED -> light-pipe hole
usb_x = 121;                         // USB-C centre x (mouth faces board bottom edge)
island = [121,25,135,39];            // SHT40 thermal island
gang = 83.3;                         // single-gang box screw spacing (vertical)

// ---------------- helpers ----------------
module rrect(w,h,r){ offset(r) square([w-2*r,h-2*r]); }
module rbox(w,h,d,r){ linear_extrude(d) translate([r,r]) rrect(w,h,r); }
module slot2d(l,w){ hull(){ translate([-l/2+w/2,0]) circle(d=w); translate([l/2-w/2,0]) circle(d=w);} }

// ---------------- backplate ----------------
pw = W-2*wall-2*clr; ph = H-2*wall-2*clr; po = wall+clr;
lip_h = 6; lip_t = 2;
module backplate(){
  difference(){
    union(){
      translate([po,po,0]) rbox(pw,ph,plate_t,3);
      // perimeter lip that the shell slides over
      translate([po,po,0]) difference(){ rbox(pw,ph,lip_h,3); translate([lip_t,lip_t,-1]) rbox(pw-2*lip_t,ph-2*lip_t,lip_h+2,1.5); }
      // snap bumps (45 deg both sides, print without supports)
      for(s=[[W/2-35,po,0],[W/2+35,po,0],[W/2-35,H-po,180],[W/2+35,H-po,180]])
        translate([s[0],s[1],3.2]) rotate([0,0,s[2]]) rotate([0,90,0]) linear_extrude(10,center=true) polygon([[0.9,0],[0,-0.8],[-0.9,0]]);
      // PCB standoffs with locating pins
      for(h=holes) translate([cx(h[0]),cy(h[1]),0]){ cylinder(d=8,h=Z_pcb_b); cylinder(d=3.9,h=Z_pcb_b+pcb_t-0.2); }
    }
    // wire pass-through aligned with rear terminal/plug (plug pokes into wall box)
    translate([cx(term[0]),cy(term[1]),-1]) linear_extrude(plate_t+2) offset(2) square([14-4,25-4],center=true);
    // single-gang box screw slots (83.3 mm), centred on wire hole, slotted +-3 mm
    for(d=[-gang/2,gang/2]) translate([cx(term[0]),cy(term[1])+d,-1]){
      linear_extrude(plate_t+2) slot2d(10,4.2);
      translate([0,0,plate_t+1-1.6]) linear_extrude(1.7) slot2d(13,8.4); }  // head recess
    // drywall anchor holes (#6/#8 screw, 4.5 mm)
    for(p=[[50,10],[110,10],[80,86],[140,48]]) translate([p[0],p[1],-1]){ cylinder(d=4.5,h=plate_t+2); translate([0,0,plate_t+1-1.6]) cylinder(d=8.4,h=1.7); }
    // vents under the SHT40 island (chimney inlet)
    for(i=[0:4]) translate([cx(island[0]+1.5+i*2.75),cy((island[1]+island[3])/2),-1]) linear_extrude(plate_t+2) square([1.6,12],center=true);
    // label recess
    translate([W/2,6,plate_t-0.6]) linear_extrude(1) text("TOP ^  V3",size=4,halign="center");
  }
}

// ---------------- front shell ----------------
module shell(){
  difference(){
    union(){
      difference(){
        rbox(W,H,Z_out,4);
        translate([wall,wall,-1]) rbox(W-2*wall,H-2*wall,Z_in+1,2);
      }
      // PCB clamp posts at mounting holes
      for(h=holes) translate([cx(h[0]),cy(h[1]),Z_pcb_t+0.1]) cylinder(d=6,h=Z_in-Z_pcb_t);
      // panel locating ribs (3 sides; FPC side open)
      translate([cx(panel[0])-1.2-clr,cy(panel[3])-clr,Z_in-1]) cube([1.2,panel[3]-panel[1]+2*clr,1.01]);
      for(y=[panel[1]-clr-1.2,panel[3]+clr]) translate([cx(panel[0])-clr, cy(y)-1.2+(y>panel[3]?0:0), Z_in-1]) cube([panel[2]-panel[0]-2,1.2,1.01]);
      // panel retaining hooks (2 per long edge) - flex 1.2 mm beams, 0.7 mm lip
      for(x=[50,88]) for(e=[0,1]) {
        yy = e==0 ? panel[1]-clr : panel[3]+clr;
        translate([cx(x),cy(yy),0]) rotate([0,0,e==0?0:180]) translate([-3,0,0]) {
          translate([0,0,Z_panel_b-0.9]) cube([6,1.2,Z_in-Z_panel_b+0.9+0.01]);
          translate([0,0,Z_panel_b-0.9]) rotate([0,90,0]) linear_extrude(6) polygon([[0,0],[-0.9,0],[0,-0.7]]); }
      }
      // thermal baffle around SHT40 island (stops 0.8 mm above PCB), open to right wall
      translate([0,0,Z_pcb_t+0.8]) linear_extrude(Z_in-Z_pcb_t-0.8+0.01) difference(){
        translate([cx(island[0]-1.4),cy(island[3]+1.6)]) square([W-wall-cx(island[0]-1.4)+0.5, island[3]-island[1]+3.2]);
        translate([cx(island[0]-0.4),cy(island[3]+0.6)]) square([W, island[3]-island[1]+1.2]);
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
    // RESET / BOOT pinholes, LED light pipe
    for(p=pin) translate([cx(p[0]),cy(p[1]),Z_in-1]) cylinder(d=1.6,h=10);
    translate([cx(led[0]),cy(led[1]),Z_in-1]) cylinder(d=2.0,h=10);
    // USB-C bench-flashing opening (bottom wall)
    translate([cx(usb_x)-6, -1, Z_pcb_t-2.0]) cube([12, wall+2, 7]);
    // vents: front over island (outlet), right side wall, bottom wall below island
    for(i=[0:4]) translate([cx(island[0]+1.5+i*2.75),cy((island[1]+island[3])/2),Z_in-1]) linear_extrude(10) square([1.6,11],center=true);
    for(i=[0:3]) translate([W-wall-1, cy(island[1]+2+i*3.2)-0.8, Z_pcb_t+1]) cube([wall+2,1.6,Z_in-Z_pcb_t-2]);
    // snap windows matching backplate bumps
    for(x=[W/2-35,W/2+35]) for(y=[-1,H-wall-1]) translate([x-5.5,y,3.2-1.0]) cube([11,wall+2,2.0]);
    // front engraving
    translate([cx(aa[0]),cy(aa[1])+3.2,Z_out-0.4]) linear_extrude(1) text("Connie's Thermostat",size=3);
    translate([cx(btn[0][0])-4.5,cy(btn[0][1])-1.5,Z_out-0.4]) linear_extrude(1) text("+",size=3,halign="right");
    translate([cx(btn[1][0])-4.5,cy(btn[1][1])-1.5,Z_out-0.4]) linear_extrude(1) text("-",size=3,halign="right");
    translate([cx(btn[2][0])-4.5,cy(btn[2][1])-1.5,Z_out-0.4]) linear_extrude(1) text("M",size=3,halign="right");
  }
}

// ---------------- button caps (print 3, flange down) ----------------
sw_top = Z_pcb_t + 1.5;   // TS-1187A H1.5 actuator
module cap(){ cylinder(d=7.4,h=Z_in-0.3-sw_top); cylinder(d=4.6,h=Z_out+0.8-sw_top); }

// ---------------- reference geometry (for renders) ----------------
module pcb(){ color("darkgreen") translate([OX,cy(BH),Z_pcb_b]) difference(){ cube([BW,BH,pcb_t]); for(h=holes) translate([h[0],BH-h[1],-1]) cylinder(d=4.3,h=5);} 
  color("silver") translate([cx(66),cy(25.5),Z_pcb_t]) cube([18,25.5,3.1]);                 // ESP32-C6
  color("black") for(b=btn) translate([cx(b[0])-3,cy(b[1])-3,Z_pcb_t]) cube([6,6,1.5]);
  color("green") translate([cx(term[0])-3.75,cy(term[1])-9.2,Z_pcb_b-9]) cube([7.5,18.4,9]);  // rear header
  color("limegreen") translate([cx(term[0])-3.5,cy(term[1])-9.6,Z_pcb_b-9-14]) cube([7,19.2,14]); // 15EDGK plug
  color("gray") translate([cx(usb_x)-4.5,cy(BH)+0.5,Z_pcb_t]) cube([9,7,3.2]);
  color("orange") translate([cx(127),cy(32.7),Z_pcb_t]) cube([1.5,1.5,0.5]);                 // SHT40
}
module epd(){ color("white") translate([cx(panel[0]),cy(panel[3]),Z_panel_b]) cube([panel[2]-panel[0],panel[3]-panel[1],panel_t]);
  color([0.2,0.2,0.2]) translate([cx(aa[0]),cy(aa[3]),Z_in-0.05]) cube([aa[2]-aa[0],aa[3]-aa[1],0.1]); }

if(part=="backplate") backplate();
else if(part=="shell") translate([0,H,Z_out]) rotate([180,0,0]) shell();   // print face-down
else if(part=="caps") for(i=[0:2]) translate([i*10,0,0]) cap();
else if(part=="assembly"){ color("lightgray") backplate(); pcb(); epd(); color("white") shell(); for(b=btn) color("lightblue") translate([cx(b[0]),cy(b[1]),sw_top]) cap(); }
else if(part=="exploded"){ color("lightgray") backplate(); translate([0,0,25]){ pcb(); } translate([0,0,40]) epd(); translate([0,0,75]) color("white") shell(); for(b=btn) color("lightblue") translate([cx(b[0]),cy(b[1]),sw_top+60]) cap(); }
else if(part=="section") difference(){ union(){ color("lightgray") backplate(); pcb(); epd(); color("white") shell(); for(b=btn) color("lightblue") translate([cx(b[0]),cy(b[1]),sw_top]) cap(); }
  translate([-1,-1,-30]) cube([W+2, cy(44)+1, 80]); }   // cut at board y=44 (through panel, FPC, DOWN button)
// all parts laid out for a 256x256 bed (Bambu P2S), print orientation, no supports
if(part=="plate"){
  translate([5,5,0]) translate([0,H,Z_out]) rotate([180,0,0]) shell();
  translate([5,H+12-(wall+clr),0]) backplate();
  for(i=[0:2]) translate([200+ (i*12), 30, 0]) cap();
}
