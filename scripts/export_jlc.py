import pcbnew, csv, collections
b=pcbnew.LoadBoard('thermostat-v3.kicad_pcb')
rows=collections.OrderedDict(); cpl=[]
ox,oy=100,164
for f in b.GetFootprints():
    ref=f.GetReference(); fld=f.GetFieldByName('LCSC'); l=fld.GetText() if fld else ''
    if f.IsDNP() or f.IsExcludedFromBOM() or ref in ('J1','C43') or not l: continue
    CM={'C318884':'TS-1187A-B-A-B'}
    key=(CM.get(l,f.GetValue()),str(f.GetFPID().GetLibItemName()),l)
    rows.setdefault(key,[]).append(ref)
    p=f.GetPosition()
    if ref=='J2':  # KiCad HRO origin != EasyEDA/JLC origin; use EasyEDA-verified offset (EasyEDA origin sits at KiCad local y=-1.575 (pads y -4.045 vs -2.47))
        import math
        a=math.radians(f.GetOrientationDegrees()); dx,dy=0,-1.575
        gx=dx*math.cos(a)+dy*math.sin(a); gy=-dx*math.sin(a)+dy*math.cos(a)
        p=pcbnew.VECTOR2I(p.x+pcbnew.FromMM(gx),p.y+pcbnew.FromMM(gy)); print('J2 corrected',gx,gy)
    cpl.append([ref,f'{pcbnew.ToMM(p.x)-ox:.3f}mm',f'{oy-pcbnew.ToMM(p.y):.3f}mm','Top',f'{f.GetOrientationDegrees()%360:.0f}'])
import re
k=lambda r:(re.sub(r'\d','',r),int(re.sub(r'\D','',r) or 0))
with open('../fab/thermostat-v3-BOM-JLC.csv','w',newline='') as fh:
    w=csv.writer(fh); w.writerow(['Comment','Designator','Footprint','LCSC Part #'])
    for (v,fp,l),refs in rows.items(): w.writerow([v,','.join(sorted(refs,key=k)),fp,l])
with open('../fab/thermostat-v3-CPL-JLC.csv','w',newline='') as fh:
    w=csv.writer(fh); w.writerow(['Designator','Mid X','Mid Y','Layer','Rotation']); w.writerows(sorted(cpl,key=lambda r:k(r[0])))
print(len(rows),'BOM lines',len(cpl),'placements')
