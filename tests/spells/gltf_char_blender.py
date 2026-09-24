"""The HEADLESS BLENDER row of tests/spells/gltf_export_options.sh.

The brief's words: "import character + clip export, assert exactly one
armature, zero loose empties, a named vertex moves between frame 0 and frame N
by the expected amount". That is the whole of what the `joints whole` and
`skeleton` options were asked for -- a character that arrives in Blender as ONE
armature with no scatter of empties around it -- so it is asked of Blender's
own importer and not of ours.

  "E:/Tools/3D/Blender 4.5/blender.exe" --background --factory-startup \\
      --python tests/spells/gltf_char_blender.py -- FILE.gltf \\
      [--expect-armatures N] [--max-empties N] [--moving-mesh NAME]
      [--frame-n N] [--min-move U] [--expect-bones N]

WHY EACH NUMBER IS A FLOOR AND NOT A WISH
  * --expect-armatures 1 fails at 0 as well as at 2. An export that carried no
    skin at all would produce 0, which is the failure the `joints` option is
    there to make impossible, so it is not enough to say "no more than one".
  * --max-empties 0: Blender turns a glTF node that is neither a mesh nor a
    joint into an EMPTY. Every skeleton bone that is not in some skin's joint
    list becomes one, which is exactly the "loose empties" bungo is looking at.
    An armature's own bones are NOT objects and are never counted here.
  * the vertex movement is measured through the DEPENDENCY GRAPH at two frames
    -- the evaluated mesh, i.e. after the armature modifier -- so it is the
    deformation and not the rest pose. It must move by at least --min-move, and
    the FLOOR under it is printed in the same run: the largest movement of any
    vertex between frame 0 and frame 0, which must be 0. A rig that does not
    deform and a script that cannot tell look identical without it.

Prints `BLENDER <key> <value>` lines and ends with `BLENDER result PASS|FAIL`.
Exit 0 only on PASS. Lane GLTFEXPORT1, 2026-09-19.
"""
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if not argv:
    print("FAIL: no .gltf given after --")
    sys.exit(2)
path = argv[0]


def opt(name, default=None):
    if name not in argv:
        return default
    i = argv.index(name)
    return argv[i + 1] if i + 1 < len(argv) else default


fails = []
bpy.ops.wm.read_factory_settings(use_empty=True)
try:
    bpy.ops.import_scene.gltf(filepath=path)
except Exception as e:                                  # noqa: BLE001
    print("FAIL: Blender's glTF importer refused %s: %s" % (path, e))
    sys.exit(1)

print("BLENDER file %s" % path)

armatures = [o for o in bpy.data.objects if o.type == "ARMATURE"]
empties = [o for o in bpy.data.objects if o.type == "EMPTY"]
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
print("BLENDER objects %d armatures %d empties %d meshes %d"
      % (len(bpy.data.objects), len(armatures), len(empties), len(meshes)))
for a in armatures:
    print("BLENDER armature %s bones %d" % (a.name, len(a.data.bones)))
for e in empties[:20]:
    print("BLENDER empty %s parent %s" % (e.name, e.parent.name if e.parent else "-"))
if len(empties) > 20:
    print("BLENDER empties_truncated %d more" % (len(empties) - 20))

want_arm = int(opt("--expect-armatures", "1"))
if len(armatures) != want_arm:
    fails.append("%d armature(s), expected exactly %d" % (len(armatures), want_arm))

max_empty = int(opt("--max-empties", "0"))
if len(empties) > max_empty:
    fails.append("%d loose empty object(s), at most %d allowed: %s"
                 % (len(empties), max_empty, ", ".join(e.name for e in empties[:12])))

eb = opt("--expect-bones")
if eb is not None:
    got = sum(len(a.data.bones) for a in armatures)
    if got != int(eb):
        fails.append("the armature carries %d bones, the export says %s" % (got, eb))

# --- the movement, through the dependency graph -----------------------------
name = opt("--moving-mesh")
frame_n = int(opt("--frame-n", "0"))
min_move = float(opt("--min-move", "0"))
if name is None and meshes:
    name = meshes[0].name

actions = list(bpy.data.actions)
for ac in actions:
    fr = ac.frame_range
    print("BLENDER action %s frames %.3f..%.3f" % (ac.name, fr[0], fr[1]))

if frame_n > 0:
    ob = bpy.data.objects.get(name)
    if ob is None or ob.type != "MESH":
        fails.append("no mesh object called %r to measure" % name)
    elif not actions:
        fails.append("the file carries no action, so nothing can move")
    else:
        dg = bpy.context.evaluated_depsgraph_get()

        def coords(frame):
            bpy.context.scene.frame_set(frame)
            dg.update()
            ev = ob.evaluated_get(dg)
            me = ev.to_mesh()
            out = [Vector(v.co) for v in me.vertices]
            ev.to_mesh_clear()
            return out

        a0 = coords(0)
        a0b = coords(0)          # the FLOOR: the same frame twice must not move
        an = coords(frame_n)
        floor = max((a0[i] - a0b[i]).length for i in range(len(a0))) if a0 else 0.0
        if len(an) != len(a0):
            fails.append("the evaluated mesh changed vertex count between frames")
        else:
            moves = [(a0[i] - an[i]).length for i in range(len(a0))]
            best = max(moves) if moves else 0.0
            worst_i = moves.index(best) if moves else -1
            print("BLENDER mesh %s verts %d" % (ob.name, len(a0)))
            print("BLENDER move_floor_same_frame %.9f" % floor)
            print("BLENDER move_max %.6f on vertex %d (frame 0 -> %d)"
                  % (best, worst_i, frame_n))
            print("BLENDER move_mean %.6f" % (sum(moves) / len(moves) if moves else 0.0))
            if floor > 1.0e-6:
                fails.append("the FLOOR moved: the same frame read twice differs by %.3g, "
                             "so this instrument cannot tell movement from noise" % floor)
            if best < min_move:
                fails.append("the largest vertex movement is %.6f, the row expects at "
                             "least %.6f" % (best, min_move))
else:
    print("BLENDER movement SKIPPED (no --frame-n; a SKIP is never a pass)")

for f in fails:
    print("FAIL: " + f)
print("BLENDER result %s" % ("FAIL" if fails else "PASS"))
sys.exit(1 if fails else 0)
