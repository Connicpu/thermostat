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
Zpb = 17.0  # backplate 3 mm + standoff 14 mm
PT = 1.6  # PCB thickness
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


# Pink and orange PLA carry a little emission so the color stays saturated. Stronger emission flattens the shading.
M = dict(
    white=mat('pla_white', (0.68, 0.68, 0.66), 0.55),
    pink=mat('pink', (0.98, 0.05, 0.30), 0.32, emit=0.12),
    black=mat('blk', (0.02, 0.02, 0.02), 0.5),
    orange=mat('orng', (1.0, 0.32, 0.02), 0.35, emit=0.10),
    plate=mat('plate', (0.72, 0.72, 0.74), 0.5),
    mask=mat('mask', (0.90, 0.90, 0.88), 0.35),
    chip=mat('chip', (0.04, 0.04, 0.045), 0.4),
    metal=mat('metal', (0.8, 0.8, 0.82), 0.25, 1.0),
    green=mat('termgreen', (0.05, 0.45, 0.15), 0.5),
    lime=mat('plug', (0.15, 0.65, 0.2), 0.5),
    cap=mat('ecap', (0.05, 0.12, 0.45), 0.35),
    ceram=mat('ceram', (0.55, 0.42, 0.28), 0.5),
    epd=mat('epd', (0.88, 0.88, 0.85), 0.6),
    aa=mat('aa', (0.55, 0.55, 0.52), 0.8),
    wall=mat('drywall', (0.16, 0.15, 0.14), 0.95),  # clearly darker than the shell
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


def stl(name, m, dz=0, z_push=0.0, fix_normals=False):
    bpy.ops.wm.stl_import(filepath=B + name + '.stl')
    o = bpy.context.selected_objects[0]
    o.location.z += dz + z_push
    o.data.materials.clear()
    o.data.materials.append(m)
    for poly in o.data.polygons:
        poly.use_smooth = False
    if fix_normals:
        # OpenSCAD flush inlays often arrive with flipped normals → render black under lit BSDF
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.normals_make_consistent(inside=False)
        bpy.ops.object.mode_set(mode='OBJECT')
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
stl('shell', M['white'], E_SH)
# Face inlays: +0.12 mm toward camera avoids z-fight with the shell face; fix normals
for n, mk in [('inlay_pink', 'pink'), ('inlay_black', 'black'), ('inlay_orange', 'orange')]:
    stl(n, M[mk], E_SH, z_push=0.12, fix_normals=True)
stl('light_pipe', M['pink'], E_SH, fix_normals=True)
btn = [(112, 30), (112, 40), (112, 50)]
for bx, by in btn:
    o = stl('cap', M['orange'], 0)
    o.location = (cx(bx), cy(by), Zpb + PT + 1.5 + E_CAP)

# ---- PCB from KiCad GLB
OY = OYT  # board origin Y offset in case coords (OX = OYT = 3.5)
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

# ---- EPD panel (GDEY029T94: outline 79.0x36.7, AA 66.896x29.056 per datasheet)
panel = (29.46, 26.24, 108.46, 62.94)
aa = (32.46, 30.06, 99.36, 59.12)  # board coords; AA 66.9 x 29.06 mm
ezb = Zpb + PT + 5.0 + (E_EPD - E_PCB) + E_PCB
box(cx(panel[0]), cy(panel[3]), ezb, cx(panel[2]), cy(panel[1]), ezb + 1.2, M['epd'], 'EPD')
# Active-area screen: firmware preview is full-bleed 296x128 (docs PNG is 2x)
# Blender 4.x plane size=N is NxN (NOT 2x2); scale must be the full AA extents.
sx0, sx1 = cx(aa[0]), cx(aa[2])
sy0, sy1 = cy(aa[3]), cy(aa[1])  # cy flips board-y
aa_w, aa_h = abs(sx1 - sx0), abs(sy1 - sy0)  # ~66.9 x 29.06
sz = ezb + 1.28  # just above panel front (panel_t=1.2)
bpy.ops.mesh.primitive_plane_add(
    size=1, location=((sx0 + sx1) / 2, (sy0 + sy1) / 2, sz)
)
screen = bpy.context.object
screen.name = 'EPDscreen'
screen.scale = (aa_w, aa_h, 1)
# Bake non-uniform scale into the mesh so UVs span the real AA (avoids 4.x size=1 half-scale bug class)
bpy.context.view_layer.objects.active = screen
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
print(f'EPD AA plane mm: {screen.dimensions.x:.2f} x {screen.dimensions.y:.2f} (expect ~66.90 x 29.06)')

# E-paper: sRGB preview as emission (crisp, lighting-independent) + slight diffuse paper
em = bpy.data.materials.new('epd_img')
em.use_nodes = True
nt = em.node_tree
for n in list(nt.nodes):
    nt.nodes.remove(n)
out_n = nt.nodes.new('ShaderNodeOutputMaterial')
bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
emit_n = nt.nodes.new('ShaderNodeEmission')
mix_sh = nt.nodes.new('ShaderNodeMixShader')
tex = nt.nodes.new('ShaderNodeTexImage')
if os.path.isfile(EPD_IMG):
    tex.image = bpy.data.images.load(EPD_IMG)
    tex.image.colorspace_settings.name = 'sRGB'
    tex.extension = 'CLIP'
else:
    tex.image = None
bsdf.inputs['Roughness'].default_value = 0.7
if 'Specular IOR Level' in bsdf.inputs:
    bsdf.inputs['Specular IOR Level'].default_value = 0.05
bsdf.inputs['Base Color'].default_value = (0.90, 0.90, 0.87, 1)
mapn = nt.nodes.new('ShaderNodeMapping')
texcoord = nt.nodes.new('ShaderNodeTexCoord')
mapn.inputs['Scale'].default_value[0] = 1.0
mapn.inputs['Scale'].default_value[1] = 1.0
nt.links.new(texcoord.outputs['UV'], mapn.inputs['Vector'])
nt.links.new(mapn.outputs['Vector'], tex.inputs['Vector'])
if tex.image:
    # Keep e-ink blacks black and paper off-white (not HDR) under Standard
    # Gamma >1 darkens midtones → crisper e-ink text; then scale to paper white
    gam = nt.nodes.new('ShaderNodeGamma')
    gam.inputs['Gamma'].default_value = 1.8
    vm = nt.nodes.new('ShaderNodeVectorMath')
    vm.operation = 'MULTIPLY'
    vm.inputs[1].default_value = (0.78, 0.78, 0.74)
    nt.links.new(tex.outputs['Color'], gam.inputs['Color'])
    nt.links.new(gam.outputs['Color'], vm.inputs[0])
    nt.links.new(vm.outputs['Vector'], emit_n.inputs['Color'])
    emit_n.inputs['Strength'].default_value = 0.65
    mix_sh.inputs['Fac'].default_value = 0.88
    nt.links.new(bsdf.outputs['BSDF'], mix_sh.inputs[1])
    nt.links.new(emit_n.outputs['Emission'], mix_sh.inputs[2])
    nt.links.new(mix_sh.outputs['Shader'], out_n.inputs['Surface'])
else:
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
# Dim world (0.04), a strong key and a weak fill for contact shadows, a wall darker than the shell, and an emissive sRGB e-paper. A bright world (0.45) flattens the shading.
world = bpy.data.worlds.new('w')
sc.world = world
world.use_nodes = True
bg = world.node_tree.nodes['Background']
bg.inputs[0].default_value = (0.30, 0.32, 0.36, 1)  # cool dark-gray void
bg.inputs[1].default_value = 0.04  # barely any ambient fill


def light(loc, energy, size=150, aim=None):
    ld = bpy.data.lights.new('l', 'AREA')
    ld.energy = energy
    ld.size = size
    ld.spread = 90  # degrees; soft but directional
    o = bpy.data.objects.new('l', ld)
    sc.collection.objects.link(o)
    o.location = loc
    target = aim if aim is not None else Vector((73.5, 35.5, 20))
    o.rotation_euler = (target - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    return o


center = Vector((73.5, 35.5, 13))
if VIEW == '34':
    camloc = Vector((330, -300, 330))
    aim = Vector((73, 35, 70))
    light((280, -220, 420), 2.8e6, size=180, aim=aim)   # key
    light((-160, -80, 200), 0.30e6, size=280, aim=aim)  # fill
    light((40, 220, 180), 0.55e6, size=180, aim=aim)    # rim
    lens = 50
    center = aim
elif VIEW == 'side':
    camloc = Vector((480, -95, 48))
    aim = Vector((73, 35, 68))
    light((400, -220, 280), 2.6e6, size=180, aim=aim)
    light((200, 120, 60), 0.25e6, size=260, aim=aim)
    light((-80, -180, 120), 0.50e6, size=180, aim=aim)
    lens = 46
    center = aim
elif VIEW in ('assembled', 'front'):
    # 3/4 front: mostly face-on, slight side + above to read the 26.8 mm depth
    camloc = Vector((195, -140, 155))
    aim = Vector((73.5, 35.5, 13))
    # Key from ABOVE (+Y) and left so the contact shadow falls toward -Y (visible under the case)
    light((-30, 240, 240), 1.8e6, size=75, aim=aim)
    light((210, -50, 70), 0.08e6, size=320, aim=aim)   # very weak fill
    light((70, 50, 360), 0.20e6, size=140, aim=aim)    # rim
    sun = bpy.data.lights.new('sun', 'SUN')
    sun.energy = 0.55
    sun.angle = math.radians(3.5)
    sun.color = (1.0, 0.97, 0.92)
    so = bpy.data.objects.new('sun', sun)
    sc.collection.objects.link(so)
    so.rotation_euler = (math.radians(48), math.radians(0), math.radians(-40))
    lens = 58
    center = aim
else:
    camloc = Vector((-120, -330, -40))
    aim = Vector((73, 35, 40))
    light((0, -300, -100), 3e6, size=200, aim=aim)
    light((300, -200, 200), 0.6e6, size=220, aim=aim)
    lens = 60
    center = aim

cam = bpy.data.cameras.new('c')
cam.lens = lens
cam.clip_end = 5000
co = bpy.data.objects.new('cam', cam)
sc.collection.objects.link(co)
co.location = camloc
co.rotation_euler = (center - camloc).to_track_quat('-Z', 'Y').to_euler()
sc.camera = co

sc.render.engine = 'CYCLES'
sc.cycles.samples = int(argv[2]) if len(argv) > 2 else 128
sc.cycles.use_denoising = False
sc.cycles.device = 'CPU'
sc.cycles.use_fast_gi = False
sc.render.resolution_x = 1600
sc.render.resolution_y = 1100
sc.render.film_transparent = False
# Standard view, because AgX and Filmic crush saturated pink and orange PLA toward black.
# Exposure is negative so the white shell keeps its shading. Contrast comes from the lighting.
sc.view_settings.view_transform = 'Standard'
sc.view_settings.look = 'None'
sc.view_settings.exposure = -0.40
sc.view_settings.gamma = 0.95
sc.render.filepath = OUT
bpy.ops.render.render(write_still=True)
print('WROTE', OUT)
