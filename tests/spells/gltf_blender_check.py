"""Gate (d): open our .gltf in Blender's OWN glTF importer, headless.

This is the only gate in the set that is not our code judging our code: the
importer is Khronos's reference Python implementation, shipped with Blender,
and it refuses a file the spec forbids. It runs with no window
(`--background`) and with `--factory-startup`, so the user's add-ons and
preferences cannot change the answer.

Run (Blender is at E:\\Tools\\3D\\Blender 4.5 on this machine):

  "E:/Tools/3D/Blender 4.5/blender.exe" --background --factory-startup \\
      --python tests/spells/gltf_blender_check.py -- FILE.gltf \\
      [--expect-bones N] [--expect-frames A B]

Every mesh in the FILE must come back with the file's own vertex and triangle
counts, matched by name -- the expectation is read out of the .gltf, not typed
in. It prints, one per line, `BLENDER <key> <value>` for: the objects created,
the armature and its bone count, every mesh and its vertex / triangle count
beside the file's, every action with its frame range and channel count, and the
images the
materials ask for (which for an FO4 export are .dds and will NOT resolve --
that is a stated loss, not a failure). Any --expect that does not match is a
FAIL line and the exit code is 1.
"""
import json
import sys

import bpy

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if not argv:
    print("FAIL: no .gltf given after --")
    sys.exit(2)
path = argv[0]


def opt(name, n=1):
    if name not in argv:
        return None
    i = argv.index(name)
    return argv[i + 1:i + 1 + n]


fails = []

bpy.ops.wm.read_factory_settings(use_empty=True)
before = set(bpy.data.meshes.keys())
try:
    bpy.ops.import_scene.gltf(filepath=path)
except Exception as e:                                  # noqa: BLE001
    print("FAIL: Blender's glTF importer refused %s: %s" % (path, e))
    sys.exit(1)

print("BLENDER file %s" % path)
print("BLENDER objects %d" % len(bpy.data.objects))

bones = 0
for ob in bpy.data.objects:
    if ob.type == "ARMATURE":
        bones += len(ob.data.bones)
        print("BLENDER armature %s bones %d" % (ob.name, len(ob.data.bones)))

# What the FILE says, read straight out of its own JSON: mesh name -> the
# POSITION accessor's count and a third of the index accessor's. Blender's
# importer must reproduce every one of them, by name. (Comparing a grand
# total instead would be wrong: the importer adds a mesh of its own -- an
# "Icosphere" it uses as a bone display shape -- which is its artefact and
# not something the export asked for.)
want = {}
with open(path, "r", encoding="utf-8") as fh:
    doc = json.load(fh)
for m in doc.get("meshes", []):
    prim = m["primitives"][0]
    want[m["name"]] = (doc["accessors"][prim["attributes"]["POSITION"]]["count"],
                       doc["accessors"][prim["indices"]]["count"] // 3)

verts = tris = 0
seen = set()
for me in bpy.data.meshes:
    if me.name in before or me.users == 0:
        continue                                    # a leftover of the startup file
    me.calc_loop_triangles()
    nv, nt = len(me.vertices), len(me.loop_triangles)
    if me.name in want:
        seen.add(me.name)
        verts += nv
        tris += nt
        if (nv, nt) != want[me.name]:
            fails.append("Blender's %s has %d verts / %d tris, the file says %d / %d"
                         % (me.name, nv, nt, want[me.name][0], want[me.name][1]))
        print("BLENDER mesh %s verts %d tris %d (file %d / %d)"
              % (me.name, nv, nt, want[me.name][0], want[me.name][1]))
    else:
        print("BLENDER extra-mesh %s verts %d tris %d  (Blender's own, not in the file)"
              % (me.name, nv, nt))
for nm in sorted(set(want) - seen):
    fails.append("Blender created no mesh called %r; the file has one" % nm)
print("BLENDER verts_total %d" % verts)
print("BLENDER tris_total %d" % tris)
print("BLENDER meshes_matched %d of %d" % (len(seen), len(want)))

def fcurve_count(ac):
    """Blender 4.4+ moved fcurves into action layers/slots; `ac.fcurves` then
    reports only a fraction. Count both ways and take the larger."""
    n = len(ac.fcurves)
    for layer in getattr(ac, "layers", []):
        for strip in getattr(layer, "strips", []):
            for bag in getattr(strip, "channelbags", []):
                n = max(n, len(bag.fcurves)) if not n else n + len(bag.fcurves)
    return n


for ac in bpy.data.actions:
    fr = ac.frame_range
    print("BLENDER action %s frames %.3f..%.3f channels %d"
          % (ac.name, fr[0], fr[1], fcurve_count(ac)))

for im in bpy.data.images:
    print("BLENDER image %s size %dx%d packed %s"
          % (im.name, im.size[0], im.size[1], bool(im.packed_file)))

eb = opt("--expect-bones")
if eb and bones != int(eb[0]):
    fails.append("Blender counts %d armature bones, the export says %s" % (bones, eb[0]))
ef = opt("--expect-frames", 2)
if ef:
    got = [(ac.frame_range[0], ac.frame_range[1]) for ac in bpy.data.actions]
    want = (float(ef[0]), float(ef[1]))
    if not got:
        fails.append("Blender created no action; the export carries an animation")
    elif not any(abs(a - want[0]) < 0.5 and abs(b - want[1]) < 0.5 for a, b in got):
        fails.append("no action has the frame range %s..%s; Blender has %s" % (ef[0], ef[1], got))

for f in fails:
    print("FAIL: " + f)
print("BLENDER result %s" % ("FAIL" if fails else "PASS"))
sys.exit(1 if fails else 0)
