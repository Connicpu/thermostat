import pcbnew
b=pcbnew.LoadBoard('thermostat-v3.kicad_pcb'); mm=pcbnew.FromMM
SILK={pcbnew.F_SilkS,pcbnew.B_SilkS}
import re
for l in open('drc.rpt').read().split('[net_conflict]')[1:]:
    m=re.search(r'schematic \((unconnected-\(.*?\))\)\..*?Pad (\S+) \[<no net>\] of (\S+)',l,re.S)
    if m:
        net,pad,ref=m.groups(); ni=pcbnew.NETINFO_ITEM(b,net); b.Add(ni); b.FindFootprintByReference(ref).FindPadByNumber(pad).SetNet(ni)
# 1 dangling vias
for v in [t for t in b.GetTracks() if t.GetClass()=='PCB_VIA']:
    L=set(t.GetLayer() for t in b.GetTracks() if t.GetClass()=='PCB_TRACK' and t.GetNetCode()==v.GetNetCode() and (t.GetStart()==v.GetPosition() or t.GetEnd()==v.GetPosition()))
    if len(L)<=1 and v.GetNetname()!='GND': b.Remove(v); print('rm via',v.GetNetname())
def cp(bb): return pcbnew.BOX2I(pcbnew.VECTOR2I(bb.GetX(),bb.GetY()),pcbnew.VECTOR2I(bb.GetWidth(),bb.GetHeight()))
edge=b.GetBoardEdgesBoundingBox(); inner=cp(edge); inner.Inflate(-mm(0.5))
def box(it,inf=0):
    bb=cp(it.GetBoundingBox()); bb.Inflate(mm(inf)); return bb
pads=[(p,box(p,0.15)) for f in b.GetFootprints() for p in f.Pads()]
ec=[d for d in b.GetDrawings() if d.GetLayer()==pcbnew.Edge_Cuts]; big=max(ec,key=lambda d:box(d).GetArea())
holes=[box(d,0.3) for d in ec if d is not big]
def padhit(bb,layer):
    cu=pcbnew.F_Cu if layer==pcbnew.F_SilkS else pcbnew.B_Cu
    return any(pb.Intersects(bb) and p.IsOnLayer(cu) for p,pb in pads)
# 2 footprint silk graphics: drop pieces touching pads or outside board
nd=0
for f in b.GetFootprints():
    for g in list(f.GraphicalItems()):
        if g.GetLayer() in SILK and g.GetClass()!='PCB_TEXT':
            bb=box(g,0.08)
            if (g.GetShape()==pcbnew.SHAPE_T_CIRCLE and padhit(bb,g.GetLayer())) or not inner.Contains(bb) or any(h.Intersects(bb) for h in holes): f.Remove(g); nd+=1
print('removed fp silk',nd)
obs={l:[] for l in SILK}
for f in b.GetFootprints():
    for g in f.GraphicalItems():
        if g.GetLayer() in SILK and g.GetClass()!='PCB_TEXT':
            bb=box(g,0.1)
            if g.GetShape() in (pcbnew.SHAPE_T_SEGMENT,pcbnew.SHAPE_T_ARC) and bb.GetWidth()>mm(1.5) and bb.GetHeight()>mm(1.5):
                # diagonal: approximate by a few sample boxes
                s,e=g.GetStart(),g.GetEnd()
                for k in range(9):
                    x=s.x+(e.x-s.x)*k//8; y=s.y+(e.y-s.y)*k//8; obs[g.GetLayer()].append(pcbnew.BOX2I(pcbnew.VECTOR2I(x-mm(.2),y-mm(.2)),pcbnew.VECTOR2I(mm(.4),mm(.4))))
            else: obs[g.GetLayer()].append(bb)
def free(bb,layer,me=None):
    if not inner.Contains(bb) or padhit(bb,layer) or any(h.Intersects(bb) for h in holes): return False
    return not any(o.Intersects(bb) for o in obs[layer])
# board texts: nudge if needed
for d in b.GetDrawings():
    if d.GetClass()=='PCB_TEXT' and d.GetLayer() in SILK:
        p0=d.GetPosition(); ok=False
        for dy in [0,0.3,-0.3,0.6,-0.6,1.0,-1.0,1.5,-1.5,2.5,-2.5]:
            for dx in [0,0.5,-0.5,1,-1,2,-2,3,-3]:
                d.SetPosition(pcbnew.VECTOR2I(p0.x+mm(dx),p0.y+mm(dy)))
                if free(box(d,0.1),d.GetLayer()): ok=True;break
            if ok:break
        if not ok:
            d.SetTextAngleDegrees(0)
            for dy in [0,-1,1,-2,2,-3,3]:
                for dx in [-3,-4,-5,-6,3,4,5]:
                    d.SetPosition(pcbnew.VECTOR2I(p0.x+mm(dx),p0.y+mm(dy)))
                    if free(box(d,0.1),d.GetLayer()): ok=True;break
                if ok:break
        if not ok: d.SetPosition(p0); print('text stuck',d.GetText()[:20])
        obs[d.GetLayer()].append(box(d,0.1))
for g in [x for x in b.GetDrawings() if x.GetClass()!='PCB_TEXT' and x.GetLayer() in SILK]: obs[g.GetLayer()].append(box(g,0.1))
# logo
for f in b.GetFootprints():
    if f.GetReference().startswith('LOGO'): obs[pcbnew.B_SilkS].append(box(f,0.2))
# 3 refs
hidden=[]
fps=sorted(b.GetFootprints(),key=lambda f:box(f).GetArea())
for f in fps:
    f.Value().SetVisible(False)
    r=f.Reference()
    if f.GetReference().startswith('LOGO') or not r.IsVisible(): continue
    r.SetTextSize(pcbnew.VECTOR2I(mm(0.8),mm(0.8))); r.SetTextThickness(mm(0.15)); r.SetKeepUpright(True)
    lay=r.GetLayer(); cb=f.GetBoundingBox(False); c=cb.GetCenter()
    hw,hh=cb.GetWidth()/2/1e6,cb.GetHeight()/2/1e6
    cands=[]
    for gap in (0.55,0.9,1.4,2.0):
        for ang,(dx,dy) in [(0,(0,-(hh+gap))),(0,(0,hh+gap)),(90,(-(hw+gap),0)),(90,(hw+gap,0)),(0,(-(hw+gap+0.8),0)),(0,(hw+gap+0.8,0))]:
            for sh in (0,0.8,-0.8,1.6,-1.6):
                cands.append((ang,dx+(sh if ang==0 and dx==0 else 0),dy+(sh if ang==90 or dy==0 else 0)))
    done=False
    for ang,dx,dy in cands:
        r.SetTextAngleDegrees(ang); r.SetPosition(pcbnew.VECTOR2I(c.x+mm(dx),c.y+mm(dy)))
        bb=box(r,0.1)
        if free(bb,lay): obs[lay].append(bb); done=True; break
    if not done: r.SetVisible(False); hidden.append(f.GetReference())
print('hidden',hidden)
f=pcbnew.ZONE_FILLER(b); f.Fill(b.Zones())
b.Save('thermostat-v3.kicad_pcb')
