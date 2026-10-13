# Rev 11: Q2-Q4 T435-600B (IH 35 mA) -> ST T405-600B-TR logic-level 4A 600V DPAK (IGT 5 mA, IH 10 mA, IL 10/15 mA), C283473.
# Same DPAK pinout (1=A1/MT1, tab/2=A2/MT2, 3=G). Snubbers stay DNP. Idempotent.
import json
P=json.load(open('parts.json'))
for p in P:
    if p['ref'] in ('Q2','Q3','Q4'):
        p.update(value='T405-600B',lcsc='C283473',mpn='T405-600B-TR (ST T4 logic-level, IGT 5mA IH 10mA; was T435-600B IH 35mA)')
json.dump(P,open('parts.json','w'),indent=1)
# board fields
import pcbnew
b=pcbnew.LoadBoard('kicad/thermostat-v3.kicad_pcb')
for r in ('Q2','Q3','Q4'):
    f=b.FindFootprintByReference(r); f.SetValue('T405-600B')
    for fl in f.GetFields():
        if fl.GetName() in ('LCSC','LCSC Part'): fl.SetText('C283473')
b.Save('kicad/thermostat-v3.kicad_pcb')
