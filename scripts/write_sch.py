import json, uuid
from sx import *
U=lambda: str(uuid.uuid4())
parts=json.load(open('parts.json'))
PROJ='thermostat-v3'
# custom symbol pin layouts: name -> (left list, right list) of (num,name)
CUST={
'HVAC_TERM':([('1','R'),('2','C'),('3','W'),('4','Y'),('5','G')],[]),
'LMR16006':([('5','VIN'),('4','~{SHDN}'),('2','GND')],[('1','CB'),('6','SW'),('3','FB')]),
'USB_C':([('A4','VBUS'),('A9','VBUS'),('B4','VBUS'),('B9','VBUS'),('A5','CC1'),('B5','CC2'),('A8','SBU1'),('B8','SBU2')],
         [('A6','D+'),('B6','D+'),('A7','D-'),('B7','D-'),('A1','GND'),('A12','GND'),('B1','GND'),('B12','GND'),('S1','SHIELD')]),
'USBLC6':([('1','IO1'),('3','IO2'),('2','GND')],[('6','IO1'),('4','IO2'),('5','VBUS')]),
'ESP32-C6-WROOM-1':([(str(i),n) for i,n in zip(range(1,15),['GND','3V3','EN','IO4','IO5','IO6','IO7','IO0','IO1','IO8','IO10','IO11','IO12/D-','IO13/D+'])]+[('29','EP_GND')],
                    [(str(i),n) for i,n in zip(range(15,29),['IO9','IO18','IO19','IO20','IO21','IO22','IO23','NC','IO15','RXD0','TXD0','IO3','IO2','GND'])]),
'STWD100':([('5','VCC'),('4','WDI'),('3','~{EN}'),('2','GND')],[('1','~{WDO}')]),
'SHT4x':([('3','VDD'),('4','VSS')],[('1','SDA'),('2','SCL')]),
'STEMMA':([('1','GND'),('2','3V3'),('3','SDA'),('4','SCL'),('MP','MP')],[]),
'MOC3063':([('1','A'),('2','K'),('3','NC')],[('6','MT2'),('5','NC'),('4','MT1')]),
'TRIAC':([('3','G')],[('2','MT2'),('1','MT1')]),
'BRIDGE_MB10S':([('3','~'),('4','~')],[('1','+'),('2','-')]),  # C2488 EasyEDA pad numbering: 1=+ 2=- 3,4=AC
'EPD_FPC24':([(str(i),n) for i,n in zip(range(1,13),['NC','GDR','RESE','NC','VSH2','TSCL','TSDA','BS1','BUSY','RES#','D/C#','CS#'])]+[('MP','MP')],
             [(str(i),n) for i,n in zip(range(13,25),['SCL','SDA','VDDIO','VCI','VSS','VDD','VPP','VSH1','VGH','VSL','VGL','VCOM'])]),
}
def mk_custom(name):
    L,Rr=CUST[name]; n=max(len(L),len(Rr)); h=(n+1)*2.54; w=15.24
    body=f'(symbol "{name}_0_1" (rectangle (start {-w/2} {h/2}) (end {w/2} {-h/2}) (stroke (width 0.254) (type default)) (fill (type background))))'
    ps=[]
    for side,lst in ((0,L),(1,Rr)):
        for k,(num,nm) in enumerate(lst):
            y=h/2-2.54*(k+1); x=-w/2-2.54 if side==0 else w/2+2.54; ang=0 if side==0 else 180
            ps.append(f'(pin passive line (at {x:.2f} {y:.2f} {ang}) (length 2.54) (name "{nm}" (effects (font (size 1.0 1.0)))) (number "{num}" (effects (font (size 1.0 1.0)))))')
    s=(f'(symbol "{name}" (exclude_from_sim no) (in_bom yes) (on_board yes) '
       f'(property "Reference" "U" (at 0 {h/2+1.27} 0) (effects (font (size 1.27 1.27)))) '
       f'(property "Value" "{name}" (at 0 {-h/2-1.27} 0) (effects (font (size 1.27 1.27)))) '
       f'(property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes))) '
       f'(property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes))) '
       f'{body} (symbol "{name}_1_1" {" ".join(ps)}))')
    return s
symdefs={}; sympins={}
for p in parts:
    lib,nm=p['sym'].split(':')
    if p['sym'] in symdefs: continue
    if lib=='tstat':
        s=mk_custom(nm); sx=parse(s)[0]
    else:
        sx=libsym(lib,nm)
    sympins[p['sym']]={q[0]:(q[2],q[3],q[4]) for q in pins(sx)}
    sx=list(sx); sx[1]=f'"{p["sym"]}"'
    symdefs[p['sym']]=dump(sx)
# library file for custom symbols
open('kicad/tstat.kicad_sym','w').write('(kicad_symbol_lib (version 20241209) (generator "gen") (generator_version "9.0") '+' '.join(mk_custom(n) for n in CUST)+')')
import re
BLOCKS=[('24VAC INPUT, PROTECTION & 3.3V BUCK',['J1','F1','D1','R51','D2','C1','C43','C41','C42','C2','U1','R50','C3','D4','L1','R1','R2','C4','C5','TP1','TP2','TP3','TP4'],(15,15,280,115)),
('USB-C (FLASHING ONLY) & POWER-PATH  -  do not connect USB to an earthed PC while 24VAC is wired',['J2','R3','R4','U2','C6','D3'],(300,15,280,115)),
('ESP32-C6 MCU, RESET/BOOT, WATCHDOG, STATUS LED',['U3','C7','C8','R5','C9','SW1','SW2','R6','U4','R12','C10','R10','D5','R11'],(15,138,280,118)),
('HVAC OUTPUTS W / Y / G  (zero-cross opto + triac, snubbers DNP)',[r for r in [p['ref'] for p in parts] if re.match(r'^(R[234]\d|U[678]|Q[234]|C2[012])$',r)],(300,138,280,118)),
('2.9in E-PAPER (GDEY029T94) FPC + BOOST (Good Display ref. circuit)',['J4']+[f'C{i}' for i in range(30,41)]+['L2','Q1','R15','R16','D6','D7','D8'],(15,264,280,118)),
('SHT40 (THERMAL ISLAND), STEMMA QT, UI BUTTONS, MOUNTING',['R13','R14','U5','C11','J3','SW3','SW4','SW5','R7','R8','R9','H1','H2','H3','H4'],(300,264,280,118))]
blk={}
for t,refs,reg in BLOCKS:
    for r in refs: blk[r]=(t,reg)
out=[]; cursor={}
for t,refs,(bx,by,bw,bh) in BLOCKS:
    out.append(f'(rectangle (start {bx} {by}) (end {bx+bw} {by+bh}) (stroke (width 0.3) (type dash)) (fill (type none)) (uuid "{U()}"))')
    out.append(f'(text "{t}" (exclude_from_sim no) (at {bx+2} {by+4} 0) (effects (font (size 2 2) (bold yes)) (justify left)) (uuid "{U()}"))')
order={p['ref']:i for i,p in enumerate(parts)}
for p in parts:
    t,(bx,by,bw,bh)=blk[p['ref']]
    c=cursor.setdefault(t,[bx+4,by+10,0])
    pp=sympins[p['sym']]
    ys=[v[1] for v in pp.values()] or [0]; xs=[v[0] for v in pp.values()] or [0]
    hgt=(max(ys)-min(ys)) if ys else 5; wid=(max(xs)-min(xs)) if xs else 5
    W=max(wid,8)+24; Hh=max(hgt,8)+10
    if c[0]+W>bx+bw: c[0]=bx+4; c[1]+=c[2]; c[2]=0
    x,y=c[0],c[1]
    X=round((x+W/2)/1.27)*1.27; Y=round((y+Hh/2)/1.27)*1.27
    c[0]+=W; c[2]=max(c[2],Hh)
    uid=p.get('uuid') or U(); p['uuid']=uid
    props=[('Reference',p['ref']),('Value',p['value']),('Footprint',p['fp']),('Datasheet',''),('LCSC',p['lcsc']),('MPN',p.get('mpn',''))]
    pr=' '.join(f'(property "{k}" "{v}" (at {X} {Y+(-3 if k=="Reference" else 3 if k=="Value" else 0)} 0) (effects (font (size 1.27 1.27)){"" if k in ("Reference","Value") else " (hide yes)"}))' for k,v in props)
    pinl=' '.join(f'(pin "{n}" (uuid "{U()}"))' for n in pp)
    bom='yes' if p['bom'] else 'no'
    out.append(f'(symbol (lib_id "{p["sym"]}") (at {X} {Y} 0) (unit 1) (exclude_from_sim no) (in_bom {bom}) (on_board yes) (dnp {"yes" if p["dnp"] else "no"}) (uuid "{uid}") {pr} {pinl} (instances (project "{PROJ}" (path "/ROOTUUID" (reference "{p["ref"]}") (unit 1)))))')
    for num,(px,py,ang) in pp.items():
        ex,ey=round(X+px,2),round(Y-py,2)
        net=p['pins'].get(num,'__NC__')
        if net is None or net=='__NC__':
            out.append(f'(no_connect (at {ex} {ey}) (uuid "{U()}"))'); continue
        rot={0:180,180:0,90:270,270:90}[ang]
        just='right' if rot in (180,270) else 'left'
        out.append(f'(global_label "{net}" (shape passive) (at {ex} {ey} {rot}) (fields_autoplaced yes) (effects (font (size 1.0 1.0)) (justify {just})) (uuid "{U()}"))')
root=U()
txt=(f'(kicad_sch (version 20250114) (generator "eeschema") (generator_version "9.0") (uuid "{root}") (paper "A2") '
     f'(title_block (title "Connie\'s Super Awesome Thermostat V3") (rev "3.0") (comment 1 "ESP32-C6 Zigbee, 24VAC R/C/W/Y/G, JLCPCB assembly")) '
     f'(lib_symbols {" ".join(symdefs.values())}) {" ".join(out)} (sheet_instances (path "/" (page "1"))))').replace('ROOTUUID',root)
open(f'kicad/{PROJ}.kicad_sch','w').write(txt)
json.dump(parts,open('parts.json','w'),indent=0)
print('ok',len(parts))
