# Rev 12 (cost): D2 SMBJ40A (C114003, extended) -> SMBJ40CA (C19077590, JLC preferred-extended = no loading fee).
# Same SMB package/footprint, same 40 V standoff / 44.4 V min breakdown / 64.5 V clamp @ 9.3 A (10/1000us);
# bidirectional clamps identically for the positive rail and removes the polarity concern. Idempotent.
import json,pcbnew
P=json.load(open('parts.json'))
for p in P:
    if p['ref']=='D2': p.update(value='SMBJ40CA',lcsc='C19077590',mpn='SMBJ40CA (bidirectional, JLC preferred-extended; was SMBJ40A C114003)')
json.dump(P,open('parts.json','w'),indent=1)
b=pcbnew.LoadBoard('kicad/thermostat-v3.kicad_pcb'); f=b.FindFootprintByReference('D2'); f.SetValue('SMBJ40CA')
for fl in f.GetFields():
    if fl.GetName() in ('LCSC','LCSC Part'): fl.SetText('C19077590')
b.Save('kicad/thermostat-v3.kicad_pcb')
