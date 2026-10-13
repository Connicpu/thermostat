// Decorative pink swirls for the front face (flush 0.6 mm inlays, AMS slot 4).
// swirl_style: "subtle" (default) | "bold" | "none"
// All coordinates are case XY (same frame as thermostat_case.scad).
function spiral(c,r0,r1,turns,a0=0,dir=1,n=90) =
  [for(i=[0:n]) let(t=i/n, a=a0+dir*360*turns*t, r=r0+(r1-r0)*t) [c[0]+r*cos(a), c[1]+r*sin(a)]];
// gentle vine: sine wave between x0..x1 around y0
function vine(x0,x1,y0,amp,wl,ph=0,n=120) = [for(i=[0:n]) let(x=x0+(x1-x0)*i/n) [x, y0+amp*sin(360*(x-x0)/wl+ph)]];
module stroke(p,w){ for(i=[0:len(p)-2]) hull(){ translate(p[i]) circle(d=w,$fn=12); translate(p[i+1]) circle(d=w,$fn=12);} }
// spiral that ends tangent-ish at its outer end: draw from outside in
// open spiral: radial pitch >= swirl_pitch_k*w (gap between turns >= (k-1)*w); inner end tapers
swirl_pitch_k = 2.5;   // gap between turns = 1.5*w
function curl_turns(r,turns,w) = max(0.3, min(turns, (r-1.0*w)/(swirl_pitch_k*w)));
module curl(c,r,turns,a0,dir,w){
  t=curl_turns(r,turns,w); rin=r-swirl_pitch_k*w*t; n=max(24,ceil(72*t));
  p=spiral(c,r,rin,t,a0,dir,n);
  for(i=[0:n-1]) let(f=i/n, wi=w*(f<0.55?1:1-0.45*(f-0.55)/0.45), wj=w*((i+1)/n<0.55?1:1-0.45*((i+1)/n-0.55)/0.45))
    hull(){ translate(p[i]) circle(d=wi,$fn=16); translate(p[i+1]) circle(d=wj,$fn=16); }
}

module swirl_lines(style,w){
  // top band vine with curls
  stroke(vine(17.6,128.4,55,3.2,36.93,0),w);
  curl([13,59.0],6.8,1.15,  90, 1,w);      // top-left corner curl
  curl([133,59.0],6.8,1.15, 90,-1,w);      // top-right corner curl
  curl([50,60.8],6.0,1.1,   200,-1,w);
  if(style!="bold") curl([121,47.0],5.4,1.0, 160, 1,w);
  // flourish wrapping the cat
  stroke(spiral([18.5,35],10.5,9.0,0.62,110,1,60),w);
  curl([10,18.5],6.0,1.1,    80,-1,w);
  stroke([[10,24.5],[10,29],[10,31.5]],w);
  if(style=="bold"){
    stroke(vine(24,140,46.5,2.2,29,90),w*0.8);
    curl([78,60.8],6.0,1.1,  -20, 1,w);
    curl([133,11.0],6.4,1.15, 270, 1,w);
    stroke(vine(110,140,3.2,1.0,14,0,60),w*0.8);
    curl([12.5,10.5],6.0,1.1,   -90, 1,w);
    stroke(vine(8,32,3.2,1.0,12,0,40),w*0.8);
  }
}
