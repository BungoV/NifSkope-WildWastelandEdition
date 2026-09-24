#!/usr/bin/env python3
"""Structural checker for a .gltf + .bin written by src/gltfexport.cpp.

Stands in for the Khronos glTF validator, which is not installable on this
machine (no node, no npx, no pip package -- lane HKX4 checked and said so).
It reads the JSON and the buffer with nothing but the standard library and
tests the parts of the glTF 2.0 spec this writer can get wrong:

  the buffer          declared byteLength == the .bin's real size
  bufferViews         inside the buffer; offset 4-byte aligned
  accessors           view valid; count * componentSize * components ==
                      byteLength (this writer packs tightly); component type
                      known; min/max present where the spec requires them
                      (POSITION, animation sampler input) and RECOMPUTED from
                      the bytes rather than believed
  nodes               children in range; no node has two parents; every node
                      is reachable from scene 0; no cycles
  meshes              every attribute accessor has the same count; indices are
                      inside the vertex range; mode 4; material in range
  skins               joints are node indices; inverseBindMatrices is MAT4 with
                      one matrix per joint; JOINTS_0 values < len(joints);
                      WEIGHTS_0 rows sum to 1 within tolerance
  animations          sampler input/output counts equal; input is SCALAR float
                      and non-decreasing; rotation outputs are unit
                      quaternions; channel targets in range; paths known
  materials/textures  every index in range; an image uri that is not .png or
                      .jpg is a WARNING, not an error (FO4 ships .dds)

Usage:  gltf_check.py FILE.gltf [--weight-tol 1e-3]
Prints one line per failed check and `N checks, M failures, W warnings`.
Exit 0 when there are no failures, 1 otherwise, 2 on a file that will not open.
"""
import json
import math
import os
import struct
import sys

CTYPE = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2),
         5125: ("I", 4), 5126: ("f", 4)}
NCOMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT2": 4, "MAT3": 9, "MAT4": 16}
PATHS = ("translation", "rotation", "scale", "weights")

checks = 0
failures = []
warnings = []


def check(ok, text):
    global checks
    checks += 1
    if not ok:
        failures.append(text)
    return ok


def warn(ok, text):
    global checks
    checks += 1
    if not ok:
        warnings.append(text)
    return ok


def read_accessor(g, buf, i):
    """Raw values of accessor i as a flat list, no sparse, no byteStride."""
    a = g["accessors"][i]
    fmt, size = CTYPE[a["componentType"]]
    n = NCOMP[a["type"]]
    v = g["bufferViews"][a["bufferView"]]
    off = v.get("byteOffset", 0) + a.get("byteOffset", 0)
    count = a["count"] * n
    return list(struct.unpack_from("<%d%s" % (count, fmt), buf, off))


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    wtol = float(argv[argv.index("--weight-tol") + 1]) if "--weight-tol" in argv else 1e-3
    with open(path, "r", encoding="utf-8") as fh:
        g = json.load(fh)
    base = os.path.dirname(os.path.abspath(path))

    check(g.get("asset", {}).get("version") == "2.0",
          "asset.version is %r, not '2.0'" % g.get("asset", {}).get("version"))

    # ---- buffer -----------------------------------------------------
    bufs = g.get("buffers", [])
    check(len(bufs) == 1, "%d buffers; this writer emits exactly one" % len(bufs))
    if not bufs:
        return report()
    uri = bufs[0].get("uri", "")
    binpath = os.path.join(base, uri)
    check(os.path.exists(binpath), "buffer uri %r does not exist beside the .gltf" % uri)
    if not os.path.exists(binpath):
        return report()
    buf = open(binpath, "rb").read()
    check(bufs[0]["byteLength"] == len(buf),
          "buffer byteLength %d, %s is %d bytes" % (bufs[0]["byteLength"], uri, len(buf)))

    # ---- bufferViews ------------------------------------------------
    for i, v in enumerate(g.get("bufferViews", [])):
        off, ln = v.get("byteOffset", 0), v["byteLength"]
        check(v.get("buffer", 0) == 0, "bufferView %d points at buffer %d" % (i, v.get("buffer", 0)))
        check(off >= 0 and ln > 0 and off + ln <= len(buf),
              "bufferView %d spans %d..%d of a %d-byte buffer" % (i, off, off + ln, len(buf)))
        check(off % 4 == 0, "bufferView %d byteOffset %d is not 4-byte aligned" % (i, off))
        if "target" in v:
            check(v["target"] in (34962, 34963), "bufferView %d target %d" % (i, v["target"]))

    # ---- accessors --------------------------------------------------
    for i, a in enumerate(g.get("accessors", [])):
        ok = check(a["componentType"] in CTYPE, "accessor %d componentType %d" % (i, a["componentType"]))
        ok &= check(a["type"] in NCOMP, "accessor %d type %r" % (i, a["type"]))
        ok &= check(0 <= a["bufferView"] < len(g["bufferViews"]),
                    "accessor %d bufferView %d" % (i, a["bufferView"]))
        if not ok:
            continue
        fmt, size = CTYPE[a["componentType"]]
        n = NCOMP[a["type"]]
        v = g["bufferViews"][a["bufferView"]]
        want = a["count"] * n * size
        check(v["byteLength"] == want,
              "accessor %d: %d elements x %d x %dB = %d, its bufferView is %d bytes"
              % (i, a["count"], n, size, want, v["byteLength"]))
        check(a["count"] > 0, "accessor %d has count 0" % i)
        if "min" in a or "max" in a:
            vals = read_accessor(g, buf, i)
            mn = [min(vals[c::n]) for c in range(n)]
            mx = [max(vals[c::n]) for c in range(n)]
            check(all(abs(x - y) <= 1e-6 * max(1.0, abs(y)) for x, y in zip(a["min"], mn)),
                  "accessor %d min %s, recomputed %s" % (i, a["min"], mn))
            check(all(abs(x - y) <= 1e-6 * max(1.0, abs(y)) for x, y in zip(a["max"], mx)),
                  "accessor %d max %s, recomputed %s" % (i, a["max"], mx))

    nodes = g.get("nodes", [])
    # ---- nodes ------------------------------------------------------
    parent = {}
    for i, nd in enumerate(nodes):
        for c in nd.get("children", []):
            check(0 <= c < len(nodes), "node %d has child %d, outside 0..%d" % (i, c, len(nodes) - 1))
            if 0 <= c < len(nodes):
                check(c not in parent, "node %d has two parents (%s and %d)" % (c, parent.get(c), i))
                parent[c] = i
        if "mesh" in nd:
            check(0 <= nd["mesh"] < len(g.get("meshes", [])), "node %d mesh %d" % (i, nd["mesh"]))
        if "skin" in nd:
            check(0 <= nd["skin"] < len(g.get("skins", [])), "node %d skin %d" % (i, nd["skin"]))
            check("mesh" in nd, "node %d has a skin but no mesh" % i)
        for key, ln in (("translation", 3), ("scale", 3), ("rotation", 4)):
            if key in nd:
                check(len(nd[key]) == ln, "node %d %s has %d values" % (i, key, len(nd[key])))
                check(all(math.isfinite(x) for x in nd[key]), "node %d %s is not finite" % (i, key))
        if "rotation" in nd:
            q = nd["rotation"]
            l = math.sqrt(sum(x * x for x in q))
            check(abs(l - 1.0) < 1e-5, "node %d rotation has length %.9f" % (i, l))

    scenes = g.get("scenes", [])
    check(len(scenes) >= 1 and g.get("scene", 0) < len(scenes), "scene index out of range")
    seen = set()
    stack = list(scenes[g.get("scene", 0)]["nodes"]) if scenes else []
    while stack:
        x = stack.pop()
        if x in seen:
            check(False, "node %d is reachable twice: the hierarchy has a cycle or a shared child" % x)
            continue
        seen.add(x)
        stack.extend(nodes[x].get("children", []))
    check(len(seen) == len(nodes),
          "%d of %d nodes are reachable from the scene" % (len(seen), len(nodes)))

    # ---- meshes -----------------------------------------------------
    for m, mesh in enumerate(g.get("meshes", [])):
        for p, prim in enumerate(mesh["primitives"]):
            check(prim.get("mode", 4) == 4, "mesh %d primitive %d mode %d" % (m, p, prim.get("mode", 4)))
            counts = set()
            for name, acc in prim["attributes"].items():
                check(0 <= acc < len(g["accessors"]), "mesh %d attribute %s accessor %d" % (m, name, acc))
                counts.add(g["accessors"][acc]["count"])
            check(len(counts) == 1, "mesh %d primitive %d attribute counts %s" % (m, p, sorted(counts)))
            nv = counts.pop() if counts else 0
            if "indices" in prim:
                idx = read_accessor(g, buf, prim["indices"])
                check(len(idx) % 3 == 0, "mesh %d has %d indices" % (m, len(idx)))
                check(max(idx) < nv, "mesh %d index %d against %d vertices" % (m, max(idx), nv))
            if "material" in prim:
                check(0 <= prim["material"] < len(g.get("materials", [])),
                      "mesh %d material %d" % (m, prim["material"]))
            if "POSITION" in prim["attributes"]:
                check("min" in g["accessors"][prim["attributes"]["POSITION"]],
                      "mesh %d POSITION accessor has no min/max (the spec requires it)" % m)

    # ---- skins ------------------------------------------------------
    for s, skin in enumerate(g.get("skins", [])):
        joints = skin["joints"]
        for j in joints:
            check(0 <= j < len(nodes), "skin %d joint %d is not a node" % (s, j))
        check(len(set(joints)) == len(joints), "skin %d lists a joint twice" % s)
        if "inverseBindMatrices" in skin:
            a = g["accessors"][skin["inverseBindMatrices"]]
            check(a["type"] == "MAT4", "skin %d inverseBindMatrices type %r" % (s, a["type"]))
            check(a["count"] == len(joints),
                  "skin %d: %d inverse-bind matrices for %d joints" % (s, a["count"], len(joints)))
        # the mesh that uses this skin
        for i, nd in enumerate(nodes):
            if nd.get("skin") != s:
                continue
            prim = g["meshes"][nd["mesh"]]["primitives"][0]
            if "JOINTS_0" not in prim["attributes"]:
                check(False, "node %d uses skin %d but its mesh has no JOINTS_0" % (i, s))
                continue
            jv = read_accessor(g, buf, prim["attributes"]["JOINTS_0"])
            check(max(jv) < len(joints),
                  "mesh of node %d references joint %d of %d" % (i, max(jv), len(joints)))
            wv = read_accessor(g, buf, prim["attributes"]["WEIGHTS_0"])
            check(len(wv) == len(jv), "JOINTS_0 has %d values, WEIGHTS_0 %d" % (len(jv), len(wv)))
            worst = 0.0
            for k in range(0, len(wv), 4):
                worst = max(worst, abs(sum(wv[k:k + 4]) - 1.0))
            check(worst <= wtol,
                  "mesh of node %d: worst weight-sum error %.6f (tolerance %.6f)" % (i, worst, wtol))

    # ---- animations -------------------------------------------------
    for ai, an in enumerate(g.get("animations", [])):
        for si, sm in enumerate(an["samplers"]):
            ia, oa = sm["input"], sm["output"]
            check(0 <= ia < len(g["accessors"]) and 0 <= oa < len(g["accessors"]),
                  "animation %d sampler %d accessor out of range" % (ai, si))
            check(g["accessors"][ia]["type"] == "SCALAR" and g["accessors"][ia]["componentType"] == 5126,
                  "animation %d sampler %d input is %s/%d" % (ai, si, g["accessors"][ia]["type"],
                                                              g["accessors"][ia]["componentType"]))
            check("min" in g["accessors"][ia] and "max" in g["accessors"][ia],
                  "animation %d sampler %d input accessor has no min/max (the spec requires it)" % (ai, si))
            check(g["accessors"][ia]["count"] == g["accessors"][oa]["count"],
                  "animation %d sampler %d: %d inputs, %d outputs"
                  % (ai, si, g["accessors"][ia]["count"], g["accessors"][oa]["count"]))
            check(sm.get("interpolation", "LINEAR") in ("LINEAR", "STEP", "CUBICSPLINE"),
                  "animation %d sampler %d interpolation %r" % (ai, si, sm.get("interpolation")))
            t = read_accessor(g, buf, ia)
            check(all(t[k] <= t[k + 1] for k in range(len(t) - 1)),
                  "animation %d sampler %d input times are not non-decreasing" % (ai, si))
        for ci, ch in enumerate(an["channels"]):
            check(0 <= ch["sampler"] < len(an["samplers"]),
                  "animation %d channel %d sampler %d" % (ai, ci, ch["sampler"]))
            tgt = ch["target"]
            check(0 <= tgt["node"] < len(nodes), "animation %d channel %d node %d" % (ai, ci, tgt["node"]))
            check(tgt["path"] in PATHS, "animation %d channel %d path %r" % (ai, ci, tgt["path"]))
            sm = an["samplers"][ch["sampler"]]
            oa = g["accessors"][sm["output"]]
            want = {"translation": "VEC3", "scale": "VEC3", "rotation": "VEC4"}.get(tgt["path"])
            if want:
                check(oa["type"] == want,
                      "animation %d channel %d path %s output type %s" % (ai, ci, tgt["path"], oa["type"]))
            if tgt["path"] == "rotation":
                q = read_accessor(g, buf, sm["output"])
                worst = 0.0
                for k in range(0, len(q), 4):
                    worst = max(worst, abs(math.sqrt(sum(x * x for x in q[k:k + 4])) - 1.0))
                check(worst < 1e-5,
                      "animation %d channel %d: worst rotation length error %.3g" % (ai, ci, worst))

    # ---- materials, textures, images --------------------------------
    for mi, mat in enumerate(g.get("materials", [])):
        pbr = mat.get("pbrMetallicRoughness", {})
        if "baseColorTexture" in pbr:
            ti = pbr["baseColorTexture"]["index"]
            check(0 <= ti < len(g.get("textures", [])), "material %d baseColorTexture %d" % (mi, ti))
            if 0 <= ti < len(g.get("textures", [])):
                tx = g["textures"][ti]
                check(0 <= tx["source"] < len(g.get("images", [])),
                      "texture %d source %d" % (ti, tx["source"]))
                if "sampler" in tx:
                    check(0 <= tx["sampler"] < len(g.get("samplers", [])),
                          "texture %d sampler %d" % (ti, tx["sampler"]))
    for ii, im in enumerate(g.get("images", [])):
        u = im.get("uri", "")
        warn(u.lower().endswith((".png", ".jpg", ".jpeg")),
             "image %d uri %r is not a png or jpeg: glTF's spec allows only those two, and "
             "Blender will not load it (the NIF's texture is a .dds)" % (ii, u))

    return report()


def report():
    for f in failures:
        print("FAIL: " + f)
    for w in warnings:
        print("WARN: " + w)
    print("%d checks, %d failures, %d warnings" % (checks, len(failures), len(warnings)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
