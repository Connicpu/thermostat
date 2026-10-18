import pcbnew, json, sys
parts=json.load(open('parts.json'))
b=pcbnew.BOARD(); OX,OY=100,100; mm=pcbnew.FromMM
def pt(x,y): return pcbnew.VECTOR2I(mm(OX+x),mm(OY+y))
nets={}
def net(n):
    if n not in nets: ni=pcbnew.NETINFO_ITEM(b,n); b.Add(ni); nets[n]=ni
    return nets[n]
FPDIR='/usr/share/kicad/footprints/'; fps={}
for p in parts:
    lib,name=p['fp'].split(':')
    f=pcbnew.FootprintLoad('kicad/tstat.pretty' if lib=='tstat' else FPDIR+lib+'.pretty',name); assert f,p['fp']
    f.SetFPID(pcbnew.LIB_ID(lib,name))
    f.SetReference(p['ref']); f.SetValue(p['value'])
    f.SetField('LCSC',p['lcsc']); f.GetFieldByName('LCSC').SetVisible(False)
    f.SetPath(pcbnew.KIID_PATH('/'+p['uuid'])); f.SetSheetfile('thermostat-v3.kicad_sch'); f.SetSheetname('')
    f.SetDNP(p['dnp'])
    if not p['bom']: f.SetExcludedFromBOM(True); f.SetExcludedFromPosFiles(True)
    for pad in f.Pads():
        n=p['pins'].get(pad.GetNumber())
        if n: pad.SetNet(net(n))
    f.Value().SetVisible(False)
    b.Add(f); fps[p['ref']]=f
P={}
def s(r,x,y,a=0): P[r]=(x,y,a)
s('J1',70,58,180)
for n,yc in enumerate([10,25,40]):
    s(f'Q{2+n}',21,yc,0); s(f'R{23+n*10}',27.2,yc,90)
    s(f'U{6+n}',34.5,yc,90); s(f'R{22+n*10}',30.5,yc+6.2,0)
    s(f'R{24+n*10}',15.5,yc-5.6,0); s(f'C{20+n}',20.5,yc-5.6,0)
    s(f'R{21+n*10}',42.5,yc+2.54,0); s(f'R{20+n*10}',42.5,yc+4.3,0)
s('F1',13.5,46,0); s('D1',16,53.5,180); s('R51',20.0,48.6,90); s('D2',16,59.3,180)
# The buck stage sits left so the 4.5 mm L1 is outside the e-paper footprint (panel x>=29.46); the L1 SW pad faces the U1 SW pin
s('L1',24.1,56.5,180); s('U1',32,52,0); s('C2',28,50,90); s('D4',31,57.3,270); s('C3',34.2,55.7,0); s('R50',35.3,49.6,180)
s('R1',37.5,51.5,0); s('R2',37.5,53,0); s('C4',22.0,61.8,0); s('C5',25.6,61.8,0)
s('C1',44.5,55.5,0); s('C41',44.5,59.0,0); s('C42',49.0,56.0,90); s('C43',55,58,0)
s('U3',75,15.74,0); s('C7',63.3,8.1,0); s('C8',63.3,9.6,0); s('R5',63.3,11.3,0); s('C9',63.3,12.8,0)
s('U4',59,17,0); s('R12',59,20.5,0); s('C10',56.2,17,90)
s('R11',63.3,19.5,0); s('D5',63.3,21.2,0); s('R10',63.3,22.9,0)
s('SW1',93,5,0); s('SW2',102,5,0); s('R6',97.5,10,0)
s('J4',100,44,90)
for r,y in [('C35',35.5),('C34',37),('C33',38.5),('C32',40),('C31',41.5),('C36',43),('C38',44.5),('C39',46.2),('C30',48)]: s(r,95,y,0)
s('L2',87,52,0); s('Q1',91,54.5,0); s('R16',91,57.5,0); s('R15',94.5,55,90)
s('D6',86,47.5,0); s('C40',86,57,0); s('D7',85,60.5,0); s('D8',91,60.5,0)
s('SW3',112,30,0); s('SW4',112,40,0); s('SW5',112,50,0)
s('R7',109.5,34,0); s('R8',109.5,44,0); s('R9',109.5,54,0)
TX,TY=128,32
s('U5',TX,TY,0); s('R13',117.3,26.5,90); s('R14',118.8,26.5,90); s('C11',117,39.8,0)
s('J3',123,8,180)
s('J2',121,60.35,0); s('U2',121,51,0); s('R3',117.5,50.5,90); s('R4',126,52,90); s('C6',128,48,90); s('D3',119,46.5,0)
s('TP1',30,61.5); s('TP2',33.5,61.5); s('TP3',47,62.2); s('TP4',114.5,46.5)
s('H1',6.4,6.4); s('H2',133.6,6.4); s('H3',6.4,57.6); s('H4',133.6,57.6)
for r,(x,y,a) in P.items(): fps[r].SetPosition(pt(x,y)); fps[r].SetOrientationDegrees(a)
# J1 vertical-entry header on the BACK (wires from wall)
fps['J1'].Flip(fps['J1'].GetPosition(),pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
for _t in ('TP1','TP2','TP3','TP4'): fps[_t].Flip(fps[_t].GetPosition(),pcbnew.FLIP_DIRECTION_LEFT_RIGHT)   # test pads on the back
fps['C43'].Flip(fps['C43'].GetPosition(),pcbnew.FLIP_DIRECTION_LEFT_RIGHT)   # 47uF/63V THT bulk on the BACK, in the standoff space
_j=fps['J1']; assert _j.GetLayer()==pcbnew.B_Cu and abs(pcbnew.ToMM(_j.GetPosition().x)-OX-70)<0.01 and abs(pcbnew.ToMM(_j.GetPosition().y)-OY-58)<0.01, 'J1 must sit at (70,58) on B.Cu (case wire hole)'
miss=[r for r in fps if r not in P]; assert not miss, miss
W,H=140,64
def shape(kind,a,b2,layer=pcbnew.Edge_Cuts,w=0.1):
    sh=pcbnew.PCB_SHAPE(b); sh.SetShape(kind); sh.SetStart(pt(*a)); sh.SetEnd(pt(*b2)); sh.SetLayer(layer); sh.SetWidth(mm(w)); b.Add(sh)
shape(pcbnew.SHAPE_T_RECT,(0,0),(W,H))
def keep(x1,y1,x2,y2):
    k=pcbnew.ZONE(b); k.SetIsRuleArea(True); k.SetDoNotAllowTracks(True); k.SetDoNotAllowVias(True); k.SetDoNotAllowPads(False); k.SetDoNotAllowCopperPour(True); k.SetDoNotAllowFootprints(False); k.SetLayerSet(pcbnew.LSET.AllCuMask())
    o=k.Outline(); o.NewOutline()
    for xx,yy in [(x1,y1),(x2,y1),(x2,y2),(x1,y2)]: o.Append(mm(OX+xx),mm(OY+yy))
    b.Add(k)
def slot(x1,y1,x2,y2,w=1.2):
    hw=w/2; r=(x1,y1-hw,x2,y1+hw) if y1==y2 else (x1-hw,y1,x1+hw,y2)
    shape(pcbnew.SHAPE_T_RECT,(r[0],r[1]),(r[2],r[3])); keep(r[0]-0.5,r[1]-0.5,r[2]+0.5,r[3]+0.5)
d=7; g=1.5
slot(TX-d,TY-d,TX-g,TY-d); slot(TX+g,TY-d,TX+d,TY-d); slot(TX-d,TY+d,TX-g,TY+d); slot(TX+g,TY+d,TX+d,TY+d)
slot(TX-d,TY-d+1.5,TX-d,TY-3); slot(TX-d,TY+3,TX-d,TY+d-1.5); slot(TX+d,TY-d+1.5,TX+d,TY-g); slot(TX+d,TY+g,TX+d,TY+d-1.5)
keep(61,0,89,6.4); b.Zones()[-1].SetZoneName('ANT_KEEPOUT')   # ESP32-C6 antenna: module width +5 mm each side, both layers
keep(TX-7.6,TY-7.6,TX+7.6,TY+7.6); b.Zones()[-1].SetZoneName('ISLAND_ROUTE') if hasattr(b.Zones(),'__getitem__') else None
def txt(t,x,y,sz=1.0,layer=pcbnew.F_SilkS,ang=0):
    o=pcbnew.PCB_TEXT(b); o.SetText(t); o.SetPosition(pt(x,y)); o.SetLayer(layer); o.SetTextSize(pcbnew.VECTOR2I(mm(sz),mm(sz))); o.SetTextThickness(mm(sz*0.15)); o.SetTextAngleDegrees(ang); b.Add(o)
# rear wiring labels next to J1 (back silk, mirrored)
for pad in fps['J1'].Pads():
    px=pcbnew.ToMM(pad.GetPosition().x)-OX; py=pcbnew.ToMM(pad.GetPosition().y)-OY
    o=pcbnew.PCB_TEXT(b); o.SetText('RCWYG'[int(pad.GetNumber())-1]); o.SetPosition(pt(px,py+3.6)); o.SetLayer(pcbnew.B_SilkS); o.SetTextSize(pcbnew.VECTOR2I(mm(1.5),mm(1.5))); o.SetTextThickness(mm(0.25)); o.SetMirrored(True); b.Add(o)
o=pcbnew.PCB_TEXT(b); o.SetText('24VAC HVAC  (plug: 15EDGK-3.5-05P)'); o.SetPosition(pt(92,56)); o.SetLayer(pcbnew.B_SilkS); o.SetTextSize(pcbnew.VECTOR2I(mm(0.9),mm(0.9))); o.SetTextThickness(mm(0.15)); o.SetMirrored(True); b.Add(o)
txt('! NO USB WHILE 24VAC IS WIRED',124.5,41.6,0.8); txt('(unless via USB isolator)',124.5,43.0,0.8)
o=pcbnew.PCB_TEXT(b); o.SetText("Connie's Super\nAwesome Thermostat\n\nVERSION 3.0"); o.SetPosition(pt(98,24)); o.SetLayer(pcbnew.B_SilkS); o.SetTextSize(pcbnew.VECTOR2I(mm(2),mm(2))); o.SetTextThickness(mm(0.3)); o.SetMirrored(True); b.Add(o)
lg=pcbnew.FootprintLoad('kicad/tstat.pretty','LOGO_CAT'); lg.SetReference('LOGO1'); lg.Reference().SetVisible(False); lg.Value().SetVisible(False); lg.SetPosition(pt(45,30)); b.Add(lg); lg.Flip(lg.GetPosition(),pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
txt('UP',112,26.3,0.8); txt('DOWN',112,36.3,0.8); txt('MODE',112,46.3,0.8)
txt('RESET',93,8.6,0.8); txt('BOOT',102,8.6,0.8)
for a,b2 in [((29.46,26.24),(108.46,26.24)),((108.46,26.24),(108.46,62.94)),((108.46,62.94),(29.46,62.94)),((29.46,62.94),(29.46,26.24))]: shape(pcbnew.SHAPE_T_SEGMENT,a,b2,pcbnew.Cmts_User,0.15)
txt('2.9in EPD outline, 5mm above PCB (FPC exits right edge, folded under, contacts down) - see fab/display-fit.png',69,27.7,0.8,pcbnew.Cmts_User)
def trk(pts,n,w=0.2,L=pcbnew.F_Cu):
    for (x1,y1),(x2,y2) in zip(pts,pts[1:]):
        t=pcbnew.PCB_TRACK(b); t.SetStart(pt(x1,y1)); t.SetEnd(pt(x2,y2)); t.SetWidth(mm(w)); t.SetLayer(L); t.SetNet(net(n)); b.Add(t)
pads={p.GetNumber():(pcbnew.ToMM(p.GetPosition().x)-OX,pcbnew.ToMM(p.GetPosition().y)-OY) for p in fps['U5'].Pads()}
E=TX-9
x1,y1=pads['1']; trk([(x1,y1),(E,y1)],'I2C_SDA')
x2,y2=pads['2']; trk([(x2,y2),(E,y2)],'I2C_SCL')
x3,y3=pads['3']; trk([(x3,y3),(x3+0.9,y3),(x3+0.9,y3+1.2),(E,y3+1.2)],'+3V3')
x4,y4=pads['4']; trk([(x4,y4),(x4+0.9,y4),(x4+0.9,y4-1.2),(E,y4-1.2)],'GND')
def via(x,y,n,w=0.6,d=0.3):
    v=pcbnew.PCB_VIA(b); v.SetPosition(pt(x,y)); v.SetWidth(mm(w)); v.SetDrill(mm(d)); v.SetNet(net(n)); b.Add(v)
vx,vy=118.05,28.4
trk([(E,y3+1.2),(vx,y3+1.2),(vx,vy)],'+3V3',0.25,pcbnew.B_Cu); via(vx,vy,'+3V3')
r13=[p for p in fps['R13'].Pads() if p.GetNumber()=='1'][0].GetPosition(); r14=[p for p in fps['R14'].Pads() if p.GetNumber()=='1'][0].GetPosition()
trk([(vx,vy),(pcbnew.ToMM(r13.x)-OX,pcbnew.ToMM(r13.y)-OY)],'+3V3',0.25); trk([(vx,vy),(pcbnew.ToMM(r14.x)-OX,pcbnew.ToMM(r14.y)-OY)],'+3V3',0.25)
via(E,y1,'I2C_SDA'); via(E,y2,'I2C_SCL'); via(E,y3+1.2,'+3V3'); via(E,y4-1.2,'GND')
assert pcbnew.ToMM(fps['L1'].GetBoundingBox(False).GetRight())-OX<29.46-0.3, 'L1 must stay outside the EPD footprint'
# VIN_RECT (D1+ -> R51) is pre-routed around D1's GND pad so the autorouter works around it
trk([(19.1,54.7),(20.55,54.7),(20.55,50.06),(20.0,50.06)],'VIN_RECT',0.35)
# DRV_G (R40/R41 -> U3 pad 27) is pre-routed; the HV gate traces would otherwise wall the autorouter off
trk([(41.99,42.54),(41.99,44.3)],'DRV_G',0.25); trk([(41.99,43.183),(81.028,43.183),(85.014,39.197)],'DRV_G',0.25); via(85.014,39.197,'DRV_G')
trk([(85.014,39.197),(83.746,37.929),(83.746,13.135),(82.539,11.928)],'DRV_G',0.25,pcbnew.B_Cu); via(82.539,11.928,'DRV_G')
trk([(82.539,11.928),(82.723,11.744),(82.723,8.75),(83.75,8.75)],'DRV_G',0.25)
TW_HV=1.0
# Triac MT1 snubber-resistor hop (R23/R33/R43 pad2 -> Q MT1) is pre-routed; the autorouter leaves it open on some runs
for _n,_yc in enumerate([10,25,40]):
    _net='HVAC_'+'WYG'[_n]; _d=_yc-25
    trk([(x,y+_d) for x,y in [(27.2,24.09),(27.33,23.96),(27.33,21.85),(24.88,19.4),(21.45,19.4),(21.68,19.63),(21.68,23.76),(23.41,25.49),(24.42,25.49),(24.42,27.29)]],_net,TW_HV)
# VIN_HV feed C43 -> D3 is pre-routed on the back, below J1
trk([(55.0,58.0),(58.36,61.36),(93.68,61.36),(108.54,46.5)],'VIN_HV',0.5,pcbnew.B_Cu); via(108.54,46.5,'VIN_HV',0.8,0.4); trk([(108.54,46.5),(116.8,46.5)],'VIN_HV',0.5)
# GND stitching for the buck input pocket (C2 / U1 GND). The HV traces cut that pocket off from the main top pour
trk([(28.0,48.53),(26.3,48.53)],'GND',0.5); via(26.3,48.53,'GND')
trk([(33.35,52.0),(34.9,52.0)],'GND',0.4); via(34.9,52.0,'GND')
# EPD +3V3 (J4 pins 15/16) escape via so the router can reach it from the back layer
trk([(99.05,42.24),(99.93,42.24),(99.93,42.74),(99.05,42.74)],'+3V3',0.25); trk([(99.93,42.49),(101.0,42.49)],'+3V3',0.3); via(101.0,42.49,'+3V3')
ds=b.GetDesignSettings(); ds.SetAuxOrigin(pt(0,64)); ds.SetGridOrigin(pt(0,64))   # fab origin = board bottom-left (gerbers, drill, CPL)
pcbnew.SaveBoard(sys.argv[1],b)
print('saved')
