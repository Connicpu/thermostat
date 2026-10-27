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
_root0=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_scad=open(os.path.join(_root0,'case','thermostat_case.scad')).read()
def scad_num(name):
    m=re.search(r'(?m)^%s\s*=\s*([0-9.]+)\s*;'%re.escape(name), _scad)
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
  pts=np.asarray(m.vertices) if isinstance(m,trimesh.points.PointCloud) else np.vstack([m.vertices,m.sample(4000)])
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
    d=trimesh.proximity.ProximityQuery(shell).signed_distance(np.vstack([pcbm.vertices,pcbm.sample(20000)]))
    pcb_shell=-float(d.max())
    print('PCB outline vs shell',round(pcb_shell,2))
    d=trimesh.proximity.ProximityQuery(back).signed_distance(np.vstack([pcbm.vertices,pcbm.sample(20000)]))
    pcb_back=-float(d.max())
    print('PCB vs backplate (standoff seat expected 0)',round(pcb_back,2))
else:
    print('PCB outline checks skipped (no board.stl)')

def mesh_clearance(part, obst, dz=0.0, n=6000):
    """Positive means air between the solids. dz lifts a second copy of the samples."""
    pts=np.vstack([part.vertices, part.sample(n)])
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
            if qi.signed_distance(bodies[j].sample(400)).max() < -0.05:
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
if cl_L<5.0 or cl_eps>=0.020 or cl_F>=8.0 or abs(math.degrees(math.atan2(cl_lip, cl_lip))-45)>1:
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
    # Window: air a 1.0 mm blade can occupy, and a solid wall on the glass side of that air.
    # u increases toward the glass. The push face is the inboard face of the outboard wall.
    u_outer=-(cl_gap+cl_t)
    u_push=u_outer-cl_slot_gap
    d_slot=float(qcar.signed_distance([_at(u_push+0.50, Zin)])[0])
    d_wall=float(qcar.signed_distance([_at(u_push-0.20, Zin)])[0])
    d_root=float(qcar.signed_distance([_at(u_outer+0.15, Zin-1.6)])[0])
    print('  slot x %.1f blade %+.2f (want <0)  push wall %+.2f (want >0)  root %+.2f (want >0)'%(0.5*(x0+x1), d_slot, d_wall, d_root))
    if d_slot>-0.05 or d_wall<0.05 or d_root<0.05:
        hook_fail=True
        print('  HOOK SLOT MISSING')
# Shelf tips, relaxed, are shelf_span apart. Holding one edge out by (glass - span)
# opens a gap the glass can slide through. rel_air is split across the two shelves.
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
def _release(G, freed):
    """Overlaps after holding `freed` edge out. Negative means the shelf is clear of the glass."""
    delta=G-shelf_span+rel_air
    eps=1.5*cl_t*delta/(cl_L*cl_L)
    force=3*cl_E*cl_I*delta/(cl_L**3)
    # Tips at board y. Top tip moves down in y when the top edge is freed; bottom tip moves up.
    y_top=26.24+cl_lip-(delta if freed=='top' else 0.0)
    y_bot=62.94-cl_lip+(delta if freed=='bottom' else 0.0)
    y0=(y_top+y_bot-G)/2.0
    top_ov=y_top-y0
    bot_ov=y0+G-y_bot
    return delta, eps, force, top_ov, bot_ov
print('release hold-out is glass width minus shelf span, plus %.2f mm so both shelves clear'%rel_air)
for G in (glass_lo, panel_h, glass_hi):
    for freed in ('top', 'bottom'):
        delta, eps, force, top_ov, bot_ov=_release(G, freed)
        true_d=G-shelf_span
        true_eps=1.5*cl_t*true_d/(cl_L*cl_L)
        true_F=3*cl_E*cl_I*true_d/(cl_L**3)
        print('  glass %.2f  %s  true hold-out %.3f mm  strain %.2f%%  force %.2f N   service %.3f mm  strain %.2f%%  force %.2f N  overlap top %+.3f bottom %+.3f'%(
            G, freed, true_d, 100*true_eps, true_F, delta, 100*eps, force, top_ov, bot_ov))
        if eps>=0.020 or force>=8.0 or top_ov>-0.02 or bot_ov>-0.02:
            hook_fail=True
            print('  RELEASE CLEARANCE FAIL')
# J4, shell off, glass seated. The pick lies in the gap over the 2.0 mm housing and
# reaches the mouth from outside the glass, through the open side of the fold.
# A locked lift pulls pi/2 mm of tail per mm; this step does not lift or slide.
fpc_R=(P['panel_gap']-0.50)/2.0
fpc_inner=fpc_R-0.15
pick_half=0.40
zc_tun=Zt+2.0+(P['panel_gap']-2.0)/2.0
pick_x1=FPC_EDGE+fpc_inner-0.30
pick=box('J4 pick', 103.55, 41.80, pick_x1, 46.20, Zt+2.05, zc_tun+pick_half)[1]
c_pick=mesh_clearance(carrier, pick, n=4000)
print('J4 pick vs carrier %6.2f (mouth to x %.2f, fold inner radius %.2f, blade half %.2f)'%(c_pick, pick_x1, fpc_inner, pick_half))
print('locked lift pulls %.3f mm of tail per mm; the %.2f mm insertion has %.2f mm of lift before it is under %.2f mm'%(
    math.pi/2, fpc_inserted(P['panel_gap']+PRINT_Z, 0.50),
    (fpc_inserted(P['panel_gap']+PRINT_Z, 0.50)-FPC_MIN)/(math.pi/2), FPC_MIN))
if c_pick<0.20 or fpc_inner-pick_half<0.80:
    hook_fail=True
    print('J4 PICK FAIL')
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
if leaf_free<4.0 or leaf_eps>=0.020 or not (0.20<=leaf_defl<=0.40):
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
    v=np.vstack([mesh.vertices, mesh.sample(5000)])
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
    v=np.vstack([mesh.vertices, mesh.sample(3000)])
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
