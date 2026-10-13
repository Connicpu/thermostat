import pcbnew
mm=pcbnew.FromMM
b=pcbnew.LoadBoard('thermostat-v3.kicad_pcb')
print('ses',pcbnew.ImportSpecctraSES(b,'../route.ses'))
gnd=b.FindNet('GND')
OX,OY=100,100
for layer in (pcbnew.F_Cu,pcbnew.B_Cu):
    z=pcbnew.ZONE(b); z.SetLayer(layer); z.SetNet(gnd)
    ol=z.Outline(); ol.NewOutline()
    for x,y in [(0.5,0.5),(139.5,0.5),(139.5,63.5),(0.5,63.5)]: ol.Append(mm(OX+x),mm(OY+y))
    z.SetLocalClearance(mm(0.3)); z.SetMinThickness(mm(0.25)); z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    z.SetAssignedPriority(0); b.Add(z)
# keepout over HVAC high-side / thermal island? (no GND under SHT island to reduce heat coupling)
U5=[f for f in b.GetFootprints() if f.GetReference()=='U5'][0]; c=U5.GetPosition()
k=pcbnew.ZONE(b); k.SetIsRuleArea(True); k.SetDoNotAllowCopperPour(True); k.SetDoNotAllowTracks(False); k.SetDoNotAllowVias(False); k.SetDoNotAllowPads(False); k.SetDoNotAllowFootprints(False)
k.SetLayerSet(pcbnew.LSET.AllCuMask())
ol=k.Outline(); ol.NewOutline()
for dx,dy in [(-7.6,-7.6),(7.6,-7.6),(7.6,7.6),(-7.6,7.6)]: ol.Append(c.x+mm(dx),c.y+mm(dy))
k.SetZoneName('ISLAND'); b.Add(k)
for z in list(b.Zones()):
    if z.GetZoneName()=='ISLAND_ROUTE': b.Remove(z)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save('thermostat-v3.kicad_pcb')
