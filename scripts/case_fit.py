# Clearance of the populated board against the case, plus the FPC fold and the panel carrier.
# board.stl is not committed. From the repo root, with KiCad 9, and without
# --drill-origin (this script places the export itself):
#   kicad-cli pcb export stl -o board.stl --force --cut-vias-in-body \
#       kicad/thermostat-v3.kicad_pcb
# Assembly-frame solids, not the flipped print STLs:
#   OPENSCADPATH=case openscad -o back.stl -D 'part="none"' -D 'which="b"' scripts/case_fit_export.scad
#   OPENSCADPATH=case openscad -o shell.stl -D 'part="none"' -D 'which="s"' scripts/case_fit_export.scad
#   OPENSCADPATH=case openscad -o carrier.stl -D 'part="none"' -D 'which="c"' scripts/case_fit_export.scad
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
H=71; OX=3.5; OYT=3.5
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
# bend at the glass back. Neither datasheet states which face the tail leaves, and
# neither states a minimum bend radius (the connector's R0.20 is a housing fillet).
front_exit=fpc_inserted(P['panel_gap']+1.2, 0.50)
print('if the tail left the glass front face instead, inserted %.2f mm at this gap'%(front_exit))
opto_h=4.0
print('opto clearance nominal %.2f mm, if the carrier prints short %.2f mm'%(P['panel_gap']-opto_h, P['panel_gap']-PRINT_Z-opto_h))
if P['panel_gap']-PRINT_Z-opto_h < 0.40:
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
for req in ('back.stl','shell.stl','carrier.stl','caps_seated.stl','caps_pressed.stl'):
    if not os.path.exists(req):
        print('missing', req)
        sys.exit(1)
back=trimesh.load('back.stl'); shell=trimesh.load('shell.stl'); carrier=trimesh.load('carrier.stl')
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
obst={'backplate':back,'shell':shell,'e-paper panel':panel}
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
# board outline vs shell. Both of these are a designed kiss (0, or a numeric -0.0).
# Sampling the contact can report a few hundredths of a millimetre of overlap, so
# -0.05 mm of slack is allowed here only. Part rows above still fail on any c<0.
pcb_shell=0.0; pcb_back=0.0
if have_board:
    d=trimesh.proximity.ProximityQuery(shell).signed_distance(np.vstack([pcbm.vertices,_sample(pcbm,20000)]))
    pcb_shell=-float(d.max())
    print('PCB outline vs shell',round(pcb_shell,2))
    d=trimesh.proximity.ProximityQuery(back).signed_distance(np.vstack([pcbm.vertices,_sample(pcbm,20000)]))
    pcb_back=-float(d.max())
    print('PCB vs backplate (standoff seat expected 0)',round(pcb_back,2))
else:
    print('PCB outline checks skipped (no board.stl)')

def mesh_clearance(part, obst, dz=0.0, n=6000):
    """Positive means air between the solids. dz lifts a second copy of the samples."""
    pts=np.vstack([part.vertices, _sample(part, n)])
    if dz:
        pts=np.vstack([pts, pts+np.array([0,0,dz])])
    return -float(trimesh.proximity.ProximityQuery(obst).signed_distance(pts).max())

carrier_fail=False
bodies=carrier.split(only_watertight=False)
print('carrier bodies', len(bodies))
if len(bodies)>1:
    # Touching bodies still print as one piece. A real gap does not.
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
# The shelves kiss the panel back and the ledges kiss the glass face. A few hundredths is the mesh.
c_panel=mesh_clearance(carrier, panel, n=8000)
print('carrier vs panel (kiss expected near 0) %6.2f'%c_panel)
if c_panel<-0.08:
    carrier_fail=True
tall=[
 box('U6 opto x4.0',28.8,6.3,40.2,14.1,Zt,Zt+4.0),
 box('U7 opto x4.0',28.8,21.3,40.2,29.1,Zt,Zt+4.0),
 box('U8 opto x4.0',28.8,36.3,40.2,44.1,Zt,Zt+4.0),
 box('L1 SWPA6045 x4.5',21.1,53.5,27.1,59.5,Zt,Zt+4.5),
 box('J4 body H2.0',98.5,34.8,103.9,53.0,Zt,Zt+2.0),
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
# Hooks. The beam is free beside the glass; only cl_foot ties it to the pad.
# E = 1.8 GPa. ε = 1.5 t δ / L^2, F = 3 E I δ / L^3, I = w t^3 / 12, w = 6 mm.
# The tool window is in the head at the free end. The root section that sets F is the full beam.
cl_t=scad_num('cl_t'); cl_gap=scad_num('cl_gap'); cl_lip=scad_num('cl_lip')
cl_lip_h=scad_num('cl_lip_h'); cl_foot=scad_num('cl_foot')
cl_slot_w=scad_num('cl_slot_w'); cl_slot_gap=scad_num('cl_slot_gap'); cl_slot_wall=scad_num('cl_slot_wall')
cl_head=scad_num('cl_head'); cl_head_above=scad_num('cl_head_above'); cl_face_below=scad_num('cl_face_below')
glass_lo=scad_num('glass_lo'); glass_hi=scad_num('glass_hi'); panel_h=scad_num('panel_h'); rel_air=scad_num('rel_air')
petg_eps=scad_num('petg_eps')
xstop_bite=scad_num('xstop_bite'); face_recess=scad_num('face_recess')
Zin=Zt+P['panel_gap']+1.2
cl_L=Zin-(Zt+cl_foot)
cl_E=1800.0
cl_I=6.0*cl_t**3/12.0
cl_eps=1.5*cl_t*cl_lip/(cl_L*cl_L)
cl_F=3*cl_E*cl_I*cl_lip/(cl_L**3)
print('hooks: L %.2f mm  t %.2f  lip %.2f  strain %.2f%%  force %.2f N/hook'%(cl_L, cl_t, cl_lip, 100*cl_eps, cl_F))
hook_fail=False
# The ramp rises by cl_lip while it steps back by cl_lip, so the lead-in is 45 deg.
if cl_L<5.0 or cl_eps>=petg_eps or cl_F>=8.0 or abs(math.degrees(math.atan2(cl_lip, cl_lip))-45)>1:
    hook_fail=True
    print('HOOK BEAM FAIL')
qcar=trimesh.proximity.ProximityQuery(carrier)
def _cy(by):
    return H-OYT-by
for x0,x1,edge,dire in ((47.0,53.0,26.24,1),(84.6,90.6,26.24,1),(49.6,55.6,62.94,-1),(76.0,82.0,62.94,-1)):
    xm=OX+0.5*(x0+x1)
    def _at(u, z):
        return [xm, _cy(edge+dire*u), z]
    d_beam=float(qcar.signed_distance([_at(-(cl_gap+cl_t/2), Zt+cl_foot+1.0)])[0])
    d_gap=float(qcar.signed_distance([_at(-cl_gap/2, Zt+cl_foot+1.0)])[0])
    d_foot=float(qcar.signed_distance([_at(-cl_gap/2, Zt+cl_foot/2)])[0])
    print('  hook x %.1f beam %+.2f (want >0)  gap %+.2f (want <0)  foot %+.2f (want >0)'%(0.5*(x0+x1), d_beam, d_gap, d_foot))
    if d_beam<0.05 or d_gap>-0.05 or d_foot<0.05:
        hook_fail=True
        print('  HOOK NOT FREE')
    # Window path. A 1.0 mm blade comes down from above the head into the opening,
    # 0.12 mm off the push face so the volume itself is air, then the face it bears
    # on is solid.
    u_outer=-(cl_gap+cl_t)
    u_push=u_outer-cl_slot_gap
    z_lo=Zin-cl_face_below+0.35
    z_hi=Zin+cl_head_above+0.70
    ya=edge+dire*(u_push+0.12); yb=edge+dire*(u_push+1.12)
    blade=box('slot blade', 0.5*(x0+x1)-1.60, min(ya,yb),
              0.5*(x0+x1)+1.60, max(ya,yb), z_lo, z_hi)[1]
    slot_blocked=False
    worst_air=-9
    for dz in (0.0, 0.6, 1.2, 1.8, 2.4):
        pts=np.vstack([blade.vertices, _sample(blade, 800)])+np.array([0, 0, dz])
        sd=qcar.signed_distance(pts)
        worst_air=max(worst_air, float(sd.max()))
        if float(sd.max())>-0.02:
            slot_blocked=True
    # The push face is vertical from just under the glass front to the top of the head.
    # Below that the head slopes away, so a sample there is air and is not the wall.
    wall_zs=(Zin-0.20, Zin+0.20, Zin+0.50)
    wall_sd=[float(qcar.signed_distance([_at(u_push-0.15, z)])[0]) for z in wall_zs]
    print('  slot x %.1f path air %+.2f (want <0)  wall %s (want >0)'%(
        0.5*(x0+x1), worst_air, ' '.join('%+.2f'%s for s in wall_sd)))
    if slot_blocked or min(wall_sd)<0.05:
        hook_fail=True
        print('  HOOK SLOT BLOCKED')
# Shelf tips, relaxed, are shelf_span apart. The release holds both edges out and lifts.
shelf_span=panel_h-2*cl_lip
hooks=((47.0,53.0,26.24,1),(84.6,90.6,26.24,1),(49.6,55.6,62.94,-1),(76.0,82.0,62.94,-1))
top=[h for h in hooks if h[3]>0]
bot=[h for h in hooks if h[3]<0]
def _span(group):
    c=[0.5*(a+b) for a,b,_,_ in group]
    return max(c)-min(c)
top_span=_span(top); bot_span=_span(bot)
print('shelf tip span %.2f mm, top hooks %.1f mm apart, bottom hooks %.1f mm apart'%(shelf_span, top_span, bot_span))
if abs(cl_head-(cl_slot_gap+cl_slot_wall))>1e-6 or cl_slot_gap<1.20 or cl_lip<0.32-1e-6 or top_span<33 or top_span>38:
    hook_fail=True
    print('RELEASE SPAN FAIL')
# Lip on the mesh: solid 0.08 mm inboard of the tip, air 0.08 mm outboard, both edges.
for x0,x1,edge,dire in hooks:
    xm=OX+0.5*(x0+x1)
    z_shelf=Zin+cl_lip_h/2
    d_in=float(qcar.signed_distance([[xm, _cy(edge+dire*(cl_lip-0.08)), z_shelf]])[0])
    d_out=float(qcar.signed_distance([[xm, _cy(edge+dire*(cl_lip+0.08)), z_shelf]])[0])
    print('  lip x %.1f in shelf %+.2f (want >0)  past tip %+.2f (want <0)'%(0.5*(x0+x1), d_in, d_out))
    if d_in<0.02 or d_out>-0.02:
        hook_fail=True
        print('  LIP TIP FAIL')
# Both long edges move out together and the glass lifts straight up. Shoved against one
# beam, the near lip still covers cl_lip + cl_gap. rel_air/2 is the air on that shelf.
# The sideways slide is rel_slide. A non-zero slide has to fit the guide play below.
rel_slide=scad_num('rel_slide')
beam_span=panel_h+2*cl_gap
hold=cl_lip+cl_gap+rel_air/2.0
near_air=hold-(cl_lip+cl_gap)
rel_eps=1.5*cl_t*hold/(cl_L*cl_L)
rel_F=3*cl_E*cl_I*hold/(cl_L**3)
print('release lifts both edges %.3f mm, sideways slide %.3f mm, strain %.2f%%, force %.2f N/hook, near-shelf air %.3f mm'%(
    hold, rel_slide, 100*rel_eps, rel_F, near_air))
if rel_eps>=petg_eps or rel_F>=8.0 or near_air<0.02:
    hook_fail=True
    print('RELEASE CLEARANCE FAIL')
for G in (glass_lo, panel_h, glass_hi):
    spare=beam_span-G
    print('  glass %.2f mm, spare between the beams %.2f mm'%(G, spare))
    if spare<0.05:
        hook_fail=True
        print('  GLASS WIDER THAN THE BEAMS')
# A lip that prints 0.10 mm long. Reported against the flexural gate, not a second gate.
lip_err=0.10
hold_err=(cl_lip+lip_err)+cl_gap+rel_air/2.0
eps_err=1.5*cl_t*hold_err/(cl_L*cl_L)
F_err=3*cl_E*cl_I*hold_err/(cl_L**3)
print('print-error lip +%.2f mm: hold-out %.3f mm  strain %.2f%%  force %.2f N  %s the %.1f%% gate'%(
    lip_err, hold_err, 100*eps_err, F_err, 'PASSES' if eps_err<petg_eps else 'OVER', 100*petg_eps))
print('service strain %.2f%% is %.0f MPa at E %.0f N/mm^2. Ultimaker PETG tensile-Z is 19 ± 6.4 MPa at 1.8%% strain, flexural strength 50 MPa at 3.6%%. The gate is 3.6/1.5 = %.1f%%.'%(
    100*rel_eps, cl_E*rel_eps, cl_E, 100*petg_eps))
print('the beam bends about the board x axis; the outer fiber is in tension along z, across the layers. The %.1f%% gate is above the interlayer elongation at break, 1.8 ± 0.8%%. The %.2f%% hold and the %.2f%% long-lip hold are under that 1.8%% mean and above 1.0%%.'%(
    100*petg_eps, 100*rel_eps, 100*eps_err))
# Guide play is the drawing width minus the tail width. The tail width is fpc_wide.
# Jushuo ±0.10 on guide column B and the GDEY ±0.10 on the tail are both printed.
# A negative play means that pair does not enter. It is not a slide failure.
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
    hook_fail=True
    print('J4 TAIL DOES NOT FIT THE GUIDE')
nonneg=[p for p in (play_nom, play_narrow, play_wide, play_hi, play_lo) if p>=-1e-9]
slide_cap=min(nonneg) if nonneg else -1.0
print('release sideways slide %.3f mm, smallest non-negative guide play %.2f mm'%(rel_slide, slide_cap))
if rel_slide>slide_cap+1e-9:
    hook_fail=True
    print('J4 GUIDE PLAY FAIL')
# J4 hook. The tongue's +x face bears on the upper ear's -x face and the pull is +x.
# Insertion holds the hook pick_lift high and rotates it about the elbow from pick_swing
# to the seated lean, then lowers the tongue into the slot. The shell is off for that,
# so shell distance is printed and is not a gate. A straight +x translation of the seated
# hook is the control: this ear hook does not cross the fold, and the pull runs into J2.
pick_ang=scad_num('pick_ang'); pick_ytip=scad_num('pick_ytip'); pick_L=scad_num('pick_L')
pick_w=scad_num('pick_w'); pick_hand=scad_num('pick_hand'); pick_inset=scad_num('pick_inset')
pick_z0=scad_num('pick_z0'); pick_z1=scad_num('pick_z1')
pick_tongue_dx=scad_num('pick_tongue_dx')
pick_tongue_y0=scad_num('pick_tongue_y0'); pick_tongue_y1=scad_num('pick_tongue_y1')
pick_tongue_z0=scad_num('pick_tongue_z0'); pick_tongue_z1=scad_num('pick_tongue_z1')
pick_head_dx=scad_num('pick_head_dx'); pick_head_hy=scad_num('pick_head_hy')
pick_head_z0=scad_num('pick_head_z0'); pick_head_z1=scad_num('pick_head_z1')
pick_swing=scad_num('pick_swing'); pick_lift=scad_num('pick_lift')
stroke_lo=scad_num('stroke_lo'); stroke_hi=scad_num('stroke_hi')
mouth_x=scad_num('mouth_x'); j4_depth=scad_num('j4_depth'); j4_len=scad_num('j4_len'); j4_h=scad_num('j4_h')
j4_front=scad_num('j4_front'); ear_x=scad_num('ear_x')
fpc_R=(P['panel_gap']-0.50)/2.0
ri=fpc_R-0.15; ro=fpc_R+0.15
j4_y0=44.0-j4_len/2.0; j4_y1=44.0+j4_len/2.0
def _rod(p0, p1, width, z0, z1):
    p0=np.asarray(p0, float); p1=np.asarray(p1, float)
    delta=p1-p0; length=float(np.linalg.norm(delta))
    th=math.atan2(delta[1], delta[0]); c,s=math.cos(th), math.sin(th)
    m=trimesh.creation.box(extents=[length, width, z1-z0])
    m.apply_translation([length/2.0, 0, (z0+z1)/2.0])
    R=np.eye(4); R[0,0]=c; R[1,0]=-s; R[0,1]=s; R[1,1]=c
    m.apply_transform(R)
    X,Y=T(p0[0], p0[1])
    m.apply_translation([X, Y, 0])
    return m
def _xform(pts, elb, psi, zlift, xshift):
    c,s=math.cos(psi), math.sin(psi)
    EX,EY=T(float(elb[0]), float(elb[1]))
    out=np.array(pts, float, copy=True)
    dX=out[:,0]-EX; dY=out[:,1]-EY
    out[:,0]=EX+c*dX+s*dY+xshift
    out[:,1]=EY-s*dX+c*dY
    out[:,2]=out[:,2]+zlift
    return out
def _elbow(ang):
    a=math.radians(ang)
    return np.array([ear_x+pick_L*math.cos(a), pick_ytip+pick_L*math.sin(a)])
def _seated(ang):
    a=math.radians(ang)
    tip=np.array([ear_x, pick_ytip])
    elb=_elbow(ang)
    unit=np.array([math.cos(a), math.sin(a)])
    s0=tip+pick_inset*unit
    shank=_rod(s0, elb, pick_w, pick_z0, pick_z1)
    hand=_rod(elb, elb+np.array([pick_hand, 0.0]), pick_w, pick_z0, pick_z1)
    tongue=box('tongue', ear_x-pick_tongue_dx, pick_tongue_y0, ear_x, pick_tongue_y1, pick_tongue_z0, pick_tongue_z1)[1]
    head=box('head', ear_x-pick_head_dx, pick_ytip-pick_head_hy, ear_x, pick_ytip+pick_head_hy, pick_head_z0, pick_head_z1)[1]
    lip=trimesh.util.concatenate([tongue, head])
    return shank, hand, lip, elb
# Half-annulus of the fold, bulging to +x. The y band is the tail width. Positive clearance is air.
def _fold_clear(pts, wide):
    bx=pts[:,0]-OX; by=H-OYT-pts[:,1]; bz=pts[:,2]
    y0,y1=44.0-wide/2.0, 44.0+wide/2.0
    zc=Zt+0.50+fpc_R
    dx=bx-FPC_EDGE; dz=bz-zc
    r=np.hypot(dx, dz)
    d_pos=np.where(r<ri, ri-r, np.where(r>ro, r-ro, -np.minimum(r-ri, ro-r)))
    az=np.abs(dz)
    d_neg=np.where(az<ri, np.hypot(dx, az-ri), np.where(az>ro, np.hypot(dx, az-ro), -dx))
    d2=np.where(dx>=0, d_pos, d_neg)
    dy=np.maximum(np.maximum(y0-by, by-y1), 0.0)
    return float(np.where(dy<=0, d2, np.where(d2<0, dy, np.hypot(d2, dy))).min())
glass=box('glass', 29.46, 26.24, 108.46, 62.94, Zt+P['panel_gap'], Zt+P['panel_gap']+1.2)[1]
# The housing +x face is j4_front. The ear's -x face is 0.10 mm in front of it.
housing=box('j4 body', j4_front-j4_depth, j4_y0, j4_front, j4_y1, Zt, Zt+j4_h)[1]
j2m=box('j2', 121.05-4.47, 56.7, 121.05+4.47, 64.0, Zt, Zt+3.26)[1]
qglass=trimesh.proximity.ProximityQuery(glass)
qshell=trimesh.proximity.ProximityQuery(shell)
qhouse=trimesh.proximity.ProximityQuery(housing)
qj2=trimesh.proximity.ProximityQuery(j2m)
_CLEAR_KEYS=('carrier','fold','glass','j2','housing','shell')
def _pose_pts(ang, psi, zlift, xshift, n=700, seed=1):
    shank, hand, lip, elb=_seated(ang)
    pts=[]
    lip_pts=None
    for i,m in enumerate((shank, hand, lip)):
        raw=np.vstack([m.vertices, m.sample(n, seed=seed+i)])
        moved=_xform(raw, elb, psi, zlift, xshift)
        pts.append(moved)
        if i==2:
            lip_pts=_xform(m.vertices, elb, psi, zlift, xshift)
    return np.vstack(pts), lip_pts
def _clears(pts, wide):
    return {
        'carrier': -float(qcar.signed_distance(pts).max()),
        'fold': _fold_clear(pts, wide),
        'glass': -float(qglass.signed_distance(pts).max()),
        'j2': -float(qj2.signed_distance(pts).max()),
        'housing': -float(qhouse.signed_distance(pts).max()),
        'shell': -float(qshell.signed_distance(pts).max()),
    }
def _accumulate(acc, c):
    for k in _CLEAR_KEYS:
        acc[k]=min(acc[k], c[k])
def _path(ang, wide, shifts):
    """Raised rotation, drop onto the face, then the +x pull. Returns clearances and lip poses."""
    acc={k: 99.0 for k in _CLEAR_KEYS}
    ymax=-1e9
    lips=[]
    for psi_deg in np.linspace(pick_swing, 0.0, 15):
        pts, lip=_pose_pts(ang, math.radians(float(psi_deg)), pick_lift, 0.0, seed=1)
        _accumulate(acc, _clears(pts, wide))
        ymax=max(ymax, float((H-OYT-pts[:,1]).max()))
    for i,zl in enumerate(np.linspace(pick_lift, 0.0, 7)):
        pts, lip=_pose_pts(ang, 0.0, float(zl), 0.0, seed=20+i)
        _accumulate(acc, _clears(pts, wide))
        ymax=max(ymax, float((H-OYT-pts[:,1]).max()))
    for i,shift in enumerate(shifts):
        pts, lip=_pose_pts(ang, 0.0, 0.0, shift, seed=40+i)
        _accumulate(acc, _clears(pts, wide))
        ymax=max(ymax, float((H-OYT-pts[:,1]).max()))
        lips.append((shift, lip))
    return acc, ymax, lips
def _bears_on_ear(lip, shift):
    """The tool's +x face pulls on the ear's -x face. The lip stays on the -x side of that face."""
    bx=lip[:,0]-OX; by=H-OYT-lip[:,1]; bz=lip[:,2]
    face=ear_x+shift
    on=abs(float(bx.max())-face)<=0.05 and float(bx.max())<=face+0.02
    behind=float(bx.min())<face-0.02
    y_ok=float(by.min())<pick_tongue_y1 and float(by.max())>pick_tongue_y0
    z_ok=float(bz.min())<Zt+j4_h and float(bz.max())>Zt
    return on and behind and y_ok and z_ok
service_shifts=(0.0, stroke_lo, 0.5*(stroke_lo+stroke_hi), stroke_hi)
elb=_elbow(pick_ang)
print('J4 hook lean %.0f deg, tongue on the ear at (%.3f, %.3f), elbow (%.2f, %.2f), handle %.2f mm to x %.2f'%(
    pick_ang, ear_x, pick_ytip, elb[0], elb[1], pick_hand, elb[0]+pick_hand))
print('J4 insertion rotates %.0f deg about that elbow at +%.2f mm, then lowers into the slot and pulls +x from %.2f mm to %.2f mm'%(
    pick_swing, pick_lift, stroke_lo, stroke_hi))
sweep, tool_ymax, lips=_path(pick_ang, fpc_wide, service_shifts)
print('J4 service sweep  carrier %6.2f  fold %6.2f  glass %6.2f  J2 %6.2f  housing %6.2f  (min 0.20, housing min 0)'%(
    sweep['carrier'], sweep['fold'], sweep['glass'], sweep['j2'], sweep['housing']))
print('J4 shell along that sweep %6.2f (shell is off in service; not a gate)'%sweep['shell'])
neighbours_ok=min(sweep['carrier'], sweep['fold'], sweep['glass'], sweep['j2'])>=0.20 and sweep['housing']>=0.0
contact_ok=all(_bears_on_ear(lip, shift) for shift, lip in lips)
if not neighbours_ok or not contact_ok:
    hook_fail=True
    print('J4 PICK FAIL')
if not contact_ok:
    print('J4 TONGUE DOES NOT BEAR ON THE EAR')
# 55° and 90° are clearance failures. An angle-band check does not stand in for that.
for ang in (55.0, 90.0):
    alt, _, _=_path(ang, fpc_wide, (0.0, stroke_hi))
    alt_min=min(alt['carrier'], alt['fold'], alt['glass'], alt['j2'])
    print('J4 hook at %.0f deg  carrier %6.2f  fold %6.2f  glass %6.2f  J2 %6.2f  housing %6.2f'%(
        ang, alt['carrier'], alt['fold'], alt['glass'], alt['j2'], alt['housing']))
    if alt_min>=0.20 and alt['housing']>=0.0:
        hook_fail=True
        print('J4 ANGLE %.0f DID NOT HIT ANYTHING'%ang)
# Straight +x of the seated hook, continued past the assumed stroke. The stroke itself is
# inside the service sweep above. This ear hook stays clear of the fold. The longer pull
# has to run into J2.
straight_j2=99.0
for shift in (0.0, 2.0, 3.0, 4.0, 5.0, 6.0):
    pts, _=_pose_pts(pick_ang, 0.0, 0.0, shift, n=500, seed=70)
    straight_j2=min(straight_j2, _clears(pts, fpc_wide)['j2'])
print('J4 straight +x of the seated hook, J2 clearance %6.2f (want <0)'%straight_j2)
if straight_j2>=0.0:
    hook_fail=True
    print('J4 STRAIGHT PATH DID NOT HIT J2')
# Free +x run: the largest shift that keeps 0.20 mm off the carrier, the fold, the glass, and J2.
lo_s, hi_s=0.0, 8.0
for _ in range(14):
    mid=0.5*(lo_s+hi_s)
    pts, _=_pose_pts(pick_ang, 0.0, 0.0, mid, n=500, seed=80)
    c=_clears(pts, fpc_wide)
    if min(c['carrier'], c['fold'], c['glass'], c['j2'])>=0.20 and c['housing']>=0.0:
        lo_s=mid
    else:
        hi_s=mid
pts, _=_pose_pts(pick_ang, 0.0, 0.0, lo_s+0.40, n=700, seed=90)
probe=_clears(pts, fpc_wide)
probe_min=min(probe['carrier'], probe['fold'], probe['glass'], probe['j2'])
print('J4 free pull %.2f mm before clearance drops under 0.20; assumed stroke %.2f to %.2f mm; probe at +0.40 mm is %6.2f'%(
    lo_s, stroke_lo, stroke_hi, probe_min))
if stroke_hi>lo_s+1e-6 or probe_min>=0.20:
    hook_fail=True
    print('J4 TRAVEL FAIL')
# Opening beside the fold. The tool's highest board-y in the sweep has to fit in it,
# with 0.20 mm left at the post. A 6.35 mm opening is under that need.
cv=carrier.vertices
cbx=cv[:,0]-OX; cby=H-OYT-cv[:,1]; cbz=cv[:,2]
fold_y1=44.0+fpc_wide/2.0
post=(cbx>108.2)&(cbx<109.6)&(cby>fold_y1-0.01)&(cbz>Zt)&(cbz<Zt+P['panel_gap']+0.2)
if not np.any(post):
    hook_fail=True
    print('J4 CARRIER OPENING MISSING')
    gap=0.0
else:
    gap=float(cby[post].min())-fold_y1
need=(tool_ymax-fold_y1)+0.20
print('J4 carrier opening %.2f mm, hook needs %.2f mm (a 6.35 mm opening fails)'%(gap, need))
if gap+1e-6<need or need<=6.35:
    hook_fail=True
    print('J4 CARRIER OPENING FAIL')
print('locked lift pulls %.3f mm of tail per mm; the %.2f mm insertion has %.2f mm of lift before it is under %.2f mm'%(
    math.pi/2, fpc_inserted(P['panel_gap']+PRINT_Z, 0.50),
    (fpc_inserted(P['panel_gap']+PRINT_Z, 0.50)-FPC_MIN)/(math.pi/2), FPC_MIN))
if hook_fail:
    print('HOOK FAIL')
# +x stop. Pull-out along x is 1 mm of tail per mm of slide (the straight run grows).
# A post that bites the outline by xstop_bite still kisses after a 0.10 mm short print
# if the bite is at least that. The tall-print insertion is the 3.11 mm row.
vcar=carrier.vertices
right=(vcar[:,0]>OX+108.2)&(vcar[:,0]<OX+109.6)&(vcar[:,2]>Zpb+0.3)&(vcar[:,2]<Zin)
if not np.any(right):
    print('X STOP MISSING')
    hook_fail=True
    bite=0.0
else:
    bite=(OX+FPC_EDGE)-float(vcar[right,0].min())
worst_slide=max(0.0, PRINT_Z-bite)
ins_slide=fpc_inserted(P['panel_gap']+PRINT_Z, 0.50)-worst_slide
print('x stop bite %.3f mm, worst +x slide after a %.2f mm short print %.3f mm, inserted then %.2f mm'%(bite, PRINT_Z, worst_slide, ins_slide))
if bite<xstop_bite-0.03 or ins_slide<FPC_MIN-1e-6:
    print('X STOP FAIL')
    hook_fail=True
# -x is the curtain at board x 29.0. That slide lengthens the tail.
print('minus-x runs into the curtain %.2f mm away; that slide lengthens the inserted tail'% (29.46-29.0))
stack_face=0.16+0.10+0.10+0.15
print('face recess %.2f mm, stack board+print+snap+standoff %.2f mm, margin %.2f mm'%(face_recess, stack_face, face_recess-stack_face))
if face_recess<stack_face+0.10:
    print('FACE RECESS FAIL')
    hook_fail=True
if hook_fail:
    print('HOOK FAIL')
# Buttons. TS-1187A-B-A-B: H=1.5±0.05, drawing TRAVEL 0.2±0.1, spec table 0.25.
# The sheet does not give an over-travel rating. The lip is the plastic stop.
# The shell is fixed to the backplate. Clamp posts start 0.1 mm above a nominal
# board, so they do not make the shell follow the board, and the board-thickness
# term stays in the stack. Snap windows add ±0.10 mm when the shell is not seated
# in the middle of the window. Moving the whole shell changes the gap and not the
# travel, because the cone and the lip move together.
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
# Past the switch's own travel the lip is still  this far away. The sheet states
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
# Lip still exists on the partition side after the FPC pocket. Neck is open.
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
if interfere or pcb_shell<-0.05 or pcb_back<-0.05 or carrier_fail or btn_fail or hook_fail:
    print('INTERFERE')
    sys.exit(1)
print('FIT OK')
