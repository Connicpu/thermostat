// Decorative pink swirls for the front face (flush 0.6 mm inlays, AMS slot 4).
// swirl_style: "subtle" (default) | "bold" | "none"
// All coordinates are case XY (same frame as thermostat_case.scad).
function spiral(c,r0,r1,turns,a0=0,dir=1,n=90) =
  [for(i=[0:n]) let(t=i/n, a=a0+dir*360*turns*t, r=r0+(r1-r0)*t) [c[0]+r*cos(a), c[1]+r*sin(a)]];
// gentle vine: sine wave between x0..x1 around y0
function vine(x0,x1,y0,amp,wl,ph=0,n=120) = [for(i=[0:n]) let(x=x0+(x1-x0)*i/n) [x, y0+amp*sin(360*(x-x0)/wl+ph)]];
module stroke(p,w){ for(i=[0:len(p)-2]) hull(){ translate(p[i]) circle(d=w,$fn=12); translate(p[i+1]) circle(d=w,$fn=12);} }
// spiral that ends tangent-ish at its outer end: draw from outside in
module curl(c,r,turns,a0,dir,w){ stroke(spiral(c,r,max(r*0.35,w*1.7),turns,a0,dir),w); }

module swirl_lines(style,w){
  // top band vine with curls
  stroke(vine(12,134,55,3.2,40.67,0),w);
  curl([12,59.5],6.0,1.5,  90, 1,w);      // top-left corner curl
  curl([134,59.5],6.0,1.5, 90,-1,w);      // top-right corner curl
  curl([50,61],3.6,1.3,   200,-1,w);
  curl([118,48],3.2,1.25, 160, 1,w);
  // flourish wrapping the cat
  stroke(spiral([18.5,35],10.5,9.0,0.62,110,1,60),w);
  curl([9.5,19.5],4.8,1.3,    80,-1,w);
  stroke([[9.5,24.3],[9.8,29],[9.9,31.5]],w);
  if(style=="bold"){
    stroke(vine(30,140,48.8,1.6,26,90),w*0.8);
    curl([78,62.5],4.2,1.3,  -20, 1,w);
    curl([134,10.5],5.5,1.4, 270, 1,w);
    stroke(vine(110,140,3.2,1.0,14,0,60),w*0.8);
    curl([12,9.5],4.5,1.4,   -90, 1,w);
    stroke(vine(8,32,3.2,1.0,12,0,40),w*0.8);
  }
}
