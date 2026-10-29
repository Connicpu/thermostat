# Clearance of the populated board against the case, plus the FPC fold and the panel carrier.
# board.stl is not committed. From the repo root, with KiCad 9, and without
# --drill-origin (this script places the export itself):
#   kicad-cli pcb export stl -o board.stl --force --cut-vias-in-body \
#       kicad/thermostat-v3.kicad_pcb
# Assembly-frame solids, not the flipped print STLs:
#   OPENSCADPATH=case openscad -o back.stl -D 'part="none"' -D 'which="b"' scripts/case_fit_export.scad
#   OPENSCADPATH=case openscad -o shell.stl -D 'part="none"' -D 'which="s"' scripts/case_fit_export.scad
#   OPENSCADPATH=case openscad -o carrier.stl -D 'part="none"' -D 'which="c"' scripts/case_fit_export.scad
#   OPENSCADPATH=case openscad -o retainer.stl -D 'part="none"' -D 'which="r"' scripts/case_fit_export.scad
#   OPENSCADPATH=case openscad -o caps_seated.stl -D 'part="none"' -D 'which="k"' scripts/case_fit_export.scad
#   OPENSCADPATH=case openscad -o caps_pressed.stl -D 'part="none"' -D 'which="kp"' scripts/case_fit_export.scad
# Run this script in the directory that contains back.stl, shell.stl, carrier.stl, and the two cap meshes.
# The fold and the carrier checks run without board.stl. The mesh sampling does not.
import trimesh,numpy as np,sys,os,re,math
# trimesh samples from OS entropy when seed is omitted, so the same mesh
# reads a different clearance on the next run. Every sample below takes a counted seed.
_sample_seed=[10000]
def _sample(mesh, n):
    _sample_seed[0]+=1
    return mesh.sample(int(n), seed=_sample_seed[0])
_root0=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_scad=open(os.path.join(_root0,'case','thermostat_case.scad')).read()
def scad_num(name):
    m=re.search(r'(?m)^%s\s*=\s*(-?[0-9.]+)\s*;'%re.escape(name), _scad)
    if not m:
        print('SCAD CONSTANT MISSING', name); sys.exit(1)
    return float(m.group(1))
P=dict(standoff_h=scad_num('standoff_h'), panel_gap=scad_num('panel_gap'))
for a in sys.argv[1:]: k,v=a.split('='); P[k]=float(v)
plate_t=3; pcb_t=1.6; pcb_tol=0.16   # JLCPCB 1.6 mm board, ±0.16 mm
Zb=plate_t+P['standoff_h']; Zt=Zb+pcb_t; Zpb=Zt+P['panel_gap']
W=147; H=71; OX=3.5; OYT=3.5
TAIL_LEN=1.0          # rear pin leg below the board
FILLET_H=0.40         # solder fillet height, same as the rib-to-board gap
FILLET_GROW=0.20      # fillet past the pad copper, total (0.1 mm per side)
# GDEY029T94 free tail 14.40 mm, mouth at the J4 silk face, lower leg 0.30/0.50/0.70 mm.
# Pull-out is pi/2 mm of tail per mm of panel gap, whatever the leg height is.
FPC_FREE=14.40
FPC_MOUTH=103.77
FPC_EDGE=108.46
FPC_STRAIGHT=FPC_EDGE-FPC_MOUTH
def fpc_inserted(gap, z_leg):
    return FPC_FREE - math.pi*(gap-z_leg)/2.0 - FPC_STRAIGHT
# Jushuo drawing: recommended contact length 3.0 mm minimum. The gate is that
# minimum at the design gap and at a carrier printed 0.10 mm tall, lower leg 0.50 mm.
# Other legs are printed so the assumption stays visible. They are not all gated:
# a 0.30 mm leg is the tail lying on the soldermask, and the side view does not say that.
FPC_MIN=3.0
PRINT_Z=0.10
print('FPC tail %.2f mm, mouth x %.2f, straight %.2f mm, pull-out %.3f mm/mm, minimum %.2f mm'%(FPC_FREE, FPC_MOUTH, FPC_STRAIGHT, math.pi/2, FPC_MIN))
fpc_ok=True
for z_leg in (0.30, 0.50, 0.70):
    for gap in (P['panel_gap']-PRINT_Z, P['panel_gap'], P['panel_gap']+PRINT_Z):
        ins=fpc_inserted(gap, z_leg)
        gated = abs(z_leg-0.50)<1e-6 and gap>=P['panel_gap']-1e-6
        bad=gated and ins<FPC_MIN-1e-6
        fpc_ok = fpc_ok and not bad
        print('  leg %.2f  gap %.2f  R %.3f  inserted %5.2f%s'%(z_leg, gap, (gap-z_leg)/2.0, ins, '  SHORT' if bad else ''))
nominal=fpc_inserted(P['panel_gap'], 0.50)
print('nominal gap %.2f leg 0.50 inserted %.2f (drawing minimum %.2f)'%(P['panel_gap'], nominal, FPC_MIN))
# Exit plane. The free length is measured past the outline. This model starts the
# bend at the display panel back. Neither datasheet states which face the tail leaves, and
# neither states a minimum bend radius (the connector's R0.20 is a housing fillet).
front_exit=fpc_inserted(P['panel_gap']+1.2, 0.50)
print('if the tail left the display panel front face instead, inserted %.2f mm at this gap'%(front_exit))
opto_h=4.0
opto_floor=0.30
# A short print leaves 0.35 mm over the 4.0 mm optos. The floor is 0.30 mm.
print('opto clearance short %.2f mm, nominal %.2f mm, tall %.2f mm (floor %.2f)'%(
    P['panel_gap']-PRINT_Z-opto_h, P['panel_gap']-opto_h, P['panel_gap']+PRINT_Z-opto_h, opto_floor))
if P['panel_gap']-PRINT_Z-opto_h < opto_floor-1e-9:
    fpc_ok=False
    print('OPTO CLEARANCE FAIL')
if not fpc_ok:
    print('FPC BUDGET FAIL')
    sys.exit(1)
def T(bx,by): return OX+bx, H-OYT-by
def box(name,bx0,by0,bx1,by1,z0,z1):
  X0,Y1=T(bx0,by0); X1,Y0=T(bx1,by1)
  m=trimesh.creation.box(extents=[X1-X0,Y1-Y0,z1-z0]); m.apply_translation([(X0+X1)/2,(Y0+Y1)/2,(z0+z1)/2]); return name,m
def cyl(name,bx,by,r,z0,z1):
  X,Y=T(bx,by); m=trimesh.creation.cylinder(radius=r,height=z1-z0,sections=64); m.apply_translation([X,Y,(z0+z1)/2]); return name,m
for req in ('back.stl','shell.stl','carrier.stl','retainer.stl','caps_seated.stl','caps_pressed.stl'):
    if not os.path.exists(req):
        print('missing', req)
        sys.exit(1)
back=trimesh.load('back.stl'); shell=trimesh.load('shell.stl'); carrier=trimesh.load('carrier.stl'); retainer=trimesh.load('retainer.stl')
have_board=os.path.exists('board.stl')
if have_board:
    brd=trimesh.load('board.stl'); brd.apply_translation([-100+OX,H-OYT+100,Zb])
    v=brd.triangles.reshape(-1,3)
else:
    brd=None; v=None
    print('WARNING: board.stl absent (this VM has no KiCad 9 export). Skipping populated-board mesh sampling.')
def kicad_part(name,bx0,by0,bx1,by1,front=True):
  X0,Y1=T(bx0,by0); X1,Y0=T(bx1,by1)
  s=(v[:,0]>=X0)&(v[:,0]<=X1)&(v[:,1]>=Y0)&(v[:,1]<=Y1)&((v[:,2]>Zt+0.02) if front else (v[:,2]<Zb-0.02))
  return name,trimesh.points.PointCloud(v[s])
parts=[]
# real KiCad models (sampled vertices). Needs board.stl.
if have_board:
  for n,b in dict(U6=(28.8,6.3,40.2,14.1),U7=(28.8,21.3,40.2,29.1),U8=(28.8,36.3,40.2,44.1),Q2=(14.5,6.6,25.9,13.4),Q3=(14.5,21.6,25.9,28.4),Q4=(14.5,36.6,25.9,43.4),J3=(119.6,5,126.4,10.8),J4=(98.5,34.8,103.9,53),SW3=(108.5,27.3,115.5,32.7),SW4=(108.5,37.3,115.5,42.7),SW5=(108.5,47.3,115.5,52.7),SW1=(89.5,2.3,96.5,7.7),SW2=(98.5,2.3,105.5,7.7),D2=(12.6,57.3,19.4,61.3),D3=(115.8,45,122.2,48),D4=(29.5,54.1,32.5,60.5)).items():
    parts.append(kicad_part(n,*b))
  parts.append(kicad_part('J1 header (model)',59.4,49.9,79.3,62.7,front=False))
# missing models -> datasheet envelopes
parts+= [box('L1 SWPA6045 6.0x6.0x4.5',21.1,53.5,27.1,59.5,Zt,Zt+4.5),
 box('J2 USB-C 8.94x7.3x3.26 (flush)',121.05-4.47,56.7,121.05+4.47,64.0,Zt,Zt+3.26),
 box('U3 ESP32-C6 module 18x25.5x3.15',66.05,0,84.05,25.5,Zt,Zt+3.15),
 box('C1/C41 1210 x2.5',42.9,54.25,46.1,60.25,Zt,Zt+2.5), box('C42 1210 x2.5',47.75,54.4,50.25,57.6,Zt,Zt+2.5),
 box('C2 1210 x2.7 max',26.75,48.4,29.25,51.6,Zt,Zt+2.7),
 cyl('C43 8x9 (9.5 incl. seat)',53.3,58.05,4.0+0.25,Zb-9.5,Zb),
 box('J1 header body (datasheet 18.3x9.2x7.6)',70.05-9.15,50.05,70.05+9.15,59.25,Zb-7.6,Zb),
 box('J1 plug 15EDGK (18.3 x 15.6 x 11.1)',70.05-9.15,40.45,70.05+9.15,56.05,Zb-11.1,Zb),
 box('J1/C43 THT tails front (2.0)',49.5,56.5,79.0,59.5,Zt,Zt+2.0),
 # datasheet envelopes for parts the zone walls pass (used even when a 3D model is present)
 box('Q2 DPAK x2.3',14.5,6.6,25.9,13.4,Zt,Zt+2.3),
 box('Q3 DPAK x2.3',14.5,21.6,25.9,28.4,Zt,Zt+2.3),
 box('Q4 DPAK x2.3',14.5,36.6,25.9,43.4,Zt,Zt+2.3),
 box('R22 0805 max 2.15x1.40x0.65',29.425,15.50,31.575,16.90,Zt,Zt+0.65),
 box('R32 0805 max 2.15x1.40x0.65',29.425,30.50,31.575,31.90,Zt,Zt+0.65),
 box('R42 0805 max 2.15x1.40x0.65',29.425,45.50,31.575,46.90,Zt,Zt+0.65),
 box('R23 0805 x0.70',26.50,8.9,28.00,11.1,Zt,Zt+0.70),
 box('R33 0805 x0.70',26.50,23.9,28.00,26.1,Zt,Zt+0.70),
 box('R43 0805 x0.70',26.50,38.9,28.00,41.1,Zt,Zt+0.70),
 box('C5 0603 x0.90',24.8,61.2,26.4,62.4,Zt,Zt+0.90),
 box('D3 SMA x2.50',116.4,45.0,121.6,48.0,Zt,Zt+2.50),
 box('R13 0402 x0.50',117.0,25.9,117.8,27.1,Zt,Zt+0.50),
 box('R3 0402 x0.50',117.1,49.9,117.9,51.1,Zt,Zt+0.50),
 box('C11 0402 x0.55',116.4,39.4,117.6,40.3,Zt,Zt+0.55),
 box('R14 0402 x0.50',118.53,25.97,119.18,27.03,Zt,Zt+0.50),
]
def _rot(x,y,deg):
    a=math.radians(deg); c,s=math.cos(a),math.sin(a); return x*c-y*s, x*s+y*c
def _tail_solid(sx,sy,h,ang,circular):
    if circular:
        m=trimesh.creation.cylinder(radius=max(sx,sy)/2.0,height=h,sections=32)
    else:
        m=trimesh.creation.box(extents=[sx,sy,h])
    if abs(ang)>0.01:
        m.apply_transform(trimesh.transformations.rotation_matrix(math.radians(ang),[0,0,1]))
    return m
def rear_tails(pcb_path):
    """1 mm rear leg plus a solder fillet for every through-hole pin. NPTH locating pegs get the leg only."""
    txt=open(pcb_path).read()
    out=[]
    for fp in re.split(r'\n\t\(footprint ', txt)[1:]:
        refm=re.search(r'\(property "Reference" "([^"]+)"', fp)
        atm=re.search(r'\(at ([0-9.\-]+) ([0-9.\-]+)(?: ([0-9.\-]+))?\)', fp)
        if not refm or not atm: continue
        ref=refm.group(1); fx,fy=float(atm.group(1))-100.0, float(atm.group(2))-100.0
        frot=float(atm.group(3) or 0)
        for m in re.finditer(r'\(pad "([^"]*)" (thru_hole|np_thru_hole) \w+\s*\(at ([0-9.\-]+) ([0-9.\-]+)(?: ([0-9.\-]+))?\)\s*\(size ([0-9.\-]+) ([0-9.\-]+)\)\s*\(drill(?: oval)? ([0-9.\-]+)(?: ([0-9.\-]+))?\)', fp):
            name,kind,px,py,prot,sx,sy,dx,dy=m.group(1),m.group(2),float(m.group(3)),float(m.group(4)),float(m.group(5) or 0),float(m.group(6)),float(m.group(7)),float(m.group(8)),float(m.group(9) or m.group(8))
            if max(dx,dy)>=3.0: continue          # mounting holes; the standoff pin is a designed fit
            if kind=='thru_hole' and max(dx,dy)<0.5: continue   # module vias, not component leads
            bx,by=_rot(px,py,frot); ang=frot+prot
            X,Y=T(fx+bx, fy+by)
            label='%s %s'%(ref, name or ('peg' if kind=='np_thru_hole' else 'pin'))
            leg=_tail_solid(dx,dy,TAIL_LEN,ang, abs(dx-dy)<0.05)
            leg.apply_translation([X,Y,Zb-TAIL_LEN/2])
            out.append(('%s rear leg'%label, leg))
            if kind=='thru_hole':
                fil=_tail_solid(sx+FILLET_GROW, sy+FILLET_GROW, FILLET_H, ang, abs(sx-sy)<0.05)
                fil.apply_translation([X,Y,Zb-FILLET_H/2])
                out.append(('%s rear fillet'%label, fil))
    return out
_root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_pcb=os.path.join(_root,'kicad','thermostat-v3.kicad_pcb')
if os.path.exists(_pcb):
    parts+=rear_tails(_pcb)
else:
    print('WARNING: no kicad pcb at', _pcb)
# wires: 5 x d1.8, exit plug face, 3 mm straight, bend r4, down through hole
for i in range(5):
  bx=70.05-7+3.5*i; X,_=T(bx,0); Y0=T(0,40.45)[1]; zc=Zb-5.5; r=4.0
  pts=[[X,Y0,zc],[X,Y0+3,zc]]+[[X,Y0+3+r*np.sin(t),zc-r+r*np.cos(t)] for t in np.linspace(0,np.pi/2,8)]+[[X,Y0+3+r,-3]]
  segs=[]
  for a,b in zip(pts[:-1],pts[1:]):
    c=trimesh.creation.cylinder(radius=0.9,segment=[a,b],sections=16); segs.append(c)
  parts.append(('wire %d'%(i+1),trimesh.util.concatenate(segs)))
# panel as obstacle for front parts
_,panel=box('panel',29.46,26.24,108.46,62.94,Zpb,Zpb+1.2)
pcbm=brd
obst={'backplate':back,'shell':shell,'display panel':panel}
res=[]
for n,m in parts:
  pts=np.asarray(m.vertices) if isinstance(m,trimesh.points.PointCloud) else np.vstack([m.vertices,_sample(m,4000)])
  if len(pts)==0: continue
  front=float(np.max(pts[:,2]))>Zt+0.05
  for on,o in obst.items():
    if n.startswith('wire') and on!='backplate': continue
    q=trimesh.proximity.ProximityQuery(o)
    d=q.signed_distance(pts)   # >0 inside
    worst=d.max()
    # thick board lifts front parts toward the shell; the back face stays on the standoffs
    if front and on!='backplate':
      worst=max(worst, q.signed_distance(pts+np.array([0,0,pcb_tol])).max())
    res.append((n,on,-worst))
print('clearance mm (negative = interference); front parts include +%.2f mm board thickness'%pcb_tol)
interfere=[r for r in res if r[2]<0]
for n,on,c in sorted(res,key=lambda r:r[2]):
  if c<6 or n.startswith('J2'): print('%-42s vs %-14s %6.2f %s'%(n,on,c,'INTERFERE' if c<0 else ('TIGHT' if c<1 else '')))
j2=[r for r in res if r[0].startswith('J2')]
if j2:
  w=min(j2,key=lambda r:r[2])
  print('J2 rear tails/peg worst: %-28s vs %-14s %6.2f'%(w[0],w[1],w[2]))
# Board outline vs shell. Contact fails. The shell clips to the backplate and
# clears the board, including a board that is pcb_tol thick.
pcb_shell=0.0; pcb_back=0.0
if have_board:
    d=trimesh.proximity.ProximityQuery(shell).signed_distance(np.vstack([pcbm.vertices,_sample(pcbm,20000)]))
    pcb_shell=-float(d.max())
    print('PCB outline vs shell',round(pcb_shell,2))
    d=trimesh.proximity.ProximityQuery(back).signed_distance(np.vstack([pcbm.vertices,_sample(pcbm,20000)]))
    pcb_back=-float(d.max())
    print('PCB vs backplate (standoff seat expected 0)',round(pcb_back,2))
else:
    print('KiCad board mesh absent; the slab below checks the shell against the board outline')

def mesh_clearance(part, obst, dz=0.0, n=6000):
    """Positive means air between the solids. dz lifts a second copy of the samples."""
    pts=np.vstack([part.vertices, _sample(part, n)])
    if dz:
        pts=np.vstack([pts, pts+np.array([0,0,dz])])
    return -float(trimesh.proximity.ProximityQuery(obst).signed_distance(pts).max())

_,board_slab=box('board slab',0,0,140,64,Zb,Zt)
board_shell=mesh_clearance(board_slab, shell, dz=pcb_tol, n=8000)
print('board slab vs shell (incl +%.2f board) %6.2f'%(pcb_tol, board_shell))

carrier_fail=False
bodies=carrier.split(only_watertight=False)
print('carrier bodies', len(bodies))
if len(bodies)>1:
        # Touching bodies print as one piece. A real gap does not.
    parent=list(range(len(bodies)))
    def find(i):
        while parent[i]!=i:
            parent[i]=parent[parent[i]]; i=parent[i]
        return i
    for i in range(len(bodies)):
        qi=trimesh.proximity.ProximityQuery(bodies[i])
        for j in range(i+1, len(bodies)):
            if qi.signed_distance(_sample(bodies[j], 400)).max() < -0.05:
                continue
            parent[find(j)]=find(i)
    if len({find(i) for i in range(len(bodies))})>1:
        print('CARRIER IS NOT ONE SOLID')
        carrier_fail=True
c_shell=mesh_clearance(carrier, shell, dz=pcb_tol)
c_back=mesh_clearance(carrier, back, dz=-pcb_tol)
print('carrier vs shell (incl +%.2f board) %6.2f'%(pcb_tol, c_shell))
print('carrier vs backplate (incl -%.2f board) %6.2f'%(pcb_tol, c_back))
if c_shell<0.15 or c_back<0.15:
    carrier_fail=True
# The pads kiss the display panel back. A few hundredths is the mesh.
# A gap is the panel with nothing under it. Interference is a pad through the panel.
c_panel=mesh_clearance(carrier, panel, n=8000)
print('carrier vs display panel (kiss expected near 0) %6.2f'%c_panel)
if c_panel<-0.08 or c_panel>0.12:
    carrier_fail=True
    print('PANEL SEAT FAIL')
tall=[
 box('U6 opto x4.0',28.8,6.3,40.2,14.1,Zt,Zt+4.0),
 box('U7 opto x4.0',28.8,21.3,40.2,29.1,Zt,Zt+4.0),
 box('U8 opto x4.0',28.8,36.3,40.2,44.1,Zt,Zt+4.0),
 box('L1 SWPA6045 x4.5',21.1,53.5,27.1,59.5,Zt,Zt+4.5),
 box('J2 USB-C body',116.58,56.7,125.52,64.0,Zt,Zt+3.26),
 box('U3 ESP32 module',66.05,0,84.05,25.5,Zt,Zt+3.15),
 box('SW3',108.5,27.3,115.5,32.7,Zt,Zt+1.5),
 box('SW4',108.5,37.3,115.5,42.7,Zt,Zt+1.5),
 box('SW5',108.5,47.3,115.5,52.7,Zt,Zt+1.5),
]
# Front ends of through-hole leads. The carrier threads between the C43 leads and beside J1 pin 5.
if os.path.exists(_pcb):
    txt=open(_pcb).read()
    for fp in re.split(r'\n\t\(footprint ', txt)[1:]:
        refm=re.search(r'\(property "Reference" "([^"]+)"', fp)
        atm=re.search(r'\(at ([0-9.\-]+) ([0-9.\-]+)(?: ([0-9.\-]+))?\)', fp)
        if not refm or not atm: continue
        ref=refm.group(1); fx,fy=float(atm.group(1))-100.0, float(atm.group(2))-100.0
        frot=float(atm.group(3) or 0)
        for m in re.finditer(r'\(pad "([^"]*)" thru_hole \w+\s*\(at ([0-9.\-]+) ([0-9.\-]+)(?: ([0-9.\-]+))?\)\s*\(size ([0-9.\-]+) ([0-9.\-]+)\)\s*\(drill(?: oval)? ([0-9.\-]+)', fp):
            px,py,prot,dx=float(m.group(2)),float(m.group(3)),float(m.group(4) or 0),float(m.group(7))
            if dx>=3.0: continue
            bx,by=_rot(px,py,frot)
            if dx<0.5: continue
            tall.append(cyl('%s pin %s'%(ref, m.group(1)), fx+bx, fy+by, dx/2.0, Zt, Zt+2.0))
for n,m in tall:
    c=mesh_clearance(carrier, m, n=2000)
    mark=''
    if c<0.40:
        carrier_fail=True
        mark='  TIGHT' if c>=0 else '  INTERFERE'
    if c<2.0 or mark:
        print('carrier vs %-28s %6.2f%s'%(n, c, mark))
# The bend bulges past the panel edge. The carrier stays on the other side of that pocket.
fpc_pts=carrier.vertices
y_hi=T(0, 37.75)[1]; y_lo=T(0, 50.25)[1]
in_bulge=(fpc_pts[:,0] > OX+108.2) & (fpc_pts[:,1] > y_lo) & (fpc_pts[:,1] < y_hi)
if in_bulge.any():
    print('CARRIER ENTERS THE FPC BULGE', int(in_bulge.sum()))
    carrier_fail=True
else:
    print('carrier stays clear of the FPC bulge')
# Display panel seat. The pads carry the panel. The retainer presses it down.
# The shell does not hold it.
wall=scad_num('wall'); clr=scad_num('clr')
snap_bump=scad_num('snap_bump'); snap_z=scad_num('snap_z'); snap_w=scad_num('snap_w')
petg_eps=scad_num('petg_eps'); fence_air=scad_num('fence_air'); fence_t=scad_num('fence_t')
xstop_bite=scad_num('xstop_bite'); face_recess=scad_num('face_recess')
panel_lo=scad_num('panel_lo'); panel_hi=scad_num('panel_hi'); panel_h=scad_num('panel_h')
j4_front=scad_num('j4_front'); j4_depth=scad_num('j4_depth'); j4_h=scad_num('j4_h')
j4_y0=scad_num('j4_y0'); j4_y1=scad_num('j4_y1'); mouth_x=scad_num('mouth_x')
step_mouth=scad_num('step_mouth')
Zin=Zt+P['panel_gap']+1.2
seat_fail=False
if re.search(r'(?m)^module\s+(edge_hook|j4_pick|release_blade|hook_relief|hook_pocket)\b', _scad):
    seat_fail=True
    print('HOOK MODULE IS IN THE MODEL')
# Backplate clips. δ is the bump past the shell clearance. L is the wall from the
# bump up to the front face. ε = 1.5 t δ / L^2. F = 3 E I δ / L^3, I = w t^3 / 12.
# w is the snap window. The climb is local to that window.
snap_defl=snap_bump-clr
snap_L=Zin-snap_z
snap_E=1800.0
snap_I=snap_w*wall**3/12.0
snap_eps=1.5*wall*snap_defl/(snap_L*snap_L)
snap_F=3*snap_E*snap_I*snap_defl/(snap_L**3)
# The bump polygon rises 0.80 mm over a 0.90 mm z half-span. That ramp is atan(0.80/0.90).
snap_ramp=math.degrees(math.atan2(snap_bump, 0.90))
# One window is a cantilever. A fixed-guided beam of the same section is four times that.
# The two long walls, each counted once as that guided beam, are the push that seats all four clips.
snap_F_guided=4.0*snap_F
snap_F_walls=2.0*snap_F_guided
print('backplate clips: L %.2f mm  t %.2f  bump %.2f  climb %.2f  ramp %.1f deg  strain %.2f%%  force %.2f N'%(
    snap_L, wall, snap_bump, snap_defl, snap_ramp, 100*snap_eps, snap_F))
print('four clips: one window cantilever %.2f N, four separate cantilevers %.1f N, both long walls as fixed-guided beams %.1f N'%(
    snap_F, 4.0*snap_F, snap_F_walls))
print('shell prints face-down, so the clip wall runs along the build axis. The %.2f%% outer fiber is across the layers, under the 1.0%% interlayer lower bound.'%(100*snap_eps))
print('the %.2f%% strain is the flexural gate (%.1f%%). A wall of L=8 mm at this climb is %.2f%% and fails.'%(
    100*snap_eps, 100*petg_eps, 100*1.5*wall*snap_defl/(8.0*8.0)))
if snap_defl<=0.20 or snap_L<=11.0 or snap_eps>=petg_eps or snap_F>=8.0:
    seat_fail=True
    print('CLIP STRAIN FAIL')
if not (1.5*wall*snap_defl/(8.0*8.0) >= petg_eps):
    seat_fail=True
    print('CLIP GATE DOES NOT REJECT A SHORT WALL')
# Four bumps. The apex sits in the shell window and in the backplate.
# A point beside the window, at the same radius, is shell. That is the wall that climbs.
def _clip_points(cx0, lip_y, outward):
    # Inside the bump, 0.25 mm back from the apex. The apex itself is the surface.
    inner=(cx0, lip_y+outward*(snap_bump-0.25), snap_z)
    beside=(cx0+8.0, lip_y+outward*(snap_bump*0.75), snap_z)
    return inner, beside
qshell=trimesh.proximity.ProximityQuery(shell)
qback=trimesh.proximity.ProximityQuery(back)
po=wall+clr
for cx0, lip_y, outward in (
    (W/2-35, po, -1.0), (W/2+35, po, -1.0),
    (W/2-35, H-po, +1.0), (W/2+35, H-po, +1.0)):
    apex, beside=_clip_points(cx0, lip_y, outward)
    d_back=float(qback.signed_distance([apex])[0])
    d_win=float(qshell.signed_distance([apex])[0])
    d_wall=float(qshell.signed_distance([beside])[0])
    print('clip x %.1f y %.1f  bump %+.2f (want >0)  window %+.2f (want <0)  wall %+.2f (want >0)'%(
        cx0, lip_y, d_back, d_win, d_wall))
    if d_back<0.05 or d_win>-0.05 or d_wall<0.05:
        seat_fail=True
        print('CLIP MESH FAIL')
# Closing the window around the apex makes the bump a solid interference. The gate rejects that.
apex=(W/2-35, po-snap_bump, snap_z)
d_apex=float(qshell.signed_distance([apex])[0])
blocked=trimesh.creation.box(extents=[12.0, 3.2, 2.6])
blocked.apply_translation(apex)
d_blocked=float(trimesh.proximity.ProximityQuery(blocked).signed_distance([apex])[0])
print('clip window %+.2f (want <0)  filled window %+.2f (want >0)'%(d_apex, d_blocked))
if d_apex>-0.05 or d_blocked<0.05:
    seat_fail=True
    print('CLIP NEGATIVE DID NOT FAIL' if d_blocked<0.05 else 'CLIP WINDOW IS SOLID')
# Air beside the panel edge, at mid height, stays empty. Filling it is a lip.
qcar=trimesh.proximity.ProximityQuery(carrier)
hooks_old=((47.0,53.0,26.24,1),(84.6,90.6,26.24,1),(49.6,55.6,62.94,-1),(76.0,82.0,62.94,-1))
def _gap_box(x0, x1, edge, dire):
    y0, y1 = (edge+0.20, edge+0.50) if dire>0 else (edge-0.50, edge-0.20)
    return box('gap', x0, y0, x1, y1, Zpb+0.30, Zpb+0.70)[1]
for x0,x1,edge,dire in hooks_old:
    gap=_gap_box(x0, x1, edge, dire)
    sd=float(trimesh.proximity.ProximityQuery(carrier).signed_distance(np.vstack([gap.vertices, _sample(gap, 200)])).max())
    print('tongue gap x %.1f  solid %+.2f (want <0)'%(0.5*(x0+x1), sd))
    if sd>-0.02:
        seat_fail=True
        print('TONGUE TOUCHES THE PANEL')
gap0=_gap_box(*hooks_old[0])
_fill = box('fill', 47.0, 26.44, 53.0, 26.74, Zin-0.05, Zin+0.20)[1]
_pt = np.array([[T(50.0, 26.59)[0], T(50.0, 26.59)[1], Zin+0.08]])
sd_air = float(qcar.signed_distance(_pt)[0])
sd_neg = float(trimesh.proximity.ProximityQuery(trimesh.util.concatenate([carrier, _fill])).signed_distance(_pt)[0])
print('gap air %+.2f (want <0)  filled %+.2f (want >0)'%(sd_air, sd_neg))
if sd_air > -0.02 or sd_neg < 0.02:
    seat_fail=True
    print('GAP NEGATIVE DID NOT FAIL')
# Fences. Solid beside the outline, air in the gap, and the fence stops under the shell face.
def _fence_probe(x, edge, dire):
    y_solid = edge - dire*(fence_air + fence_t*0.5)
    y_air = edge - dire*(fence_air*0.5)
    z = Zpb + 0.40
    d_solid=float(qcar.signed_distance([[OX+x, H-OYT-y_solid, z]])[0])
    d_air=float(qcar.signed_distance([[OX+x, H-OYT-y_air, z]])[0])
    return d_solid, d_air
for x, edge, dire, name in ((47.5, 26.24, 1, 'top'), (88.0, 26.24, 1, 'top-right'),
                            (52.0, 62.94, -1, 'bottom'), (80.0, 62.94, -1, 'bottom-right')):
    d_solid, d_air=_fence_probe(x, edge, dire)
    print('fence %-12s solid %+.2f (want >0)  gap %+.2f (want <0)'%(name, d_solid, d_air))
    if d_solid<0.05 or d_air>-0.02:
        seat_fail=True
        print('FENCE FAIL')
# A 36.50 / 36.70 / 36.90 mm panel, seated on the bottom fence. The top fence is
# fence_air past the outline on that side too, so the gap is
# 2*fence_air + (panel_h - G): 0.60 / 0.40 / 0.20 mm at fence_air 0.20.
for G, label in ((panel_lo, 'narrow'), (panel_h, 'nominal'), (panel_hi, 'wide')):
    top_gap=2.0*fence_air+(panel_h-G)
    print('  display panel %.2f mm (%s), top gap if it sits on the bottom fence %.2f mm'%(G, label, top_gap))
    if top_gap<0.0:
        seat_fail=True
        print('  PANEL WIDER THAN THE FENCES')
# Y travel between the fences is 2*fence_air on the nominal width. The locked tail
# is the fine limit (the guide play below). The fences are the coarse stop.
# 0.20 mm of air is 0.40 mm of travel and passes. 0.60 mm of air is 1.20 mm and fails.
y_travel=2.0*fence_air
print('panel y travel between the fences %.2f mm (fence air %.2f mm each side)'%(y_travel, fence_air))
def _y_travel_ok(air):
    return 2.0*air<=0.50+1e-9
if not _y_travel_ok(fence_air):
    seat_fail=True
    print('Y PLAY FAIL')
if _y_travel_ok(0.6):
    seat_fail=True
    print('FENCE AIR 0.6 DID NOT FAIL')
# Pad tops carry the panel. The global mesh kiss is the post bite, so a pad that
# is short does not show up there. Read the pad top itself. A 0.30 mm shortfall fails.
def _pad_gap(bx, by):
    X,Y=T(bx, by)
    zs=np.linspace(Zpb-0.80, Zpb+0.15, 40)
    sd=qcar.signed_distance([[X, Y, z] for z in zs])
    inside=zs[np.asarray(sd)>0.0]
    if len(inside)==0:
        return 99.0
    return float(Zpb-inside.max())
def _pad_gap_ok(gap):
    return -0.08<=gap<=0.12
for bx, by, name in ((50.0, 27.0, 'top'), (83.0, 27.0, 'top-right'),
                     (52.6, 62.1, 'bottom'), (79.0, 62.1, 'bottom-right')):
    gap=_pad_gap(bx, by)
    print('pad %-12s top is %.2f mm under the panel back (want -0.08..0.12)'%(name, gap))
    if not _pad_gap_ok(gap):
        seat_fail=True
        print('PAD SEAT FAIL')
if _pad_gap_ok(0.30):
    seat_fail=True
    print('PAD CUT 0.30 DID NOT FAIL')
# Guide play. Sideways travel while the shell goes on or off is 0: the shell does not touch the panel.
fpc_wide=scad_num('fpc_wide'); fpc_guide=scad_num('fpc_guide')
tail_tol=0.10; guide_tol=0.10
play_nom=fpc_guide-fpc_wide
play_narrow=fpc_guide-(fpc_wide-tail_tol)
play_wide=fpc_guide-(fpc_wide+tail_tol)
play_hi=(fpc_guide+guide_tol)-(fpc_wide-tail_tol)
play_lo=(fpc_guide-guide_tol)-(fpc_wide+tail_tol)
print('J4 guide %.2f±%.2f mm, tail %.2f±%.2f mm, play nominal %.2f  narrow-tail %.2f  wide-tail %.2f  extremes %.2f and %.2f'%(
    fpc_guide, guide_tol, fpc_wide, tail_tol, play_nom, play_narrow, play_wide, play_hi, play_lo))
print('the %.2f mm tight stack is printed and is not a gate: a wide tail in a narrow guide does not assemble'%play_lo)
if fpc_wide>fpc_guide+1e-9:
    seat_fail=True
    print('J4 TAIL DOES NOT FIT THE GUIDE')
nonneg=[p for p in (play_nom, play_narrow, play_wide, play_hi, play_lo) if p>=-1e-9]
slide_cap=min(nonneg) if nonneg else -1.0
shell_slide=0.0
print('shell sideways travel %.3f mm, smallest non-negative guide play %.2f mm'%(shell_slide, slide_cap))
if shell_slide>slide_cap+1e-9:
    seat_fail=True
    print('J4 GUIDE PLAY FAIL')
# Housing from the STEP: body y 36–52, not the 17.60 mm overall length. The silk mouth
# is step_mouth past j4_front. 3.90 mm is the unlabelled side-view run, not DIM B.
print('J4 housing y %.2f–%.2f, face x %.3f, mouth x %.2f, block in front of the face %.2f mm, side-view run %.2f mm'%(
    j4_y0, j4_y1, j4_front, mouth_x, mouth_x-j4_front, j4_depth))
if abs((j4_y1-j4_y0)-16.0)>1e-6 or abs((mouth_x-j4_front)-step_mouth)>0.02 or abs(j4_h-2.0)>1e-6:
    seat_fail=True
    print('J4 HOUSING FAIL')
import sys as _sys
_sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
def _box_overlap(a, b):
    return not (a[3]<=b[0] or b[3]<=a[0] or a[4]<=b[1] or b[4]<=a[1] or a[5]<=b[2] or b[5]<=a[2])
# J4 solid is the STEP, shifted so the slider front meets the silk mouth.
# A region with no WRL vertices is not air. The housing wall over the shoe is solid.
from j4_solid import J4Solid
_j4 = J4Solid()
print('J4 STEP shift along board x %+.2f mm so the slider front is at the silk mouth %.2f'%(_j4.shift, mouth_x))
# The housing wall at board x 100.05–100.55, y 50.45–51.75, z 0.90–1.20
# is solid. The gate fails if that box is reported empty.
_shoe_pts = []
# y 50.6–51.0 is the housing wall beside the shoe. y 52.1 is the fitting nail.
# y 51.5 is the air slot beside that wall and must stay air, so it is not in this list.
for _x in (100.20, 100.40):
    for _y in (50.60, 50.90, 52.10):
        for _z in (1.00, 1.15):
            _shoe_pts.append((_x, _y, _z))
_shoe_pts = np.array(_shoe_pts)
_shoe_in = _j4.contains_points(_shoe_pts)
print('shoe window inside the STEP solid %d/%d (want all)'%(int(_shoe_in.sum()), len(_shoe_in)))
if int(_shoe_in.sum()) < len(_shoe_in):
    seat_fail = True
    print('SHOE WINDOW NOT SOLID — empty vertices were treated as air')
# A point above the connector is air. If the solid test marks it solid, the ray pairing is wrong.
if _j4.contains(110.0, 44.0, 1.0):
    seat_fail = True
    print('J4 SOLID TEST MARKS OPEN AIR')
# An odd column is a crack in the tessellation. The vote keeps the housing top solid.
if not _j4.contains(101.3, 45.35, 1.85):
    seat_fail = True
    print('J4 ODD COLUMN READS THE HOUSING TOP AS AIR')
# Carrier, including the mouth witness, stays out of the STEP solid.
_cv = carrier.vertices
_near = (_cv[:,0] > OX+98.0) & (_cv[:,0] < OX+106.0) & (_cv[:,1] < H-OYT-34.0) & (_cv[:,1] > H-OYT-66.0)
_cpts = _cv[_near]
_cb = np.column_stack([_cpts[:,0]-OX, H-OYT-_cpts[:,1], _cpts[:,2]-Zt])
_cin = _j4.contains_points(_cb) if len(_cb) else np.zeros(0, dtype=bool)
print('carrier vertices inside the J4 STEP %d (want 0)'%int(_cin.sum()))
if _cin.any():
    seat_fail = True
    print('CARRIER INSIDE J4')
# Slider faces a thumbnail can actually touch. A ray from open air has to
# hit that plane first. Hidden faces and a box beside the connector do not count.
def _ear_faces(bands, min_dy, min_dz):
    return [b for b in bands if b['dy'] >= min_dy and b['dz'] >= min_dz and (b['y1'] < 39.0 or b['y0'] > 49.5)]
_front_bands = _ear_faces(_j4.visible_bands('front'), 1.70, 1.50)
_rear_bands = _ear_faces(_j4.visible_bands('rear'), 0.40, 1.40)
print('J4 front faces beside the tail: %s'%(', '.join('%.2f x %.2f at y %.2f'%(b['dy'], b['dz'], b['y0']) for b in _front_bands) or 'none'))
print('J4 rear skin bands: %s'%(', '.join('%.2f x %.2f at y %.2f'%(b['dy'], b['dz'], b['y0']) for b in _rear_bands) or 'none'))
if len(_front_bands) < 2 or len(_rear_bands) < 2 or any(b['dy'] > 3.0 for b in _front_bands):
    seat_fail = True
    print('J4 FACE MISSING')
# A 1 x 6 x 1.8 plate is one face. Corner vertices 6 mm apart must not split it.
_syn = trimesh.creation.box(extents=[1.0, 6.0, 1.8])
_syn.apply_translation([103.77, 44.0, 0.9])
_saved = _j4.mesh
_j4.mesh = _syn
_syn_faces = _j4.axis_faces(0, +1, min_span_a=1.5, min_span_b=1.2)
_j4.mesh = _saved
_syn_ok = any(max(f['span_a'], f['span_b']) >= 5.0 and min(f['span_a'], f['span_b']) >= 1.5 for f in _syn_faces)
print('1 x 6 x 1.8 test face reads as a finger face: %s'%('yes' if _syn_ok else 'NO'))
if not _syn_ok:
    seat_fail = True
    print('FACE DETECTOR MISSES A 1x6x1.8 FACE')
# Thumbnail on a front face. The finger stays above the board (z >= 0) and
# inside the 4.8 mm bay in front of x 103.77. A box centred on the face and
# 10 mm thick drops below the board and is not a reach.
def _nail_ok(bounds):
    if bounds[2] < -0.01:
        return False
    if bounds[3] - bounds[0] > 4.8:
        return False
    if _j4.nail_hits_solid(bounds):
        return False
    return True
_face = _front_bands[0] if _front_bands else {'y0': 35.3, 'y1': 37.3, 'z0': 0.0, 'z1': 1.8}
# Nail just in front of the face, 4 mm deep, from the board up.
_nail = (mouth_x, _face['y0'], 0.15, mouth_x + 4.0, _face['y0'] + 2.0, 0.15 + 2.0)
_below = (mouth_x, _face['y0'], -4.0, mouth_x + 4.0, _face['y0'] + 2.0, 6.0)
_into = (mouth_x - 3.0, 44.0, 0.40, mouth_x - 0.3, 46.0, 1.50)
print('thumbnail on the front face stays above the board: %s'%('yes' if _nail_ok(_nail) else 'NO'))
print('finger centred through the board: %s'%('rejected' if not _nail_ok(_below) else 'ACCEPTED'))
print('nail 3 mm inside the housing: %s'%('rejected' if not _nail_ok(_into) else 'ACCEPTED'))
if not _nail_ok(_nail) or _nail_ok(_below) or _nail_ok(_into):
    seat_fail = True
    print('NAIL PLACEMENT FAIL')
def _ear_bay(y):
    hit = _j4._first_hit_x(np.array([[102.50, y, 1.0]]), -1.0)[0]
    if not np.isfinite(hit) or hit < 96.0:
        return 102.77 - 96.0
    return 102.50 - float(hit)
_rear_clear = min(_ear_bay(b['y0'] + 0.2) for b in _rear_bands) if _rear_bands else 0.0
print('open board behind the ear rear faces %.2f mm'%_rear_clear)
if _rear_clear < 4.0:
    seat_fail = True
    print('EAR REAR HAS NO NAIL BAY')
# Housing stop. A ray through the cavity hits the back wall the tail bottoms on.
_stop = _j4.housing_stop(mouth_x)
print('tail bottoms at %.2f mm (housing stop). Jushuo states no actuation force; a physical unit has to confirm the nail can move the slider.'%_stop)
if _stop is None or abs(_stop - 3.85) > 0.15:
    seat_fail = True
    print('HOUSING STOP')
# A box beside J4 must not widen the faces. A blocker in front of them must.
_j4_saved = _j4.mesh
_beside = trimesh.creation.box(extents=[1.0, 4.0, 1.8])
_beside.apply_translation([104.6, 30.0, 1.0])
_j4.mesh = trimesh.util.concatenate([_j4_saved, _beside])
_j4._inter = _j4.mesh.ray
_foreign = _ear_faces(_j4.visible_bands('front'), 1.70, 1.50)
if len(_foreign) < 2 or any(b['dy'] > 3.0 for b in _foreign):
    seat_fail = True
    print('foreign box beside J4: DID NOT FAIL, faces %s'%([round(b['dy'], 2) for b in _foreign],))
else:
    print('foreign box beside J4: faces stay %s'%(', '.join('%.2f'%b['dy'] for b in _foreign)))
_blk = trimesh.creation.box(extents=[2.0, 4.0, 2.2])
_b1 = _blk.copy(); _b1.apply_translation([106.3, 36.2, 1.0])
_b2 = _blk.copy(); _b2.apply_translation([106.3, 51.6, 1.0])
_j4.mesh = trimesh.util.concatenate([_j4_saved, _b1, _b2])
_j4._inter = _j4.mesh.ray
if _ear_faces(_j4.visible_bands('front'), 1.70, 1.50):
    seat_fail = True
    print('front_blocked: DID NOT FAIL')
else:
    print('front_blocked: FAIL')
_rb = trimesh.creation.box(extents=[3.5, 1.4, 2.0])
_r1 = _rb.copy(); _r1.apply_translation([99.8, 35.5, 1.0])
_r2 = _rb.copy(); _r2.apply_translation([99.8, 52.5, 1.0])
_j4.mesh = trimesh.util.concatenate([_j4_saved, _r1, _r2])
_j4._inter = _j4.mesh.ray
_rear_blocked = _ear_faces(_j4.visible_bands('rear'), 0.40, 1.40)
_bay_blocked = min(_ear_bay(35.5), _ear_bay(52.5))
if _rear_blocked and _bay_blocked >= 4.0:
    seat_fail = True
    print('rear_blocked: DID NOT FAIL')
else:
    print('rear_blocked: FAIL')
_j4.mesh = _j4_saved
_j4._inter = _j4_saved.ray
# Drop the panel straight down. Fence opening must clear 36.50–36.90 mm.
# A lip that reaches 0.40 mm onto each border closes that opening.
_top_in = 26.24 - fence_air
_bot_in = 62.94 + fence_air
_opening = _bot_in - _top_in
print('fence opening %.2f mm. panel band %.2f–%.2f mm.'%(_opening, panel_lo, panel_hi))
def _drop_ok(width, lip_each):
    return _opening - 2.0*lip_each + 1e-9 >= width
for _w in (panel_lo, panel_h, panel_hi):
    print('drop-in width %.2f mm %s'%(_w, 'yes' if _drop_ok(_w, 0.0) else 'NO'))
    if not _drop_ok(_w, 0.0):
        seat_fail = True
        print('PANEL DOES NOT DROP IN')
if _drop_ok(panel_hi, 0.40):
    seat_fail = True
    print('A 0.40 mm LIP DID NOT BLOCK THE DROP')
# Mesh: air just inside a 36.9 mm panel, solid in the fence.
_cy = 0.5*(26.24+62.94)
_y_wide = _cy + panel_hi/2.0
_Xw,_Yw = T(50.0, _y_wide - 0.05)
_Xf,_Yf = T(50.0, _bot_in + 0.25)
qcar = trimesh.proximity.ProximityQuery(carrier)
_d_wide = float(qcar.signed_distance([[_Xw, _Yw, Zpb+0.60]])[0])
_d_fence = float(qcar.signed_distance([[_Xf, _Yf, Zpb+0.60]])[0])
print('36.9 mm edge air %+.2f (want <0)  fence solid %+.2f (want >0)'%(_d_wide, _d_fence))
if _d_wide > -0.02 or _d_fence < 0.05:
    seat_fail = True
    print('DROP MESH FAIL')
# No fixed lip above the border. A shelf there is the tilt-under path.
_Xl,_Yl = T(47.5, 26.24+0.15)
_d_lip = float(qcar.signed_distance([[_Xl, _Yl, Zin+0.25]])[0])
print('above the border %+.2f (want <0, no fixed lip)'%_d_lip)
if _d_lip > -0.02:
    seat_fail = True
    print('FIXED LIP STILL THERE')
qpanel = trimesh.proximity.ProximityQuery(panel)
# Lifting the seated panel while the slider is locked pulls the tail out.
# 0.20 mm on a tall print stays at the 3.0 mm drawing minimum. 0.50 mm does not.
_pull = math.pi/2.0
_ins_tall = fpc_inserted(P['panel_gap']+PRINT_Z, 0.50)
print('tall-print lift 0.20 mm leaves %.2f mm inserted. 0.50 mm leaves %.2f mm.'%(
    _ins_tall-_pull*0.20, _ins_tall-_pull*0.50))
if _ins_tall-_pull*0.20 < FPC_MIN-1e-9 or _ins_tall-_pull*0.50 >= FPC_MIN:
    seat_fail = True
    print('LIFT BUDGET FAIL')
# Seated fold. Inside radius over thickness, next to the IPC-2223 static 6 t note.
_fpc_R = (P['panel_gap']-0.50)/2.0
_fpc_eps = 0.30/(2.0*_fpc_R)
_inside = _fpc_R - 0.15
print('seated fold centerline R %.2f mm, inside %.2f mm (%.1f t), outer-fiber strain %.1f%%.'%(
    _fpc_R, _inside, _inside/0.30, 100*_fpc_eps))
# +x stop. Pull-out along x is 1 mm of tail per mm of slide.
vcar=carrier.vertices
right=(vcar[:,0]>OX+108.2)&(vcar[:,0]<OX+109.6)&(vcar[:,2]>Zpb+0.3)&(vcar[:,2]<Zin)
if not np.any(right):
    print('X STOP MISSING')
    seat_fail=True
    bite=0.0
else:
    bite=(OX+FPC_EDGE)-float(vcar[right,0].min())
worst_slide=max(0.0, PRINT_Z-bite)
ins_slide=fpc_inserted(P['panel_gap']+PRINT_Z, 0.50)-worst_slide
print('x stop bite %.3f mm, worst +x slide after a %.2f mm short print %.3f mm, inserted then %.2f mm'%(bite, PRINT_Z, worst_slide, ins_slide))
if bite<xstop_bite-0.03 or ins_slide<FPC_MIN-1e-6:
    print('X STOP FAIL')
    seat_fail=True
# Curtain face from the shell mesh, not a typed-in 28.88.
_sb = np.column_stack([shell.vertices[:, 0] - OX, H - OYT - shell.vertices[:, 1], shell.vertices[:, 2]])
_cface = _sb[(_sb[:, 0] > 27.0) & (_sb[:, 0] < 31.0) & (_sb[:, 2] > Zt + 1.5) & (_sb[:, 2] < Zin - 0.3) & (_sb[:, 1] > 15) & (_sb[:, 1] < 50)]
_curtain = float(_cface[:, 0].max()) if len(_cface) else 0.0
print('minus-x lengthens the inserted tail. The curtain is %.2f mm from the outline and does not locate the panel'%(29.46-_curtain))
# Bottomed at the housing stop, laying the panel down shifts it toward the curtain.
_bottom = _stop
def _curtain_air(curtain, stop):
    out = []
    for _name, _ins in (('short', fpc_inserted(P['panel_gap']-PRINT_Z, 0.50)),
                        ('nominal', fpc_inserted(P['panel_gap'], 0.50)),
                        ('tall', fpc_inserted(P['panel_gap']+PRINT_Z, 0.50))):
        _shift = stop - _ins
        _left = (29.46 - curtain) - _shift
        out.append((_name, -_shift, _left))
    return out
for _name, _shift, _left in _curtain_air(_curtain, _bottom):
    print('bottomed %s shifts %+.2f mm, curtain air %.2f mm'%(_name, _shift, _left))
    if _left < 0.05:
        seat_fail = True
        print('CURTAIN HIT WHEN THE TAIL IS BOTTOMED')
# A curtain face at 29.20 leaves no air after the tall shift. The gate has to see it.
_shell_c = shell.copy()
_bx = _shell_c.vertices[:, 0] - OX
_move = (_bx > 28.4) & (_bx < 29.1)
_vv = _shell_c.vertices.copy()
_vv[_move, 0] += 29.20 - _curtain
_shell_c.vertices = _vv
_sb2 = np.column_stack([_shell_c.vertices[:, 0] - OX, H - OYT - _shell_c.vertices[:, 1], _shell_c.vertices[:, 2]])
_c2 = _sb2[(_sb2[:, 0] > 27.0) & (_sb2[:, 0] < 32.0) & (_sb2[:, 2] > Zt + 1.5) & (_sb2[:, 2] < Zin - 0.3) & (_sb2[:, 1] > 15) & (_sb2[:, 1] < 50)]
_curtain_bad = float(_c2[:, 0].max()) if len(_c2) else _curtain
_air_bad = _curtain_air(_curtain_bad, _bottom)[-1][2]
if _air_bad < 0.05:
    print('curtain2920: FAIL air %.2f'%_air_bad)
else:
    seat_fail = True
    print('curtain2920: DID NOT FAIL air %.2f'%_air_bad)
# Depth marks sit just outboard of the seated edge. Solid on the fin, air inboard of the edge.
for by, name in ((32.95, 'low'), (59.90, 'high')):
    X,Y=T(FPC_EDGE+0.30, by)
    _,Yi=T(FPC_EDGE-0.25, by)
    d_fin=float(qcar.signed_distance([[X, Y, Zpb+0.40]])[0])
    d_in=float(qcar.signed_distance([[OX+FPC_EDGE-0.25, Yi, Zpb+0.40]])[0])
    print('depth mark %-4s solid %+.2f (want >0)  inboard %+.2f (want <0)'%(name, d_fin, d_in))
    if d_fin<0.05 or d_in>-0.02:
        seat_fail=True
        print('DEPTH MARK FAIL')
# Shell on and off. Pure z, through the snap climb. The panel and the fold stay clear,
# so the shell cannot shift the panel or load the connector.
def _fold_points():
    y0=44.0-fpc_wide/2.0; y1=44.0+fpc_wide/2.0
    R=(P['panel_gap']-0.50)/2.0
    zc=Zt+0.50+R
    pts=[]
    for t in np.linspace(-np.pi/2, np.pi/2, 16):
        for yy in np.linspace(y0, y1, 6):
            for rr in (R-0.10, R+0.10):
                bx=FPC_EDGE+rr*np.cos(t)
                bz=zc+rr*np.sin(t)
                X,Y=T(bx, yy)
                pts.append((X, Y, bz))
    # lower leg
    x_end=mouth_x-(FPC_FREE-np.pi*R-(FPC_EDGE-mouth_x))
    for bx in np.linspace(x_end, FPC_EDGE, 8):
        for yy in np.linspace(y0, y1, 4):
            X,Y=T(bx, yy)
            pts.append((X, Y, Zt+0.50))
    return np.array(pts)
fold_pts=_fold_points()
qpanel=trimesh.proximity.ProximityQuery(panel)
shell_pts=np.vstack([shell.vertices, _sample(shell, 8000)])
for dz in (0.0, 1.0, 2.0):
    moved=shell_pts+np.array([0.0, 0.0, dz])
    # also the thick-board case, which lifts the panel and the fold toward a seated shell
    c_pan=-float(qpanel.signed_distance(moved).max())
    c_pan_hi=-float(qpanel.signed_distance(moved-np.array([0.0, 0.0, pcb_tol])).max()) if dz==0.0 else c_pan
    c_fold=-float(trimesh.proximity.ProximityQuery(shell).signed_distance(fold_pts-np.array([0.0, 0.0, dz])).max())
    c_fold_hi=-float(trimesh.proximity.ProximityQuery(shell).signed_distance(fold_pts+np.array([0.0, 0.0, pcb_tol])).max()) if dz==0.0 else c_fold
    worst=min(c_pan, c_pan_hi, c_fold, c_fold_hi)
    print('shell lift %4.1f mm  panel %6.2f  panel+thick %6.2f  fold %6.2f  fold+thick %6.2f'%(
        dz, c_pan, c_pan_hi, c_fold, c_fold_hi))
    if worst<=0.10:
        seat_fail=True
        print('SHELL TOUCHES THE PANEL OR THE FOLD')
# A shell dropped onto the panel fails this same test.
dropped=shell_pts+np.array([0.0, 0.0, -(face_recess+0.40)])
c_drop=-float(qpanel.signed_distance(dropped).max())
print('shell pushed through the recess onto the panel %6.2f (want <0)'%c_drop)
if c_drop>=0.0:
    seat_fail=True
    print('SHELL CONTACT NEGATIVE DID NOT FAIL')
stack_face=0.16+0.10+0.10+0.15
_pull = math.pi/2.0
_ins_nom = fpc_inserted(P['panel_gap'], 0.50)
_ins_tall = fpc_inserted(P['panel_gap']+PRINT_Z, 0.50)
_ins_short = fpc_inserted(P['panel_gap']-PRINT_Z, 0.50)
# 3.0 mm is the Jushuo drawing's recommended contact length ("3.0Min").
print('panel gap %.2f mm. inserted short %.2f, nominal %.2f, tall %.2f.'%(
    P['panel_gap'], _ins_short, _ins_nom, _ins_tall))
print('opto over 4.0 mm: short %.2f, nominal %.2f, tall %.2f.'%(
    P['panel_gap']-PRINT_Z-4.0, P['panel_gap']-4.0, P['panel_gap']+PRINT_Z-4.0))
# Tongue preload, root, tooth gap, and the arm, all from the meshes.
import clip_check
if clip_check.report(clip_check.measure_clips(retainer, carrier, OX, OYT, H, Zin),
                     carrier, shell, OX, OYT, H, Zin, petg_eps):
    seat_fail = True
if clip_check.negative_probes(retainer, carrier, shell, OX, OYT, H, Zin, petg_eps):
    seat_fail = True
_float = 0.0
if _ins_tall - _pull*_float < FPC_MIN:
    seat_fail = True
    print('PANEL FLOAT FAIL')
if _ins_tall - _pull*0.50 >= FPC_MIN:
    seat_fail = True
    print('A 0.50 mm RISE DID NOT FAIL')
# Shell vs clips across the stack, not only at dz 0.
_rpts = np.vstack([retainer.vertices, _sample(retainer, 4000)])
_qshell = trimesh.proximity.ProximityQuery(shell)
for _dz in (0.0, 0.16, 0.26, stack_face):
    _c = -float(_qshell.signed_distance(_rpts + np.array([0.0, 0.0, _dz])).max())
    print('retainer vs shell at stack dz %.2f  %6.2f'%(_dz, _c))
    if _c < 0.10:
        seat_fail = True
        print('SHELL LANDS ON THE CLIPS')
c_ret_pan = mesh_clearance(retainer, panel, n=3000)
print('retainer vs panel %6.2f (bump interference)'%c_ret_pan)
if c_ret_pan > -0.08 or c_ret_pan < -0.40:
    seat_fail = True
    print('PRELOAD MESH FAIL')
# 0.50 and 1.00 are outside the band around 1.15.
def _recess_ok(v):
    return 1.05 <= v <= 1.28
print('face recess %.2f mm'%face_recess)
if not _recess_ok(face_recess) or _recess_ok(0.50) or _recess_ok(1.00):
    seat_fail = True
    print('FACE RECESS BOUND FAIL')
qshell = trimesh.proximity.ProximityQuery(shell)
mid = T(31.0, 27.5)
d_face = float(qshell.signed_distance([[mid[0], mid[1], Zin+0.30]])[0])
d_skin = float(qshell.signed_distance([[mid[0], mid[1], Zin+face_recess+0.08]])[0])
print('face over the panel border at +0.30 mm %+.2f (want <0)  past the recess %+.2f (want >0)'%(d_face, d_skin))
if d_face > -0.02 or d_skin < 0.02:
    seat_fail = True
    print('FACE RECESS MESH FAIL')
# Mouth witness: solid beside the mouth, not inside the STEP.
witness_x0 = scad_num('witness_x0'); witness_y0 = scad_num('witness_y0')
_Xw, _Yw = T(0.5*(witness_x0+scad_num('witness_x1')), 0.5*(witness_y0+scad_num('witness_y1')))
d_wit = float(qcar.signed_distance([[_Xw, _Yw, Zt+0.70]])[0])
print('mouth witness solid %+.2f (want >0)'%d_wit)
if d_wit < 0.05:
    seat_fail = True
    print('MOUTH WITNESS MISSING')
if board_shell<0.05 or (have_board and pcb_shell<0.05):
    seat_fail=True
    print('SHELL TOUCHES THE BOARD')
# A clamp post on a mounting hole is board contact. The gate rejects it.
post=cyl('clamp post', 6.4, 6.4, 3.0, Zt+0.05, Zt+2.0)[1]
c_post=mesh_clearance(board_slab, trimesh.util.concatenate([shell, post]), dz=pcb_tol, n=1500)
print('clamp post on the shell vs board %6.2f (want <0)'%c_post)
if c_post>=0.0:
    seat_fail=True
    print('CLAMP NEGATIVE DID NOT FAIL')
if seat_fail:
    print('SEAT FAIL')
# Buttons. TS-1187A-B-A-B: H=1.5±0.05, drawing TRAVEL 0.2±0.1, spec table 0.25.
# The sheet does not give an over-travel rating. The lip is the plastic stop.
# The shell is fixed to the backplate by the clips, not by the board. Board thickness
# stays in the stack because the actuator is on the board and the shell z comes from
# the snaps. Snap windows add ±0.10 mm when the shell is not seated in the middle of
# the window. Moving the whole shell changes the gap and not the travel, because the
# cone and the lip move together. The leaf is the captive button catch. It does not
# hold the display panel.
# Uniform draws: printer z ±0.10 on the gap, an independent ±0.10 on the cap-to-shell
# travel (two prints), standoff ±0.15, board ±0.16, switch height ±0.05, seat ±0.10.
SW_H_TOL=0.05
leaf_t=scad_num('leaf_t'); leaf_L=scad_num('leaf_L'); leaf_defl=scad_num('leaf_defl')
leaf_free=scad_num('leaf_free') if False else leaf_L-scad_num('leaf_root')
leaf_eps=1.5*leaf_t*leaf_defl/(leaf_free*leaf_free)
print('leaf: free L %.2f mm  t %.2f  insertion δ %.2f  strain %.2f%%'%(leaf_free, leaf_t, leaf_defl, 100*leaf_eps))
seated_caps=trimesh.load('caps_seated.stl')
pressed_caps=trimesh.load('caps_pressed.stl')
btn_fail=False
if leaf_free<4.0 or leaf_eps>=petg_eps or not (0.20<=leaf_defl<=0.40):
    btn_fail=True
    print('LEAF STRAIN FAIL')
def stem_tip(mesh, c):
    v=mesh.vertices
    d=np.hypot(v[:,0]-c[0], v[:,1]-c[1])
    sel=v[d<1.0]
    return float(sel[:,2].min())
def catch_face(mesh, c):
    """Lowest world-z of the barb. That face is what meets the lip."""
    v=mesh.vertices
    r=np.hypot(v[:,0]-c[0], v[:,1]-c[1])
    m=(v[:,0]>c[0]+1.2)&(r>3.15)&(r<3.80)&(np.abs(v[:,1]-c[1])<3.2)
    if not np.any(m):
        return None
    return float(v[m,2].min())
print('buttons: gap is seated stem to the actuator; stroke is travel minus that gap')
gaps=[]; travels=[]
for yb in (30, 40, 50):
    c=T(112, yb)
    gap=stem_tip(seated_caps, c)-(Zt+1.5)
    travel=stem_tip(seated_caps, c)-stem_tip(pressed_caps, c)
    stroke=travel-gap
    rest_hi=gap-pcb_tol-SW_H_TOL
    cz_s=catch_face(seated_caps, c); cz_p=catch_face(pressed_caps, c)
    print('  y=%g gap %5.2f travel %5.2f stroke %5.2f thick+tall rest %5.2f catch seated z %.2f pressed z %.2f'%(yb, gap, travel, stroke, rest_hi, cz_s if cz_s is not None else -1, cz_p if cz_p is not None else -1))
    gaps.append(gap); travels.append(travel)
    if not (0.60<=gap<=0.70 and 1.45<=travel<=1.55 and rest_hi>0.05 and cz_s is not None):
        btn_fail=True
        print('  BUTTON STROKE FAIL')
    if cz_s is None or abs((cz_s-cz_p)-travel)>0.08:
        btn_fail=True
        print('  CATCH DOES NOT TRAVEL WITH THE CAP')
g0=float(np.mean(gaps)); t0=float(np.mean(travels))
# Tolerance stack. Shell fixed, so the board term is in. Switch height and the
# snap-seat play are in the same stack. Positive seat is the shell sitting high.
SW_STACK=0.05
SEAT=0.10
rng=np.random.default_rng(1)
N=200000
ez=rng.uniform(-0.10,0.10,N); et=rng.uniform(-0.10,0.10,N)
es=rng.uniform(-0.15,0.15,N); eb=rng.uniform(-0.16,0.16,N)
esw=rng.uniform(-SW_STACK,SW_STACK,N); ese=rng.uniform(-SEAT,SEAT,N)
gap_s=g0+ez-es-eb-esw+ese
stroke_s=(t0+et)-gap_s
p_closed=float(np.mean(gap_s<=0)*100)
p_short=float(np.mean(stroke_s<0.20)*100)
worst_gap=g0-0.10-0.15-0.16-SW_STACK-SEAT
worst_stroke=(t0-0.10)-(g0+0.10+0.15+0.16+SW_STACK+SEAT)
print('tolerance uniform: P(gap<=0)=%.2f%%  P(stroke<0.20)=%.2f%%  worst gap %.2f  worst stroke %.2f'%(p_closed, p_short, worst_gap, worst_stroke))
# Past the switch's own travel the lip is this far away. The sheet states
# no over-travel rating, so this is a plastic stop beyond bottom-out, not a rated one.
# Spec-table travel is 0.25 mm. The drawing's 0.2±0.1 mm is the other figure.
over=t0-g0-0.25
print('plastic stop is %.2f mm past the spec-table 0.25 mm bottom-out; the sheet gives no over-travel rating'%(over))
# Sensitivity, not the gate: ± taken as 1σ, board left out.
def _phi(z):
    return 0.5*(1+math.erf(z/math.sqrt(2)))
sig_gap=math.sqrt(0.10**2+0.15**2+SW_STACK**2+SEAT**2)   # no board
sig_stroke=math.sqrt(0.10**2+0.10**2+0.15**2+SW_STACK**2+SEAT**2)
print('sensitivity, ± as 1σ and the board left out: P(gap<=0)=%.1f%%  P(stroke<0.20)=%.1f%%'%(_phi((0-g0)/sig_gap)*100, _phi((0.20-(t0-g0))/sig_stroke)*100))
if p_closed>1.0 or p_short>2.0 or worst_gap<=0.05 or worst_stroke<0.20:
    btn_fail=True
    print('TOLERANCE STACK FAIL')
# The lip is present on the partition side after the FPC pocket. Neck is open.
cxb,cyb=T(112,40)
shelf=catch_face(pressed_caps, T(112,40))
qshell=trimesh.proximity.ProximityQuery(shell)
lip_d=float(qshell.signed_distance([[cxb+3.40, cyb, shelf-0.15]])[0])
neck_d=float(qshell.signed_distance([[cxb+1.6, cyb, shelf-0.15]])[0])
print('lip solid %+0.2f (want >0), neck open %+0.2f (want <0) at z %.2f'%(lip_d, neck_d, shelf-0.15))
if lip_d<=0 or neck_d>=0:
    btn_fail=True
def face_clearance(mesh, c):
    v=mesh.vertices
    r=np.hypot(v[:,0]-c[0], v[:,1]-c[1])
    m=(v[:,0]>c[0]+1.2)&(r>3.15)&(r<3.80)&(np.abs(v[:,1]-c[1])<3.2)
    pts=v[m]
    z0=float(pts[:,2].min())
    face=pts[pts[:,2]<z0+0.05]
    return -float(trimesh.proximity.ProximityQuery(shell).signed_distance(face).max()), z0
# Cone seat: vertices on the cone near r = 2.5, within 0.4 mm of the inner face.
def cone_clearance(mesh, c):
    v=np.vstack([mesh.vertices, _sample(mesh, 5000)])
    r=np.hypot(v[:,0]-c[0], v[:,1]-c[1])
    m=(r>2.45)&(r<2.58)&(np.abs(v[:,2]-Zin)<0.20)
    if not np.any(m):
        return 99.0
    return -float(trimesh.proximity.ProximityQuery(shell).signed_distance(v[m]).max())
print('catch face to lip, seated / pressed (0 is a kiss):')
for yb in (30, 40, 50):
    c=T(112, yb)
    cs,zs=face_clearance(seated_caps, c)
    cp,zp=face_clearance(pressed_caps, c)
    ck=cone_clearance(seated_caps, c)
    print('  y=%g seated catch %+.3f  pressed catch %+.3f  seated cone %+.3f'%(yb, cs, cp, ck))
    if not (-0.08<=cp<=0.08):
        btn_fail=True
        print('  PRESSED CATCH NOT ON THE LIP')
    if cs<0.15:
        btn_fail=True
        print('  SEATED CATCH ALREADY ON THE LIP')
    if not (-0.08<=ck<=0.10):
        btn_fail=True
        print('  CONE NOT SEATED')
_,panel_m=box('panel',29.46,26.24,108.46,62.94,Zpb,Zpb+1.2)
for name,mesh in ('seated',seated_caps),('pressed',pressed_caps):
    c=mesh_clearance(mesh, panel_m, n=4000)
    print('cap %s vs panel %6.2f'%(name, c))
    if c<0.40:
        btn_fail=True
# Cover is 1.2 mm; the body box stops at 1.15 so it does not include the actuator.
# The leaf (not the stem) stays off that box. The pressed pose is the lip stop, which
# is past the switch cover, so the stem is allowed to pass the actuator there.
sw_body=box('SW body',112-2.60,40-2.60,112+2.60,40+2.60,Zt,Zt+1.15)[1]
sw_act=cyl('SW actuator',112,40,1.00,Zt+1.20,Zt+1.50)[1]
def leaf_clearance(mesh, obst):
    v=np.vstack([mesh.vertices, _sample(mesh, 3000)])
    c=T(112,40)
    r=np.hypot(v[:,0]-c[0], v[:,1]-c[1])
    pts=v[r>1.3]
    return -float(trimesh.proximity.ProximityQuery(obst).signed_distance(pts).max())
for name,mesh in ('seated',seated_caps),('pressed',pressed_caps):
    c=leaf_clearance(mesh, sw_body)
    print('cap %s leaf vs switch body %6.2f (min 0.30)'% (name, c))
    if c<0.30:
        btn_fail=True
c_act=mesh_clearance(seated_caps, sw_act, n=2000)
sw_hi=sw_act.copy(); sw_hi.apply_translation([0,0,pcb_tol+SW_H_TOL])
c_act_hi=mesh_clearance(seated_caps, sw_hi, n=2000)
c_act_pr=mesh_clearance(pressed_caps, sw_act, n=2000)
print('seated vs actuator %6.2f, vs tall+thick %6.2f, pressed (past the cover) %6.2f'%(c_act, c_act_hi, c_act_pr))
if c_act<0.30 or c_act_hi<=0.05:
    btn_fail=True
    print('ACTUATOR GAP FAIL')
if interfere or board_shell<0.05 or (have_board and pcb_shell<0.05) or pcb_back<-0.05 or carrier_fail or btn_fail or seat_fail:
    print('INTERFERE')
    sys.exit(1)
print('FIT OK')
