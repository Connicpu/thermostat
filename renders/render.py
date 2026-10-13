import bpy, json, math, sys, mathutils, os
from mathutils import Vector
argv = sys.argv[sys.argv.index('--') + 1:]
VIEW = argv[0]
OUT = argv[1]
B = '/workspace/thermostat/v3/renders/build/'
_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EPD_IMG = os.path.join(_REPO, 'firmware', 'docs', 'display_preview.png')
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
W, H = 147, 71
OX, OYT = 3.5, 3.5
Zpb = 17.0
PT = 1.6  # rev10: 3 plate + 14 standoff
# Explosion offsets: zero for assembled / closed views
if VIEW in ('assembled', 'front'):
    E_PCB = E_EPD = E_SH = E_CAP = 0
else:
    E_PCB, E_EPD, E_SH, E_CAP = 42, 80, 128, 112


def mat(name, col, rough=0.5, metal=0.0, trans=0.0, emit=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes['Principled BSDF']
    p.inputs['Base Color'].default_value = (*col, 1)
    p.inputs['Roughness'].default_value = rough
    p.inputs['Metallic'].default_value = metal
    if trans:
        p.inputs['Transmission Weight'].default_value = trans
    if emit:
        p.inputs['Emission Color'].default_value = (*col, 1)
        p.inputs['Emission Strength'].default_value = emit
    return m


# Pink is bumped + slight emission so AgX doesn't crush saturated PLA to near-black
M = dict(
    white=mat('pla_white', (0.85, 0.85, 0.83), 0.45),
    pink=mat('pink', (0.98, 0.18, 0.48), 0.36, emit=0.22),
    black=mat('blk', (0.02, 0.02, 0.02), 0.5),
    orange=mat('orng', (1.0, 0.45, 0.05), 0.40, emit=0.18),
    plate=mat('plate', (0.78, 0.78, 0.80), 0.5),
    mask=mat('mask', (0.92, 0.92, 0.9), 0.35),
    chip=mat('chip', (0.04, 0.04, 0.045), 0.4),
    metal=mat('metal', (0.8, 0.8, 0.82), 0.25, 1.0),
    green=mat('termgreen', (0.05, 0.45, 0.15), 0.5),
    lime=mat('plug', (0.15, 0.65, 0.2), 0.5),
    cap=mat('ecap', (0.05, 0.12, 0.45), 0.35),
    ceram=mat('ceram', (0.55, 0.42, 0.28), 0.5),
    epd=mat('epd', (0.9, 0.9, 0.88), 0.6),
    aa=mat('aa', (0.6, 0.6, 0.58), 0.8),
    wall=mat('drywall', (0.62, 0.6, 0.56), 0.9),
    copper=mat('cu', (0.95, 0.55, 0.3), 0.3, 1.0),
    silk=mat('silk', (0.05, 0.05, 0.05), 0.6),
)
WC = {
    'R': (0.75, 0.05, 0.04),
    'C': (0.08, 0.25, 0.85),
    'W': (0.95, 0.95, 0.95),
    'Y': (0.98, 0.8, 0.05),
    'G': (0.05, 0.6, 0.15),
}


def stl(name, m, dz=0):
    bpy.ops.wm.stl_import(filepath=B + name + '.stl')
    o = bpy.context.selected_objects[0]
    o.location.z += dz
    o.data.materials.clear()
    o.data.materials.append(m)
    for p in o.data.polygons:
        p.use_smooth = False
    return o


def box(x0, y0, z0, x1, y1, z1, m, name='box'):
    bpy.ops.mesh.primitive_cube_add(
        size=1, location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    )
    o = bpy.context.object
    o.scale = (abs(x1 - x0), abs(y1 - y0), abs(z1 - z0))
    o.name = name
    o.data.materials.append(m)
    bev = o.modifiers.new('b', 'BEVEL')
    bev.width = min(0.25, abs(z1 - z0) / 4)
    bev.segments = 2
    return o


def cyl(x, y, z0, z1, d, m):
    bpy.ops.mesh.primitive_cylinder_add(
        radius=d / 2, depth=z1 - z0, location=(x, y, (z0 + z1) / 2), vertices=48
    )
    o = bpy.context.object
    o.data.materials.append(m)
    bpy.ops.object.shade_smooth()
    return o


cx = lambda x: OX + x
cy = lambda y: H - OYT - y

# ---- case parts
stl('backplate', M['plate'])
for n, m in [
    ('shell', 'white'),
    ('inlay_pink', 'pink'),
    ('inlay_black', 'black'),
    ('inlay_orange', 'orange'),
    ('light_pipe', 'pink'),
]:
    stl(n, M[m], E_SH)
btn = [(112, 30), (112, 40), (112, 50)]
for bx, by in btn:
    o = stl('cap', M['orange'], 0)
    o.location = (cx(bx), cy(by), Zpb + PT + 1.5 + E_CAP)

# ---- PCB from KiCad GLB
OY = OYT  # board origin Y offset in case coords (same as original OX,OY=3.5,3.5)
pz = Zpb + E_PCB
bpy.ops.import_scene.gltf(filepath=B + 'pcb.glb')
have = set()
for o in sc.objects:
    if o.type == 'MESH' and o.data.materials and o.data.materials[0] and o.data.materials[0].name.startswith('mat_'):
        have.add(o.name.split('.')[0])
        for i, ms in enumerate(o.data.materials):
            c = tuple(ms.diffuse_color)
            if abs(c[0] - 0.08) < 0.02 and abs(c[1] - 0.2) < 0.02:
                o.data.materials[i] = M['mask']
            elif c[0] > 0.99 and c[1] > 0.99 and c[3] < 0.95:
                o.data.materials[i] = M['silk']
            elif abs(c[0] - 0.3) < 0.02 and abs(c[1] - 0.3) < 0.02:
                o.data.materials[i] = M['mask']
# move GLB roots: meters->mm and to case position
for o in sc.objects:
    if o.parent is None and (o.name.startswith('=>') or o.name in have or o.type == 'EMPTY'):
        if o.name.startswith(('backplate', 'shell', 'inlay', 'light', 'cap')):
            continue
        o.scale = (1000, 1000, 1000)
        o.location = (
            OX + o.location.x * 1000,
            OY + o.location.y * 1000,
            pz + o.location.z * 1000,
        )

P = json.load(open(B + 'parts.json'))


def hgt(p):
    f = p['fp']
    r = p['ref']
    if r == 'U3':
        return 3.1
    if r.startswith('Q'):
        return 2.3
    if r.startswith('U') and r in ('U6', 'U7', 'U8'):
        return 3.6
    if r == 'L1':
        return 4.5
    if r == 'L2':
        return 1.5
    if r == 'J2':
        return 3.2
    if r == 'J4':
        return 2.0
    if '1210' in f or '1812' in f:
        return 1.6
    if 'SMA' in f or 'SMB' in f or 'MBS' in f:
        return 2.2
    if '0805' in f:
        return 0.9
    if '1206' in f:
        return 0.6
    if '0603' in f:
        return 0.8
    return 0.5


def colr(p):
    r = p['ref']
    f = p['fp']
    if r == 'U3':
        return M['metal']
    if r == 'J2':
        return M['metal']
    if r.startswith('C') and ('0402' in f or '0603' in f or '0805' in f or '1210' in f):
        return M['ceram']
    if r == 'L1' or r == 'L2':
        return M['chip']
    return M['chip']


for p in P:
    r = p['ref']
    if r in have or r in ('J1', 'C43') or p['dnp']:
        continue
    sh = 0.25
    x0, x1 = cx(p['x0']) + sh, cx(p['x1']) - sh
    y0, y1 = cy(p['y1']) + sh, cy(p['y0']) - sh
    if r == 'U3':
        box(x0, y0, pz + PT, x1, y1, pz + PT + 0.8, M['chip'], 'U3pcb')
        box(x0 + 0.8, y0 + 0.8, pz + PT + 0.8, x1 - 0.8, y1 - 6.5, pz + PT + 3.1, M['metal'], 'U3can')
        continue
    if r == 'U5':
        box(x0, y0, pz + PT, x1, y1, pz + PT + 0.5, M['chip'])
        continue
    h = hgt(p)
    box(x0, y0, pz + PT, x1, y1, pz + PT + h, colr(p), r)

# rear: J1 right-angle header + 15EDGK plug, C43
tx, ty = 70, 58
box(cx(tx) - 9.2, cy(ty) - 1.2, pz - 9.5, cx(tx) + 9.2, cy(ty) + 8, pz, M['green'], 'J1hdr')
box(cx(tx) - 9.6, cy(ty) + 8, pz - 10.5, cx(tx) + 9.6, cy(ty) + 19.5, pz - 0.5, M['lime'], 'J1plug')
for i in range(5):
    cyl(cx(tx) - 7 + i * 3.5, cy(ty) + 13.5, pz - 11.0, pz - 10.4, 2.6, M['metal'])
c43 = [q for q in P if q['ref'] == 'C43'][0]
ccx = cx((c43['x0'] + c43['x1']) / 2)
ccy = cy((c43['y0'] + c43['y1']) / 2)
cyl(ccx, ccy, pz - 9.5, pz - 0.5, 8.0, M['cap'])
cyl(ccx, ccy, pz - 9.6, pz - 9.4, 7.0, M['metal'])
box(ccx - 4.1, ccy - 1.2, pz - 9.0, ccx - 3.6, ccy + 1.2, pz - 1.0, M['white'], 'C43stripe')

# ---- EPD panel
panel = (29.46, 26.24, 108.46, 62.94)
aa = (32.46, 30.06, 99.36, 59.12)
ezb = Zpb + PT + 5.0 + (E_EPD - E_PCB) + E_PCB
box(cx(panel[0]), cy(panel[3]), ezb, cx(panel[2]), cy(panel[1]), ezb + 1.2, M['epd'], 'EPD')
# Active-area screen plane with firmware preview image (assembled / closed views)
sx0, sx1 = cx(aa[0]), cx(aa[2])
sy0, sy1 = cy(aa[3]), cy(aa[1])  # cy flips board-y
sz = ezb + 1.28
bpy.ops.mesh.primitive_plane_add(
    size=1, location=((sx0 + sx1) / 2, (sy0 + sy1) / 2, sz)
)
screen = bpy.context.object
screen.name = 'EPDscreen'
screen.scale = (abs(sx1 - sx0) / 2, abs(sy1 - sy0) / 2, 1)  # plane is 2x2 by default at size=1? size=1 => 1x1
# Blender plane size=1 is 2x2 units from -1..1; use scale half-extents:
screen.scale = (abs(sx1 - sx0) / 2, abs(sy1 - sy0) / 2, 1)

# Build e-paper image material
em = bpy.data.materials.new('epd_img')
em.use_nodes = True
nt = em.node_tree
for n in list(nt.nodes):
    nt.nodes.remove(n)
out_n = nt.nodes.new('ShaderNodeOutputMaterial')
bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
tex = nt.nodes.new('ShaderNodeTexImage')
if os.path.isfile(EPD_IMG):
    tex.image = bpy.data.images.load(EPD_IMG)
    tex.image.colorspace_settings.name = 'sRGB'
else:
    tex.image = None
bsdf.inputs['Roughness'].default_value = 0.62
if 'Specular IOR Level' in bsdf.inputs:
    bsdf.inputs['Specular IOR Level'].default_value = 0.12
# UV: default plane UVs; flip U so board-x (left=temp) matches image left after cy/cx placement
mapn = nt.nodes.new('ShaderNodeMapping')
texcoord = nt.nodes.new('ShaderNodeTexCoord')
# Empirically: plane local +X is case +X (image should read L→R), V already matches case +Y→image top
# If a prior flip left text upside-down, leave Scale at 1; mirror fix only if needed:
mapn.inputs['Scale'].default_value[0] = 1.0
mapn.inputs['Scale'].default_value[1] = 1.0
nt.links.new(texcoord.outputs['UV'], mapn.inputs['Vector'])
nt.links.new(mapn.outputs['Vector'], tex.inputs['Vector'])
if tex.image:
    # Slight warm paper lift: mostly texture, tiny cream blend
    mix = nt.nodes.new('ShaderNodeMix')
    mix.data_type = 'RGBA'
    mix.inputs[0].default_value = 0.10  # Factor
    # A = texture (dominant), B = cream paper
    nt.links.new(tex.outputs['Color'], mix.inputs[6])  # A (Color)
    mix.inputs[7].default_value = (0.93, 0.93, 0.90, 1)  # B (Color)
    nt.links.new(mix.outputs[2], bsdf.inputs['Base Color'])  # Result
else:
    bsdf.inputs['Base Color'].default_value = (0.85, 0.85, 0.82, 1)
nt.links.new(bsdf.outputs['BSDF'], out_n.inputs['Surface'])
screen.data.materials.append(em)
# FPC ribbon folded under (flat amber strip)
fpc = mat('fpc', (0.85, 0.5, 0.1), 0.4)
box(cx(108.46), cy(52), ezb - 0.1, cx(112), cy(36), ezb + 0.1, fpc, 'fpc')

# ---- drywall slab with hole (behind backplate)
dw = box(-25, -30, -12.7, W + 25, H + 30, -0.2, M['wall'], 'drywall')
dw.modifiers.clear()
hole = box(W / 2 - 10, H / 2 - 7, -20, W / 2 + 10, H / 2 + 7, 5, M['wall'], 'hole')
hole.modifiers.clear()
bo = dw.modifiers.new('h', 'BOOLEAN')
bo.object = hole
bo.operation = 'DIFFERENCE'
hole.hide_render = True
hole.hide_viewport = True

# ---- 5 thermostat wires
entry_y = cy(ty) + 19.5
zw = pz - 6.0
for i, n in enumerate('RCWYG'):
    ex = cx(tx) - 7 + i * 3.5
    sx = W / 2 - 5 + i * 2.5
    sy = H / 2 + 1.5
    pts = [
        (sx, sy, -60),
        (sx, sy, -14),
        (sx, sy, zw - 14),
        (sx + (ex - sx) * 0.5, sy - 0.5, zw - 2),
        (ex, entry_y + 7, zw),
        (ex, entry_y + 1.5, zw),
    ]
    cd = bpy.data.curves.new('w' + n, 'CURVE')
    cd.dimensions = '3D'
    cd.bevel_depth = 0.9
    cd.bevel_resolution = 6
    sp = cd.splines.new('BEZIER')
    sp.bezier_points.add(len(pts) - 1)
    for bp, pt in zip(sp.bezier_points, pts):
        bp.co = pt
        bp.handle_left_type = bp.handle_right_type = 'AUTO'
    ob = bpy.data.objects.new('wire' + n, cd)
    sc.collection.objects.link(ob)
    ob.data.materials.append(mat('ins' + n, WC[n], 0.45))
    bpy.ops.mesh.primitive_cylinder_add(
        radius=0.5, depth=6, location=(ex, entry_y - 2.5, zw), rotation=(math.pi / 2, 0, 0)
    )
    bpy.context.object.data.materials.append(M['copper'])

cd = bpy.data.curves.new('jacket', 'CURVE')
cd.dimensions = '3D'
cd.bevel_depth = 3.6
cd.bevel_resolution = 8
sp = cd.splines.new('POLY')
sp.points.add(1)
sp.points[0].co = (W / 2, H / 2 + 1.5, -80, 1)
sp.points[1].co = (W / 2, H / 2 + 1.5, -16, 1)
ob = bpy.data.objects.new('jacket', cd)
sc.collection.objects.link(ob)
ob.data.materials.append(mat('jk', (0.7, 0.68, 0.6), 0.7))

# ---- lighting / world / camera
world = bpy.data.worlds.new('w')
sc.world = world
world.use_nodes = True
bg = world.node_tree.nodes['Background']
bg.inputs[0].default_value = (0.94, 0.95, 0.97, 1)
bg.inputs[1].default_value = 0.45


def light(loc, energy, size=150):
    ld = bpy.data.lights.new('l', 'AREA')
    ld.energy = energy
    ld.size = size
    o = bpy.data.objects.new('l', ld)
    sc.collection.objects.link(o)
    o.location = loc
    d = Vector((73, 35, 50)) - Vector(loc)
    o.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()


center = Vector((73.5, 35.5, 13))
if VIEW == '34':
    camloc = Vector((330, -300, 330))
    light((250, -200, 400), 4e6)
    light((-200, -100, 150), 1.5e6)
    light((100, 250, 250), 1e6)
    lens = 50
    center = Vector((73, 35, 70))
elif VIEW == 'side':
    camloc = Vector((480, -95, 48))
    light((350, -250, 250), 4e6)
    light((300, 100, 30), 2e6)
    light((-100, -200, 100), 1e6)
    lens = 46
    center = Vector((73, 35, 68))
elif VIEW in ('assembled', 'front'):
    # 3/4 front: mostly face-on, slight side + above to read the 26.8 mm depth
    camloc = Vector((195, -140, 155))
    light((160, -180, 280), 3.2e6, size=180)
    light((-60, -30, 140), 1.1e6, size=140)
    light((90, 180, 200), 0.85e6, size=160)
    light((73, 35, 350), 0.7e6, size=220)  # soft fill from front
    lens = 58
    center = Vector((73.5, 35.5, 13))
else:
    camloc = Vector((-120, -330, -40))
    light((0, -300, -100), 8e6)
    light((300, -200, 200), 4e6)
    lens = 60
    center = Vector((73, 35, 40))

cam = bpy.data.cameras.new('c')
cam.lens = lens
cam.clip_end = 5000
co = bpy.data.objects.new('cam', cam)
sc.collection.objects.link(co)
co.location = camloc
co.rotation_euler = (center - camloc).to_track_quat('-Z', 'Y').to_euler()
sc.camera = co

sc.render.engine = 'CYCLES'
sc.cycles.samples = int(argv[2]) if len(argv) > 2 else 96
sc.cycles.use_denoising = False
sc.cycles.device = 'CPU'
sc.render.resolution_x = 1600
sc.render.resolution_y = 1100
sc.render.film_transparent = False
sc.view_settings.view_transform = 'AgX'
sc.view_settings.look = 'AgX - Medium High Contrast'
sc.view_settings.exposure = -0.2 if VIEW in ('assembled', 'front') else -0.35
sc.render.filepath = OUT
bpy.ops.render.render(write_still=True)
print('WROTE', OUT)
