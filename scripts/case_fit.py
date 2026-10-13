import trimesh,numpy as np,sys,json
P=dict(standoff_h=12.0,panel_gap=5.0)
for a in sys.argv[1:]: k,v=a.split('='); P[k]=float(v)
plate_t=3; pcb_t=1.6; Zb=plate_t+P['standoff_h']; Zt=Zb+pcb_t; Zpb=Zt+P['panel_gap']
H=71; OX=3.5; OYT=3.5
def T(bx,by): return OX+bx, H-OYT-by
def box(name,bx0,by0,bx1,by1,z0,z1):
  X0,Y1=T(bx0,by0); X1,Y0=T(bx1,by1)
  m=trimesh.creation.box(extents=[X1-X0,Y1-Y0,z1-z0]); m.apply_translation([(X0+X1)/2,(Y0+Y1)/2,(z0+z1)/2]); return name,m
def cyl(name,bx,by,r,z0,z1):
  X,Y=T(bx,by); m=trimesh.creation.cylinder(radius=r,height=z1-z0,sections=64); m.apply_translation([X,Y,(z0+z1)/2]); return name,m
back=trimesh.load('back.stl'); shell=trimesh.load('shell.stl')
brd=trimesh.load('board.stl'); brd.apply_translation([-100+OX,H-OYT+100,Zb])
v=brd.triangles.reshape(-1,3)
def kicad_part(name,bx0,by0,bx1,by1,front=True):
  X0,Y1=T(bx0,by0); X1,Y0=T(bx1,by1)
  s=(v[:,0]>=X0)&(v[:,0]<=X1)&(v[:,1]>=Y0)&(v[:,1]<=Y1)&((v[:,2]>Zt+0.02) if front else (v[:,2]<Zb-0.02))
  return name,trimesh.points.PointCloud(v[s])
parts=[]
# real KiCad models (sampled vertices)
for n,b in dict(U6=(28.8,6.3,40.2,14.1),U7=(28.8,21.3,40.2,29.1),U8=(28.8,36.3,40.2,44.1),Q2=(14.5,6.6,25.9,13.4),Q3=(14.5,21.6,25.9,28.4),Q4=(14.5,36.6,25.9,43.4),J3=(119.6,5,126.4,10.8),J4=(98.5,34.8,103.9,53),SW3=(108.5,27.3,115.5,32.7),SW4=(108.5,37.3,115.5,42.7),SW5=(108.5,47.3,115.5,52.7),SW1=(89.5,2.3,96.5,7.7),SW2=(98.5,2.3,105.5,7.7),D2=(12.6,57.3,19.4,61.3),D3=(115.8,45,122.2,48),D4=(33.8,57,40.2,60)).items():
  parts.append(kicad_part(n,*b))
parts.append(kicad_part('J1 header (model)',59.4,49.9,79.3,62.7,front=False))
# missing models -> datasheet envelopes
parts+= [box('L1 SWPA6045 6.0x6.0x4.5',42.05,54.85,48.05,60.85,Zt,Zt+4.5),
 box('J2 USB-C 8.94x7.3x3.26 (flush)',121.05-4.47,56.7,121.05+4.47,64.0,Zt,Zt+3.26),
 box('U3 ESP32-C6 module 18x25.5x3.15',66.05,0,84.05,25.5,Zt,Zt+3.15),
 box('C1/C41/C42 1210 x2.5',23,51.4,26.3,61.1,Zt,Zt+2.5), box('C2 1210 x2.5',33.8,48.4,36.3,51.7,Zt,Zt+2.5),
 cyl('C43 8x9 (9.5 incl. seat)',53.3,58.05,4.0+0.25,Zb-9.5,Zb),
 box('J1 header body (datasheet 18.3x9.2x7.6)',70.05-9.15,50.05,70.05+9.15,59.25,Zb-7.6,Zb),
 box('J1 plug 15EDGK (18.3 x 15.6 x 11.1)',70.05-9.15,40.45,70.05+9.15,56.05,Zb-11.1,Zb),
 box('J1/C43 THT tails front (2.0)',49.5,56.5,79.0,59.5,Zt,Zt+2.0),
]
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
  for on,o in obst.items():
    if n.startswith('wire') and on!='backplate': continue
    d=trimesh.proximity.ProximityQuery(o).signed_distance(pts)   # >0 inside
    worst=d.max()
    res.append((n,on,-worst))
for n,on,c in sorted(res,key=lambda r:r[2]):
  if c<6: print('%-42s vs %-14s %6.2f %s'%(n,on,c,'INTERFERE' if c<0 else ('TIGHT' if c<1 else '')))
# board outline vs shell
d=trimesh.proximity.ProximityQuery(shell).signed_distance(np.vstack([pcbm.vertices,pcbm.sample(20000)]))
print('PCB outline vs shell',round(-d.max(),2))
d=trimesh.proximity.ProximityQuery(back).signed_distance(np.vstack([pcbm.vertices,pcbm.sample(20000)]))
print('PCB vs backplate (standoff seat expected 0)',round(-d.max(),2))
