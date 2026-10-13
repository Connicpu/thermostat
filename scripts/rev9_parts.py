# Rev 9: series surge resistor R51 (bridge -> bulk) and 6045 22uH L1 - idempotent
import json,copy
P=json.load(open('parts.json')); R={p['ref']:p for p in P}
R['D1']['pins']['1']='VIN_RECT'
if 'R51' not in R:
    r=copy.deepcopy(R['R50']); r.pop('uuid',None)
    r.update(ref='R51',value='3.3R 0.75W pulse',fp='Resistor_SMD:R_1206_3216Metric',lcsc='C844038',mpn='CRCW12063R30FKEAHP (Vishay pulse-proof, 750 mW)',pins={'1':'VIN_RECT','2':'VIN_HV'})
    P.insert(P.index(R['D1'])+1,r)
R['L1'].update(fp='Inductor_SMD:L_Sunlord_SWPA6045S',lcsc='C83454',mpn='SWPA6045S220MT 22uH Isat 2.2A 89mOhm (was SWPA4030 1.0A)')
json.dump(P,open('parts.json','w'),indent=1)
