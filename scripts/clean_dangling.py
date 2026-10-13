import pcbnew
b=pcbnew.LoadBoard('thermostat-v3.kicad_pcb')
def key(p): return (round(p.x/1000),round(p.y/1000))
tot=0
pads=list(b.GetPads())
while True:
    pts={}
    tr=list(b.GetTracks())
    for t in tr:
        for p in ([t.GetStart(),t.GetEnd()] if t.GetClass()!='PCB_VIA' else [t.GetPosition()]):
            pts.setdefault((t.GetNetCode(),key(p)),[]).append(t)
    rm=[]
    for t in tr:
        if t.GetClass()=='PCB_VIA': continue
        for p in (t.GetStart(),t.GetEnd()):
            if len(pts[(t.GetNetCode(),key(p))])>1: continue
            # pad hit?
            hit=False
            for pad in pads:
                if pad.GetNetCode()==t.GetNetCode() and pad.HitTest(p) : hit=True; break
            if not hit: rm.append(t); break
    if not rm: break
    for t in rm: b.Remove(t)
    tot+=len(rm)
pcbnew.ZONE_FILLER(b).Fill(b.Zones()); b.Save('thermostat-v3.kicad_pcb'); print('removed',tot)
