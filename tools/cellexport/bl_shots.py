# Headless Blender: several models x several cameras from one spec file (tools/cellexport, lane CELL1).
# blender -b --factory-startup -P bl_shots.py -- <spec.json>
# spec: {"W":1280,"H":720,"out_dir":"...","shade":"texture"|"clay"|"mask",
#        "models":[{"label":"full","files":["a.glb"],"drop":["Ground"]}],
#        "cams":[{"name":"oblique","spec":"persp:ex,ey,ez:tx,ty,tz:fov" | "ortho:...:width"}]}
# Camera positions are GAME units relative to each file's export origin (z absolute in that frame).
# Output: <out_dir>/<label>__<cam>.png ; prints TRIS <label> <n> per model.
import bpy, sys, json, math, mathutils, os, time

spec = json.load(open(sys.argv[sys.argv.index('--') + 1]))
MPU = 0.9144 / 64.0
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
W, H = spec['W'], spec['H']
shade = spec.get('shade', 'texture')
os.makedirs(spec['out_dir'], exist_ok=True)

cols = {}
for m in spec['models']:
    t0 = time.time()
    col = bpy.data.collections.new(m['label'])
    sc.collection.children.link(col)
    before = set(bpy.data.objects)
    for f in m['files']:
        bpy.ops.import_scene.gltf(filepath=f, merge_vertices=False, import_shading='NORMALS')
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        for c in list(o.users_collection):
            c.objects.unlink(o)
        col.objects.link(o)
    for o in list(new):
        if any(o.name.startswith(d) for d in m.get('drop', [])):
            bpy.data.objects.remove(o, do_unlink=True)
    tris = 0
    for o in col.objects:
        if o.type == 'MESH':
            o.data.calc_loop_triangles()
            tris += len(o.data.loop_triangles)
    print('TRIS %s %d (import %.1fs)' % (m['label'], tris, time.time() - t0))
    cols[m['label']] = col

cd = bpy.data.cameras.new('cam')
co = bpy.data.objects.new('cam', cd)
sc.collection.objects.link(co)
sc.camera = co
cd.clip_start = 1.0
cd.clip_end = 20000.0
sc.render.resolution_x, sc.render.resolution_y = W, H
sc.render.resolution_percentage = 100
sc.render.engine = 'BLENDER_EEVEE_NEXT'
try:
    sc.eevee.taa_render_samples = 16 if shade != 'mask' else 1
except Exception:
    pass
sc.view_settings.view_transform = 'Standard'

w = bpy.data.worlds.new('w')
w.use_nodes = True
bg = next(n for n in w.node_tree.nodes if n.type == 'BACKGROUND')
sc.world = w
if shade == 'mask':
    bg.inputs[1].default_value = 0.0
    sc.render.film_transparent = True
    mat = bpy.data.materials.new('mask')
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    em = nt.nodes.new('ShaderNodeEmission')
    em.inputs[0].default_value = (1, 1, 1, 1)
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    nt.links.new(em.outputs[0], out.inputs[0])
    sc.view_layers[0].material_override = mat
    sc.render.image_settings.color_mode = 'RGBA'
else:
    bg.inputs[0].default_value = (0.55, 0.65, 0.78, 1.0)
    bg.inputs[1].default_value = 0.9
    sun = bpy.data.lights.new('sun', 'SUN')
    sun.energy = 4.0
    sun.angle = math.radians(2.0)
    so = bpy.data.objects.new('sun', sun)
    sc.collection.objects.link(so)
    so.rotation_euler = mathutils.Vector((0.45, 0.55, -0.70)).to_track_quat('-Z', 'Y').to_euler()
    if shade == 'clay':
        mat = bpy.data.materials.new('clay')
        mat.use_nodes = True
        bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        bsdf.inputs[0].default_value = (0.30, 0.29, 0.27, 1)
        sun.energy = 2.5
        bsdf.inputs['Roughness'].default_value = 0.9
        sc.view_layers[0].material_override = mat

if shade != 'mask' and spec.get('context'):
    ctx = bpy.data.collections.new('_context')
    sc.collection.children.link(ctx)
    before = set(bpy.data.objects)
    for f in spec['context']:
        bpy.ops.import_scene.gltf(filepath=f, merge_vertices=False, import_shading='NORMALS')
    for o in [o for o in bpy.data.objects if o not in before]:
        for c in list(o.users_collection):
            c.objects.unlink(o)
        ctx.objects.link(o)
if shade in ('clay', 'mask'):
    # the view-layer override is not honoured here (measured: clay renders came out textured),
    # so every slot of every model mesh gets the override material directly
    for col in cols.values():
        for o in col.objects:
            if o.type == 'MESH':
                if not o.data.materials:
                    o.data.materials.append(mat)
                for i in range(len(o.data.materials)):
                    o.data.materials[i] = mat
                for s in o.material_slots:
                    s.link = 'DATA'

for m in spec['models']:
    for lab, col in cols.items():
        col.hide_render = lab != m['label']
    for c in spec['cams']:
        kind, e, t, p = c['spec'].split(':')
        e = mathutils.Vector([float(v) * MPU for v in e.split(',')])
        t = mathutils.Vector([float(v) * MPU for v in t.split(',')])
        # game (x,y,z) -> Blender after the glTF importer: (x, y, z) with Z up (the importer converts Y-up back)
        co.location = e
        co.rotation_euler = (t - e).to_track_quat('-Z', 'Y').to_euler()
        if kind == 'ortho':
            cd.type = 'ORTHO'
            cd.ortho_scale = float(p) * MPU
        else:
            cd.type = 'PERSP'
            cd.sensor_fit = 'HORIZONTAL'
            cd.angle = math.radians(float(p))
        sc.render.filepath = os.path.join(spec['out_dir'], '%s__%s.png' % (m['label'], c['name']))
        t0 = time.time()
        bpy.ops.render.render(write_still=True)
        print('RENDERED %s %.1fs' % (sc.render.filepath, time.time() - t0))
