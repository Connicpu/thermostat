# Rev 9: hand-route VIN_RECT (D1+ -> R51) around D1's GND pad; drop the autorouter GND stub that blocks it (D1 GND pad is fed by the pour)
import pcbnew
b=pcbnew.LoadBoard('thermostat-v3.kicad_pcb'); mm=pcbnew.FromMM
P=lambda x,y: pcbnew.VECTOR2I(mm(100+x),mm(100+y))
for t in list(b.GetTracks()):
    if t.GetNetname()=='GND' and t.GetClass()=='PCB_TRACK':
        xs=[pcbnew.ToMM(v)-100 for v in (t.GetStart().x,t.GetEnd().x)]; ys=[pcbnew.ToMM(v)-100 for v in (t.GetStart().y,t.GetEnd().y)]
        if min(xs)<21.5 and max(xs)>19 and 51.5<min(ys) and max(ys)<56: print('rm GND',xs,ys); b.Remove(t)
n=b.FindNet('VIN_RECT'); pts=[(19.1,54.7),(21.2,54.7),(21.2,50.06),(20.0,50.06)]
for a,c in zip(pts,pts[1:]):
    t=pcbnew.PCB_TRACK(b); t.SetStart(P(*a)); t.SetEnd(P(*c)); t.SetWidth(mm(0.5)); t.SetLayer(pcbnew.F_Cu); t.SetNet(n); b.Add(t)
pcbnew.ZONE_FILLER(b).Fill(b.Zones()); b.Save('thermostat-v3.kicad_pcb')
