import trimesh, zipfile, numpy as np, json
def mesh_xml(oid,name,m):
    v='\n'.join(f'<vertex x="{x:.4f}" y="{y:.4f}" z="{z:.4f}"/>' for x,y,z in m.vertices)
    t='\n'.join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a,b,c in m.faces)
    return f'<object id="{oid}" name="{name}" type="model"><mesh><vertices>\n{v}\n</vertices><triangles>\n{t}\n</triangles></mesh></object>\n'
ld=lambda f: trimesh.load(f,process=True)
groups=[('front_shell',(5,5),[('shell_white',1),('shell_black',2),('shell_orange',3),('shell_pink',4)]),
        ('backplate',(5,85),[('backplate',1)]),
        ('button_caps_x3',(175,20),[('caps',3)])]
objs='';items='';cfg='<?xml version="1.0" encoding="UTF-8"?>\n<config>\n';oid=1;plate=''
for gname,(ox,oy),parts in groups:
    ms=[(p,e,ld(p+'.stl')) for p,e in parts]
    lo=np.min([m.bounds[0] for _,_,m in ms],axis=0)
    comp='';cfgp=''
    for p,e,m in ms:
        m.apply_translation(-lo)
        objs+=mesh_xml(oid,p,m); comp+=f'<component objectid="{oid}"/>'
        cfgp+=f'  <part id="{oid}" subtype="normal_part"><metadata key="name" value="{p}"/><metadata key="extruder" value="{e}"/></part>\n'
        oid+=1
    objs+=f'<object id="{oid}" name="{gname}" type="model"><components>{comp}</components></object>\n'
    items+=f'<item objectid="{oid}" transform="1 0 0 0 1 0 0 0 1 {ox} {oy} 0" printable="1"/>\n'
    cfg+=f' <object id="{oid}">\n  <metadata key="name" value="{gname}"/>\n  <metadata key="extruder" value="{parts[0][1]}"/>\n{cfgp} </object>\n'
    plate+=f'  <model_instance><metadata key="object_id" value="{oid}"/><metadata key="instance_id" value="0"/></model_instance>\n'
    oid+=1
cfg+=' <plate>\n  <metadata key="plater_id" value="1"/>\n  <metadata key="plater_name" value="Thermostat case PETG"/>\n'+plate+' </plate>\n</config>\n'
model=f'''<?xml version="1.0" encoding="UTF-8"?>
<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02" xmlns:BambuStudio="http://schemas.bambulab.com/package/2021">
<metadata name="Application">BambuStudio-02.00.00.00</metadata>
<metadata name="BambuStudio:3mfVersion">1</metadata>
<metadata name="Title">Connie's Thermostat V3 case</metadata>
<resources>
{objs}</resources>
<build>
{items}</build>
</model>'''
proj={"filament_colour":["#FFFFFF","#000000","#FF7A00","#FF9ECF"],"filament_type":["PETG","PETG","PETG","PETG"],
      "filament_settings_id":["Generic PETG","Generic PETG","Generic PETG","Generic PETG"],"enable_support":"0","printer_model":"Bambu Lab P2S"}
ct='''<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/><Default Extension="config" ContentType="text/xml"/></Types>'''
rels='''<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Target="/3D/3dmodel.model" Id="rel-1" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>'''
with zipfile.ZipFile('thermostat-case-P2S.3mf','w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('[Content_Types].xml',ct); z.writestr('_rels/.rels',rels); z.writestr('3D/3dmodel.model',model)
    z.writestr('Metadata/model_settings.config',cfg); z.writestr('Metadata/project_settings.config',json.dumps(proj,indent=1))
print('ok',oid-1,'objects')
