import pcbnew, json, sys
mm=pcbnew.FromMM
b=pcbnew.LoadBoard('kicad/thermostat-v3.kicad_pcb')
pro=json.load(open('pro_template.json'))
ns=b.GetDesignSettings().m_NetSettings
for c in pro['net_settings']['classes']:
    if c['name']=='Default': nc=ns.GetDefaultNetclass()
    else: nc=pcbnew.NETCLASS(c['name'])
    nc.SetTrackWidth(mm(c['track_width'])); nc.SetClearance(mm(c['clearance'])); nc.SetViaDiameter(mm(c['via_diameter'])); nc.SetViaDrill(mm(c['via_drill']))
    if c['name']!='Default': ns.SetNetclass(c['name'],nc)
for p in pro['net_settings']['netclass_patterns']: ns.SetNetclassPatternAssignment(p['pattern'],p['netclass'])
for n in b.GetNetsByName().values() if False else []: pass
import fnmatch
cls={c['name']:ns.GetNetClassByName(c['name']) for c in pro['net_settings']['classes']}
for name,ni in b.GetNetsByName().items():
    name=str(name)
    for p in pro['net_settings']['netclass_patterns']:
        if fnmatch.fnmatchcase(name,p['pattern']): ni.SetNetClass(cls[p['netclass']]); break

print(pcbnew.ExportSpecctraDSN(b,sys.argv[1]))
