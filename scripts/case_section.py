import trimesh,numpy as np,matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from matplotlib.patches import Polygon
src=open('fit.py').read().split('obst=')[0]; exec(src)
back=trimesh.load('back.stl'); shell=trimesh.load('shell.stl')
named=dict(parts)
def slice_(m,origin,normal):
  s=m.section(plane_origin=origin,plane_normal=normal)
  if s is None: return []
  return [np.asarray(e) for e in s.discrete]
cuts=[('A: section at board y=58 (J1 pin row, C43 centre), looking toward board top',[0,T(0,58)[1],0],[0,1,0],0,(40,100)),
      ('B: section at board x=70 (J1 header + 15EDGK plug + wire bend, wire hole, plug pocket)',[T(70,0)[0],0,0],[1,0,0],1,(0,50)),
      ('C: section at board x=56.6 (C43 edge vs backplate lip notch)',[T(56.6,0)[0],0,0],[1,0,0],1,(0,30))]
fig,axs=plt.subplots(3,1,figsize=(14,15))
cols={'backplate':'#bbbbbb','shell':'#f2f2f2'}
for ax,(title,o,n,ax_i,xl) in zip(axs,cuts):
  for nm,m,c in [('backplate',back,'#b0b0b0'),('shell',shell,'#e8e8e8')]:
    for p in slice_(m,o,n): ax.fill(p[:,ax_i],p[:,2],color=c,ec='k',lw=0.6)
  for p in slice_(brd,o,n): ax.plot(p[:,ax_i],p[:,2],color='darkgreen',lw=0.6)
  for nm,m in parts:
    if isinstance(m,trimesh.points.PointCloud): continue
    c='royalblue' if 'C43' in nm else 'limegreen' if 'plug' in nm else 'green' if 'header' in nm else 'darkred' if 'wire' in nm else 'orange'
    for p in slice_(m,o,n): ax.fill(p[:,ax_i],p[:,2],color=c,alpha=0.6,ec=c)
  _,pn=box('panel',29.46,26.24,108.46,62.94,Zpb,Zpb+1.2)
  for p in slice_(pn,o,n): ax.fill(p[:,ax_i],p[:,2],color='white',ec='k',hatch='//')
  ax.axhline(0,color='saddlebrown',lw=2); ax.text(xl[0]+0.5,-1.2,'wall surface',color='saddlebrown')
  ax.set_title(title); ax.set_aspect('equal'); ax.set_xlim(*xl); ax.set_ylim(-3,26); ax.grid(alpha=0.3)
  ax.set_xlabel('case X (mm)' if ax_i==0 else 'case Y (mm)'); ax.set_ylabel('Z (mm, out of wall)')
# annotations
a=axs[0]; X43=T(53.3,0)[0]
a.annotate('',xy=(X43,3.0),xytext=(X43,Zb-9.5),arrowprops=dict(arrowstyle='<->',color='b')); a.text(X43+0.5,3.6,'C43 bottom -> plate 2.5',color='b')
a.annotate('',xy=(T(70,0)[0]+6,3.0),xytext=(T(70,0)[0]+6,Zb-7.6),arrowprops=dict(arrowstyle='<->',color='g')); a.text(T(70,0)[0]+6.5,5,'J1 header -> plate 4.4',color='g')
b=axs[1]; Yp=T(0,48)[1]
b.annotate('',xy=(Yp,1.8),xytext=(Yp,Zb-11.1),arrowprops=dict(arrowstyle='<->',color='darkgreen')); b.text(Yp+0.4,2.3,'plug -> pocket floor 2.1\n(1.3 min at pocket edge)',color='darkgreen',fontsize=9)
c=axs[2]; c.text(2.5,7.5,'lip notched to Z=4\nC43 -> lip 1.58 min',color='b',fontsize=9)
plt.tight_layout(); plt.savefig('/workspace/thermostat/v3/case/case-section-j1-c43.png',dpi=110)
