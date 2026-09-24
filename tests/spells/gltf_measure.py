"""Read MEASUREMENTS out of a .gltf + .bin, for tests/spells/gltf_export_options.sh.

The exporter prints its own counts on the MEASURED line, which is what it
BELIEVES it did. This reads the file it actually wrote, which is what a reader
will see -- two different instruments, and the gate runs both so that a
bookkeeping bug cannot pass itself off as an export.

  python tests/spells/gltf_measure.py FILE.gltf [--bone NAME] [--anim-node NAME]

Prints `M <key> <value>` lines:
  nodes, meshes, skins, joints_max, joints_lists, animations, channels,
  scale_channels, images, buffers_uri, root_scale
  bone_<NAME>_len       the length of NAME's own translation, in FILE units --
                        the same bone in a metre file and a game-unit file must
                        differ by exactly 64/0.9144, which is the units row
  anim_<NAME>_translation_span  the distance between the first and last
                        translation key of NAME's channel, in FILE units --
                        0 when the travel was stripped, non-zero when it was
                        kept on the bone. That is the root-motion row.
Exit 2 on a file it cannot read. Lane GLTFEXPORT1, 2026-09-19.
"""
import base64
import json
import math
import os
import struct
import sys

argv = sys.argv[1:]
if not argv:
    print("usage: gltf_measure.py FILE.gltf [--bone NAME] [--anim-node NAME]")
    sys.exit(2)
path = argv[0]


def opt(name):
    return argv[argv.index(name) + 1] if name in argv and argv.index(name) + 1 < len(argv) else None


with open(path, "r", encoding="utf-8") as fh:
    doc = json.load(fh)

nodes = doc.get("nodes", [])
print("M nodes %d" % len(nodes))
print("M meshes %d" % len(doc.get("meshes", [])))
skins = doc.get("skins", [])
print("M skins %d" % len(skins))
lists = sorted({len(s.get("joints", [])) for s in skins})
print("M joints_max %d" % (max(lists) if lists else 0))
print("M joints_lists %s" % (",".join(str(x) for x in lists) if lists else "-"))
# one joint list shared by every skin is the point of the `joints whole` option:
# it is what makes Blender build ONE armature
same = "yes" if skins and all(s.get("joints") == skins[0].get("joints") for s in skins) else "no"
print("M joints_identical %s" % same)

anims = doc.get("animations", [])
print("M animations %d" % len(anims))
nch = sum(len(a.get("channels", [])) for a in anims)
nsc = sum(1 for a in anims for c in a.get("channels", [])
          if c.get("target", {}).get("path") == "scale")
print("M channels %d" % nch)
print("M scale_channels %d" % nsc)
print("M images %d" % len(doc.get("images", [])))
print("M buffers_uri %s" % ",".join(str(b.get("uri", "<embedded>")) for b in doc.get("buffers", [])))
mpu = doc.get("asset", {}).get("extras", {}).get("metresPerUnit")
print("M asset_metres_per_unit %s" % (repr(mpu) if mpu is not None else "-"))

roots = doc.get("scenes", [{}])[doc.get("scene", 0)].get("nodes", []) if doc.get("scenes") else []
if roots:
    rs = nodes[roots[0]].get("scale", [1.0, 1.0, 1.0])
    print("M root_scale %.9f,%.9f,%.9f" % tuple(float(x) for x in rs))

byname = {}
for i, n in enumerate(nodes):
    if n.get("name") and n["name"] not in byname:
        byname[n["name"]] = i

bone = opt("--bone")
if bone:
    i = byname.get(bone)
    if i is None:
        print("M bone_%s_len MISSING" % bone)
    else:
        t = nodes[i].get("translation", [0.0, 0.0, 0.0])
        print("M bone_%s_len %.9f" % (bone, math.sqrt(sum(float(x) ** 2 for x in t))))
        print("M bone_%s_translation %.9f,%.9f,%.9f" % ((bone,) + tuple(float(x) for x in t)))

# ---- accessor reading, only when an animation measurement was asked for ----

def load_buffer(b):
    uri = b.get("uri")
    if uri is None:
        raise SystemExit("M ERROR this reader does not open .glb")
    if uri.startswith("data:"):
        return base64.b64decode(uri.split(",", 1)[1])
    with open(os.path.join(os.path.dirname(os.path.abspath(path)), uri), "rb") as f:
        return f.read()


CT = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2),
      5125: ("I", 4), 5126: ("f", 4)}
NC = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def read_accessor(idx, bufs):
    acc = doc["accessors"][idx]
    fmt, size = CT[acc["componentType"]]
    n = NC[acc["type"]]
    bv = doc["bufferViews"][acc["bufferView"]]
    data = bufs[bv.get("buffer", 0)]
    base = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    stride = bv.get("byteStride") or size * n
    out = []
    for e in range(acc["count"]):
        off = base + e * stride
        out.append(struct.unpack_from("<" + fmt * n, data, off))
    return out


anim_node = opt("--anim-node")
if anim_node is not None:
    ni = byname.get(anim_node)
    if ni is None:
        print("M anim_%s_translation_span MISSING_NODE" % anim_node)
    else:
        bufs = [load_buffer(b) for b in doc.get("buffers", [])]
        found = False
        for a in anims:
            for c in a.get("channels", []):
                tg = c.get("target", {})
                if tg.get("node") != ni or tg.get("path") != "translation":
                    continue
                found = True
                vals = read_accessor(a["samplers"][c["sampler"]]["output"], bufs)
                span = math.sqrt(sum((vals[-1][k] - vals[0][k]) ** 2 for k in range(3)))
                far = max(math.sqrt(sum((v[k] - vals[0][k]) ** 2 for k in range(3))) for v in vals)
                print("M anim_%s_keys %d" % (anim_node, len(vals)))
                print("M anim_%s_translation_span %.6f" % (anim_node, span))
                print("M anim_%s_translation_max %.6f" % (anim_node, far))
        if not found:
            print("M anim_%s_translation_span NO_CHANNEL" % anim_node)

# ---- the SCALE track of one node (finding 10) ----
# A scale channel that exists but never leaves (1,1,1) carries nothing. This
# reports the per-axis range and the largest non-uniformity of one node's scale
# samples, so the row can say the channel MOVED and that it is not uniform.
scale_node = opt("--scale-node")
if scale_node is not None:
    ni = byname.get(scale_node)
    if ni is None:
        print("M scale_%s_span MISSING_NODE" % scale_node)
    else:
        bufs = [load_buffer(b) for b in doc.get("buffers", [])]
        found = False
        for a in anims:
            for c in a.get("channels", []):
                tg = c.get("target", {})
                if tg.get("node") != ni or tg.get("path") != "scale":
                    continue
                found = True
                vals = read_accessor(a["samplers"][c["sampler"]]["output"], bufs)
                span = max(max(v[k] for v in vals) - min(v[k] for v in vals) for k in range(3))
                nonuni = max(max(v) - min(v) for v in vals)
                print("M scale_%s_keys %d" % (scale_node, len(vals)))
                print("M scale_%s_span %.6f" % (scale_node, span))
                print("M scale_%s_nonuniform %.6f" % (scale_node, nonuni))
                print("M scale_%s_first %.6f,%.6f,%.6f" % ((scale_node,) + tuple(vals[0][:3])))
                print("M scale_%s_max %.6f" % (scale_node, max(max(v) for v in vals)))
        if not found:
            print("M scale_%s_span NO_CHANNEL" % scale_node)
