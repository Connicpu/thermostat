# Rev 8 (review fixes) applied to parts.json - idempotent
import json,copy
P=json.load(open('parts.json')); R={p['ref']:p for p in P}
R['D3'].update(value='SS210',fp='tstat:SMA_SS210',lcsc='C14996',mpn='SS210 100V 2A Schottky (was B5819W 40V)')
R['F1'].update(value='0.5A 60V PTC',lcsc='C70113',mpn='mSMD050-60V')
# optos stay MOC3063S zero-cross: field-proven on this HVAC system in V1 (no snubbers)
R['U1']['pins']['4']='BUCK_EN'
if 'R50' not in R:
    r=copy.deepcopy(R['R1']); r.pop('uuid',None)
    r.update(ref='R50',value='100k',lcsc='C25741',mpn='0402WGF1003TCE',pins={'1':'VIN_HV','2':'BUCK_EN'})
    P.insert(P.index(R['U1'])+1,r)
if 'C43' not in R:
    c=copy.deepcopy(R['C1']); c.pop('uuid',None)
    c.update(ref='C43',sym='Device:C_Polarized',value='47uF 63V',fp='Capacitor_THT:CP_Radial_D8.0mm_P3.50mm',lcsc='C19270693',
             mpn='JBLH2470M063D090RLM 8x9 THT, BACK side, hand-solder',pins={'1':'VIN_HV','2':'GND'},bom=True)
    P.insert(P.index(R['C1'])+1,c)
json.dump(P,open('parts.json','w'),indent=1)
print([ (p['ref'],p['value'],p['fp'],p['pins']) for p in P if p['ref'] in ('R1','R50','C43','D3')])
# standard-gate, snubberless triac (field-proven V1 used standard-gate BT136-600, IGT ~35 mA): ST T435-600B, 4A 600V, IGT 35 mA, DPAK (1=A1/MT1, tab=A2/MT2, 3=G)
P=json.load(open('parts.json'))
for p in P:
    if p['ref'] in ('Q2','Q3','Q4'): p.update(value='T435-600B',lcsc='C145128',mpn='T435-600B-TR (ST, standard-gate snubberless, was BT136S-600D)')
json.dump(P,open('parts.json','w'),indent=1)
# bring-up test points (bare pads, back side, no BOM/assembly cost)
P=json.load(open('parts.json')); refs={p['ref'] for p in P}
for r,n in [('TP1','VIN_HV'),('TP2','GND'),('TP3','+3V3'),('TP4','VBUS')]:
    if r not in refs: P.append({'ref':r,'sym':'Connector:TestPoint','value':n,'fp':'TestPoint:TestPoint_Pad_D1.5mm','lcsc':'','pins':{'1':n},'dnp':False,'bom':False,'mpn':'test pad'})
json.dump(P,open('parts.json','w'),indent=1)
