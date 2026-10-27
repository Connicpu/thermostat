# Clearance of the populated board against the case, plus the FPC fold and the panel carrier.
# board.stl is not committed. From the repo root, with KiCad 9, and without
# --drill-origin (this script places the export itself):
#   kicad-cli pcb export stl -o board.stl --force --cut-vias-in-body \
#       kicad/thermostat-v3.kicad_pcb
# Assembly-frame solids, not the flipped print STLs:
#   OPENSCADPATH=case openscad -o back.stl -D 'part="none"' -D 'which="b"' scripts/case_fit_export.scad
#   OPENSCADPATH=case openscad -o shell.stl -D 'part="none"' -D 'which="s"' scripts/case_fit_export.scad
#   OPENSCADPATH=case openscad -o carrier.stl -D 'part="none"' -D 'which="c"' scripts/case_fit_export.scad
# Run this script in the directory that contains back.stl, shell.stl, and carrier.stl.
# The fold and the carrier checks run without board.stl. The mesh sampling does not.
import trimesh,numpy as np,sys,os,re,math
P=dict(standoff_h=14.0,panel_gap=5.0)   # case standoff and panel gap
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
print('FPC tail %.2f mm, mouth x %.2f, straight %.2f mm, pull-out %.3f mm/mm'%(FPC_FREE, FPC_MOUTH, FPC_STRAIGHT, math.pi/2))
fpc_ok=True
for z_leg in (0.30, 0.50, 0.70):
    for gap in (4.6, 5.0, 5.2):
        ins=fpc_inserted(gap, z_leg)
        bad=ins<=2.0
        fpc_ok = fpc_ok and not bad
        print('  leg %.2f  gap %.1f  R %.3f  inserted %5.2f%s'%(z_leg, gap, (gap-z_leg)/2.0, ins, '  SHORT' if bad else ''))
nominal=fpc_inserted(P['panel_gap'], 0.50)
print('nominal gap %.2f leg 0.50 inserted %.2f (model band 2.2..3.8)'%(P['panel_gap'], nominal))
if not (2.2 < nominal < 3.8):
    fpc_ok=False
    print('nominal insertion outside 2.2..3.8')
if not fpc_ok:
    print('FPC BUDGET FAIL')
    sys.exit(1)
def T(bx,by): return OX+bx, H-OYT-by
def box(name,bx0,by0,bx1,by1,z0,z1):
  X0,Y1=T(bx0,by0); X1,Y0=T(bx1,by1)
  m=trimesh.creation.box(extents=[X1-X0,Y1-Y0,z1-z0]); m.apply_translation([(X0+X1)/2,(Y0+Y1)/2,(z0+z1)/2]); return name,m
def cyl(name,bx,by,r,z0,z1):
  X,Y=T(bx,by); m=trimesh.creation.cylinder(radius=r,height=z1-z0,sections=64); m.apply_translation([X,Y,(z0+z1)/2]); return name,m
for req in ('back.stl','shell.stl','carrier.stl'):
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
if interfere or pcb_shell<-0.05 or pcb_back<-0.05 or carrier_fail:
    print('INTERFERE')
    sys.exit(1)
print('FIT OK')
