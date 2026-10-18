import re,sys,json,fnmatch
t=open(sys.argv[1]).read()
pro=json.load(open('pro_template.json'))
m=re.search(r'\(class kicad_default (.*?)\(circuit',t,re.S)
nets=m.group(1).split()
groups={'kicad_default':[]}
for n in nets:
    c='kicad_default'
    for p in pro['net_settings']['netclass_patterns']:
        if fnmatch.fnmatchcase(n,p['pattern']): c=p['netclass']; break
    groups.setdefault(c,[]).append(n)
cl={c['name']:c for c in pro['net_settings']['classes']}; cl['kicad_default']=cl['Default']
start=t.rfind('(class kicad_default'); end=t.index('(wiring',start); end=t.rfind(')',start,end)  # close of network
blocks=[]
for c,ns in groups.items():
    if not ns: continue
    k=cl[c]
    blocks.append(f'    (class {c} {" ".join(ns)}\n      (circuit (use_via "Via[0-1]_600:300_um"))\n      (rule (width {int(k["track_width"]*1000)}) (clearance {int(k["clearance"]*1000)}))\n    )\n')
# find end of the default class block
i=t.index('(class kicad_default'); depth=0; j=i
while True:
    if t[j]=='(': depth+=1
    elif t[j]==')':
        depth-=1
        if depth==0: break
    j+=1
t=t[:i]+''.join(blocks).lstrip()+t[j+1:]
# Hand pre-routes are protected from the autorouter
for n in ('DRV_G','VIN_RECT','HVAC_W','HVAC_Y','HVAC_G','VIN_HV','GND'): t=t.replace('(net %s)(type route)'%n,'(net %s)(type protect)'%n)
open(sys.argv[1],'w').write(t); print({k:len(v) for k,v in groups.items()})
