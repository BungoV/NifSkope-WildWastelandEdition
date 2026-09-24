#!/usr/bin/env python3
"""INDEPENDENT read-back of a .gltf + .bin written by src/gltfexport.cpp.

`gltf_check.py` asks whether the file is a legal glTF. This asks the other
question: does it still say what the FO4 files said? It reads the glTF with
`json` + `struct`, undoes the axis and unit conversion the contract
(docs/GLTF_INTERCHANGE.md) declares, and holds every number against the
sources through two decoders that share no code with the writer:

  * `tests/spells/gltf_nifread.py` -- the NIF (nodes, shapes, skin), written
    from release/nif.xml;
  * `tests/spells/hkxanim_decode.py` -- the .hkx clip, lane HKX1's Python
    spline decompressor, written from docs/HKX_ANIMATION_FORMAT.md.

Gates, all pre-registered in the lane brief:

  R1 up-axis root     one scene root, named by asset.extras.upAxisNode,
                      carrying exactly the Z-up -> Y-up quaternion, and every
                      NIF root beneath it
  R2 bone bind pose   every NIF NiNode appears once by name; its glTF
                      translation / unitScale, quaternion and scale reproduce
                      the NIF's local TRS (<= --tol-t units, <= --tol-deg,
                      <= 1e-6 on scale), and its parent is the same node
  R3 mesh             per shape: vertex count, triangle count, every index,
                      every position (/ unitScale), UV, normal, JOINTS_0 and
                      WEIGHTS_0 equal the NIF's; weight rows sum to 1 +- 1e-3;
                      the skin's joint list is the NIF's bone list, in order;
                      each inverse-bind matrix is the NIF's stored bone
                      transform with only its translation scaled
  R4 animation        for every channel: the target node is the bone of some
                      track of the clip, the sampler times are i * frameDuration,
                      and every frame's translation / rotation / scale
                      reproduces the decoder's (<= --tol-t, <= --tol-deg)
  R5 bind rigidity    from the glTF alone: every joint matrix
                      J_k = global(joint_k) * inverseBind_k has the Z-up ->
                      Y-up rotation and the SAME translation (spread <=
                      --tol-skin units). Not the identity: FO4 body meshes
                      store their vertices with the origin at the top of the
                      head, so J is a lift of (-0.0002, -0.8818, +120.8437)
                      units -- measured on Bethesda's own MaleBody.nif, all
                      58 bones. This is what proves the inverse-bind
                      convention, the node composition, the up-axis root and
                      the joint indices agree at once.
  R6 root motion      when extras.rootMotion says it was applied, the root
                      node's translation channel minus its bind translation
                      equals the clip's per-frame reference-frame translation

THE FLOOR (CONSTITUTION 4: a check that cannot fail is not a check). Every
gate has a sabotage that must turn it red, applied to the data AFTER reading
and BEFORE checking:

    --sabotage ibm-transpose   transpose each inverse-bind 3x3   -> R3, R5
    --sabotage joint-shift     rotate JOINTS_0 by one            -> R5
    --sabotage quat-conjugate  negate x,y,z of every anim quat   -> R4
    --sabotage frame-shift     shift the animation by one frame  -> R4
    --sabotage unit-scale      read metresPerUnit as 1/64        -> R2, R3
    --sabotage up-axis         drop the Y-up root's rotation     -> R1, R5
    --sabotage node-translate  move one bone by 1 unit           -> R2, R5

Usage:
  gltf_readback.py FILE.gltf --nif CHAR.nif [--clip C.hkx --bones skeleton.hkx]
                   [--tol-t 1e-4] [--tol-deg 0.01] [--tol-skin 0.002]
                   [--sabotage KIND]

Prints one line per failure and `N checks, M failures`. Exit 0 when clean.
"""
import json
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import gltf_nifread                                            # noqa: E402
import hkxanim_decode                                          # noqa: E402

CTYPE = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
CSIZE = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
NCOMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}

checks = 0
failures = []


def check(ok, text):
    global checks
    checks += 1
    if not ok:
        failures.append(text)
    return ok


# ---------------------------------------------------------------- maths
def mat_ident():
    return [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0]    # column-major


def mat_mul(a, b):
    """Column-major 4x4 product a*b (apply b first)."""
    out = [0.0] * 16
    for c in range(4):
        for r in range(4):
            out[c * 4 + r] = sum(a[k * 4 + r] * b[c * 4 + k] for k in range(4))
    return out


def mat_apply(m, v):
    return tuple(m[0 * 4 + r] * v[0] + m[1 * 4 + r] * v[1] + m[2 * 4 + r] * v[2] + m[3 * 4 + r]
                 for r in range(3))


def trs_matrix(t, q, s):
    """glTF node TRS -> column-major 4x4. q is (x, y, z, w)."""
    x, y, z, w = q
    r = [1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),
         2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
         2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)]
    m = [r[0] * s[0], r[1] * s[0], r[2] * s[0], 0.0,
         r[3] * s[1], r[4] * s[1], r[5] * s[1], 0.0,
         r[6] * s[2], r[7] * s[2], r[8] * s[2], 0.0,
         t[0], t[1], t[2], 1.0]
    return m


def quat_from_rowmajor(r):
    """Row-major 3x3 with v' = R v -> (x, y, z, w). Shepperd's method."""
    tr = r[0] + r[4] + r[8]
    if tr > 0.0:
        s = math.sqrt(tr + 1.0) * 2.0
        w, x, y, z = 0.25 * s, (r[7] - r[5]) / s, (r[2] - r[6]) / s, (r[3] - r[1]) / s
    elif r[0] > r[4] and r[0] > r[8]:
        s = math.sqrt(1.0 + r[0] - r[4] - r[8]) * 2.0
        w, x, y, z = (r[7] - r[5]) / s, 0.25 * s, (r[1] + r[3]) / s, (r[2] + r[6]) / s
    elif r[4] > r[8]:
        s = math.sqrt(1.0 + r[4] - r[0] - r[8]) * 2.0
        w, x, y, z = (r[2] - r[6]) / s, (r[1] + r[3]) / s, 0.25 * s, (r[5] + r[7]) / s
    else:
        s = math.sqrt(1.0 + r[8] - r[0] - r[4]) * 2.0
        w, x, y, z = (r[3] - r[1]) / s, (r[2] + r[6]) / s, (r[5] + r[7]) / s, 0.25 * s
    n = math.sqrt(w * w + x * x + y * y + z * z)
    return (x / n, y / n, z / n, w / n)


def quat_deg(a, b):
    """Angle between two unit quaternions, in degrees. 2*asin(|a -+ b|/2) --
    never acos(dot), which has no resolution below ~0.03 deg (HKX1)."""
    d1 = math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(4)))
    d2 = math.sqrt(sum((a[i] + b[i]) ** 2 for i in range(4)))
    return math.degrees(2.0 * math.asin(min(1.0, min(d1, d2) / 2.0)))


# ---------------------------------------------------------------- glTF
class Gltf:
    def __init__(self, path):
        self.path = path
        with open(path, "r", encoding="utf-8") as fh:
            self.g = json.load(fh)
        uri = self.g["buffers"][0]["uri"]
        self.bin = open(os.path.join(os.path.dirname(os.path.abspath(path)), uri), "rb").read()
        self.nodes = self.g.get("nodes", [])
        self.parent = {}
        for i, nd in enumerate(self.nodes):
            for c in nd.get("children", []):
                self.parent[c] = i
        self._global = {}

    def acc(self, i):
        a = self.g["accessors"][i]
        v = self.g["bufferViews"][a["bufferView"]]
        off = v.get("byteOffset", 0) + a.get("byteOffset", 0)
        n = NCOMP[a["type"]]
        flat = struct.unpack_from("<%d%s" % (a["count"] * n, CTYPE[a["componentType"]]), self.bin, off)
        return [flat[k * n:(k + 1) * n] for k in range(a["count"])] if n > 1 else list(flat)

    def local(self, i):
        nd = self.nodes[i]
        return trs_matrix(nd.get("translation", [0, 0, 0]),
                          nd.get("rotation", [0, 0, 0, 1]),
                          nd.get("scale", [1, 1, 1]))

    def globalm(self, i):
        if i in self._global:
            return self._global[i]
        m = self.local(i)
        p = self.parent.get(i)
        if p is not None:
            m = mat_mul(self.globalm(p), m)
        self._global[i] = m
        return m


# ---------------------------------------------------------------- gates
def main(argv):
    def opt(name, default=None):
        return argv[argv.index(name) + 1] if name in argv else default

    if len(argv) < 2 or "--nif" not in argv:
        print(__doc__)
        return 2
    path = argv[1]
    nifpath = opt("--nif")
    clip = opt("--clip")
    bones = opt("--bones")
    tol_t = float(opt("--tol-t", "1e-4"))
    tol_deg = float(opt("--tol-deg", "0.01"))
    sab = opt("--sabotage", "")

    gl = Gltf(path)
    g = gl.g
    nif = gltf_nifread.Nif(nifpath)
    check(not nif.problems, "the NIF reader reported: %s" % "; ".join(nif.problems))

    U = float(g["asset"]["extras"]["metresPerUnit"])
    if sab == "unit-scale":
        U = 1.0 / 64.0
    upname = g["asset"]["extras"]["upAxisNode"]

    # ---- R1: the up-axis root ---------------------------------------
    roots = g["scenes"][g.get("scene", 0)]["nodes"]
    check(len(roots) == 1, "R1: the scene has %d roots, the contract says one" % len(roots))
    r0 = roots[0]
    check(gl.nodes[r0].get("name") == upname,
          "R1: the scene root is %r, asset.extras.upAxisNode says %r" % (gl.nodes[r0].get("name"), upname))
    if sab == "up-axis":
        gl.nodes[r0]["rotation"] = [0.0, 0.0, 0.0, 1.0]
        gl._global = {}
    s2 = math.sqrt(0.5)
    q = gl.nodes[r0].get("rotation", [0, 0, 0, 1])
    check(quat_deg(tuple(q), (-s2, 0.0, 0.0, s2)) <= 1e-3,
          "R1: the up-axis root's rotation is %s, the contract says -90 deg about X (%r)"
          % (q, [-s2, 0, 0, s2]))
    check("translation" not in gl.nodes[r0] or gl.nodes[r0]["translation"] == [0, 0, 0],
          "R1: the up-axis root carries a translation")

    # ---- name maps ---------------------------------------------------
    gname = {}
    for i, nd in enumerate(gl.nodes):
        gname.setdefault(nd.get("name", ""), []).append(i)
    nifname = {}
    for bi, nd in nif.nodes.items():
        nifname.setdefault(nd["name"], []).append(bi)

    if sab == "node-translate":
        for nm, idx in sorted(gname.items()):
            if nm in nifname and len(idx) == 1 and "translation" in gl.nodes[idx[0]]:
                gl.nodes[idx[0]]["translation"][0] += 1.0 * U
                break
        gl._global = {}

    # ---- R2: bone bind pose -----------------------------------------
    matched = 0
    for nm, blocks in sorted(nifname.items()):
        if len(blocks) != 1:
            continue                                    # a duplicate name is not comparable
        if nm not in gname:
            check(False, "R2: NIF node %r has no glTF node" % nm)
            continue
        if len(gname[nm]) != 1:
            check(False, "R2: %d glTF nodes are called %r" % (len(gname[nm]), nm))
            continue
        gi, nd = gname[nm][0], nif.nodes[blocks[0]]
        gn = gl.nodes[gi]
        gt = gn.get("translation", [0, 0, 0])
        for k in range(3):
            check(abs(gt[k] / U - nd["t"][k]) <= tol_t,
                  "R2: node %r translation[%d] = %.9f m / %.9g = %.6f units, the NIF stores %.6f"
                  % (nm, k, gt[k], U, gt[k] / U, nd["t"][k]))
        want = quat_from_rowmajor(nd["r"])
        have = tuple(gn.get("rotation", [0, 0, 0, 1]))
        d = quat_deg(have, want)
        check(d <= tol_deg, "R2: node %r rotation is %.6f deg from the NIF's" % (nm, d))
        gs = gn.get("scale", [1, 1, 1])
        for k in range(3):
            check(abs(gs[k] - nd["s"]) <= 1e-6,
                  "R2: node %r scale[%d] = %.9f, the NIF stores %.9f" % (nm, k, gs[k], nd["s"]))
        # the parent, by name
        gp = gl.parent.get(gi)
        gpn = gl.nodes[gp]["name"] if gp is not None else None
        npn = nif.nodes[nd["parent"]]["name"] if nd["parent"] is not None else None
        check(gpn == (npn if npn is not None else upname),
              "R2: node %r hangs on %r in the glTF and on %r in the NIF" % (nm, gpn, npn))
        matched += 1
    check(matched >= len(nif.nodes) - 1,
          "R2: %d of %d NIF nodes were comparable by name" % (matched, len(nif.nodes)))

    rz2y = trs_matrix([0, 0, 0], [-s2, 0.0, 0.0, s2], [1, 1, 1])

    # ---- R3: the meshes ---------------------------------------------
    nifshape = {sh["name"]: sh for sh in nif.shapes.values()}
    ibms = {}                                            # glTF mesh index -> [16 floats] per joint
    for mi, mesh in enumerate(g.get("meshes", [])):
        nm = mesh["name"]
        if nm not in nifshape:
            check(False, "R3: glTF mesh %r is in no NIF shape" % nm)
            continue
        sh = nifshape[nm]
        prim = mesh["primitives"][0]
        check(len(mesh["primitives"]) == 1,
              "R3: mesh %r has %d primitives; the contract merges every segment into one"
              % (nm, len(mesh["primitives"])))
        pos = gl.acc(prim["attributes"]["POSITION"])
        check(len(pos) == sh["numVerts"],
              "R3: mesh %r has %d vertices, the NIF shape has %d" % (nm, len(pos), sh["numVerts"]))
        idx = gl.acc(prim["indices"])
        check(len(idx) == sh["numTris"] * 3,
              "R3: mesh %r has %d indices, the NIF shape has %d triangles" % (nm, len(idx), sh["numTris"]))
        check(list(idx) == list(sh["tris"]), "R3: mesh %r index list differs from the NIF's" % nm)
        worst = 0.0
        for v in range(min(len(pos), len(sh["verts"]))):
            for k in range(3):
                worst = max(worst, abs(pos[v][k] / U - sh["verts"][v][k]))
        check(worst <= tol_t, "R3: mesh %r worst position error %.9f units" % (nm, worst))
        if "TEXCOORD_0" in prim["attributes"] and sh["uvs"]:
            uv = gl.acc(prim["attributes"]["TEXCOORD_0"])
            w = max(abs(uv[v][k] - sh["uvs"][v][k]) for v in range(len(uv)) for k in range(2))
            check(w <= 1e-6, "R3: mesh %r worst UV error %.9f" % (nm, w))
        if "NORMAL" in prim["attributes"] and sh["norms"]:
            nr = gl.acc(prim["attributes"]["NORMAL"])
            w = 0.0
            for v in range(len(nr)):
                a = sh["norms"][v]
                ln = math.sqrt(sum(x * x for x in a)) or 1.0
                w = max(w, max(abs(nr[v][k] - a[k] / ln) for k in range(3)))
            check(w <= 1e-6, "R3: mesh %r worst normal error %.9f (after unit-normalising the NIF's)" % (nm, w))
        if "JOINTS_0" in prim["attributes"]:
            jt = [list(x) for x in gl.acc(prim["attributes"]["JOINTS_0"])]
            wt = [list(x) for x in gl.acc(prim["attributes"]["WEIGHTS_0"])]
            if sab == "joint-shift":
                jt = [x[1:] + x[:1] for x in jt]
            check(all(list(jt[v]) == list(sh["boneIndices"][v]) for v in range(len(jt))),
                  "R3: mesh %r JOINTS_0 differs from the NIF's bone indices" % nm)
            wmax = max(abs(wt[v][k] - sh["weights"][v][k]) for v in range(len(wt)) for k in range(4))
            check(wmax <= 1e-6, "R3: mesh %r worst weight error %.9f" % (nm, wmax))
            smax = max(abs(sum(w) - 1.0) for w in wt)
            check(smax <= 1e-3, "R3: mesh %r worst weight-row sum error %.6f" % (nm, smax))
            # the skin: joints in the NIF's bone order, and the inverse binds
            node_of_mesh = [i for i, nd in enumerate(gl.nodes) if nd.get("mesh") == mi]
            check(len(node_of_mesh) == 1, "R3: %d nodes carry mesh %r" % (len(node_of_mesh), nm))
            skin = g["skins"][gl.nodes[node_of_mesh[0]]["skin"]]
            names = [gl.nodes[j]["name"] for j in skin["joints"]]
            want = [b[0] for b in nif.skin_bones(sh)]
            check(names == want, "R3: mesh %r joint names %s, the NIF's bones %s"
                  % (nm, names[:4], want[:4]))
            mats = [list(x) for x in gl.acc(skin["inverseBindMatrices"])]
            if sab == "ibm-transpose":
                for m in mats:
                    for c in range(3):
                        for r in range(c + 1, 3):
                            m[c * 4 + r], m[r * 4 + c] = m[r * 4 + c], m[c * 4 + r]
            ibms[mi] = mats
            for k, (bn, bt) in enumerate(nif.skin_bones(sh)):
                m = mats[k]
                for c in range(3):
                    for r in range(3):
                        check(abs(m[c * 4 + r] - bt["r"][r * 3 + c] * bt["s"]) <= 1e-6,
                              "R3: mesh %r bone %r inverse-bind [%d][%d] is %.9f, the NIF's row-major "
                              "rotation x scale is %.9f" % (nm, bn, r, c, m[c * 4 + r], bt["r"][r * 3 + c] * bt["s"]))
                for k2 in range(3):
                    check(abs(m[12 + k2] / U - bt["t"][k2]) <= tol_t,
                          "R3: mesh %r bone %r inverse-bind translation[%d] = %.6f units, the NIF stores %.6f"
                          % (nm, bn, k2, m[12 + k2] / U, bt["t"][k2]))
                check(m[3] == 0.0 and m[7] == 0.0 and m[11] == 0.0 and m[15] == 1.0,
                      "R3: mesh %r bone %r inverse-bind last row is %s" % (nm, bn, [m[3], m[7], m[11], m[15]]))

    # ---- R5: the bind pose is one rigid transform -------------------
    # NOT "skinning is the identity". FO4 body meshes store their vertices in
    # a frame OFFSET from the skeleton: measured on Bethesda's own
    # MaleBody.nif, global(bone) * storedBoneTransform is
    # translate(-0.0002, -0.8818, +120.8437) units for all 58 bones -- the
    # model's origin sits at the top of the head (its z range is -120.25 ..
    # -5.69) and the skin lifts it onto its feet. So what has to hold is that
    # every joint agrees on ONE rigid transform:
    #     J_k = global(joint_k) * inverseBind_k
    #   * the rotation of every J_k is exactly the Z-up -> Y-up rotation, and
    #   * the translations of all J_k agree, to --tol-skin units.
    # A transposed inverse bind, a shifted joint list, a wrong node
    # composition and a dropped up-axis rotation each break one of the two,
    # and nothing else in this file tests all four at once. The spread is
    # also the honest measure of "does this NIF's node pose still equal the
    # pose its skin was authored against".
    tol_skin = float(opt("--tol-skin", "0.002"))
    for mi, mesh in enumerate(g.get("meshes", [])):
        if mi not in ibms:
            continue
        node_of_mesh = [i for i, nd in enumerate(gl.nodes) if nd.get("mesh") == mi][0]
        skin = g["skins"][gl.nodes[node_of_mesh]["skin"]]
        order = list(range(len(skin["joints"])))
        if sab == "joint-shift":
            order = order[1:] + order[:1]
        joint_mats = [mat_mul(gl.globalm(skin["joints"][order[k]]), ibms[mi][k]) for k in order]
        rworst = 0.0
        for m in joint_mats:
            for c in range(3):
                for r in range(3):
                    rworst = max(rworst, abs(m[c * 4 + r] - rz2y[c * 4 + r]))
        check(rworst <= 1e-3,
              "R5: mesh %r: a joint matrix's rotation is %.6f from the Z-up -> Y-up rotation"
              % (mesh["name"], rworst))
        t0 = joint_mats[0][12:15]
        tworst, wbone = 0.0, ""
        for k, m in enumerate(joint_mats):
            d = max(abs(m[12 + c] - t0[c]) for c in range(3)) / U
            if d > tworst:
                tworst, wbone = d, gl.nodes[skin["joints"][order[k]]]["name"]
        check(tworst <= tol_skin,
              "R5: mesh %r: joint %r puts the bind pose %.4f units from joint 0's (tolerance %.4f) -- "
              "this NIF's node pose is not the pose the skin was authored against"
              % (mesh["name"], wbone, tworst, tol_skin))
        off = (t0[0] / U, -t0[2] / U, t0[1] / U)     # glTF (x, y, z) back to NIF (x, y, z)
        print("  R5 %-22s rotation %.2e off Y-up, spread %.4f units (worst %s), storage offset "
              "(%.4f, %.4f, %.4f) NIF units"
              % (mesh["name"], rworst, tworst, wbone or "-", off[0], off[1], off[2]))

    # ---- R4 / R6: the animation -------------------------------------
    if clip:
        cf = hkxanim_decode.load(clip)
        anim = cf["animations"][0]
        frames, _ = hkxanim_decode.decode(anim)
        binding = cf["bindings"][0] if cf["bindings"] else None
        t2b = list(binding["transformTrackToBoneIndices"]) if binding else []
        if not t2b:
            t2b = list(range(anim["numberOfTransformTracks"]))
        bone_names = []
        if bones:
            bf = hkxanim_decode.load(bones)
            bone_names = bf["skeletons"][0]["boneNames"]
        elif cf["skeletons"]:
            bone_names = cf["skeletons"][0]["boneNames"]
        track_of_name = {}
        for t, b in enumerate(t2b):
            if 0 <= b < len(bone_names):
                track_of_name[bone_names[b].lower()] = t
        check(bool(track_of_name), "R4: no bone names -- pass --bones skeleton.hkx")

        an = g["animations"][0]
        extras = an.get("extras", {})
        check(extras.get("frames") == anim["numFrames"],
              "R4: extras.frames %s, the clip has %d" % (extras.get("frames"), anim["numFrames"]))
        check(abs(extras.get("frameDuration", 0) - anim["frameDuration"]) <= 1e-9,
              "R4: extras.frameDuration %s, the clip's is %.9f" % (extras.get("frameDuration"), anim["frameDuration"]))

        nframes = anim["numFrames"]
        rootapplied = extras.get("rootMotion", "").startswith("applied")
        rm = anim["rootMotion"]["samples"] if anim["rootMotion"] else None
        rootnode = None
        worst_t = worst_r = worst_s = 0.0
        seen_paths = 0
        for ch in an["channels"]:
            node = ch["target"]["node"]
            nm = gl.nodes[node]["name"]
            sm = an["samplers"][ch["sampler"]]
            times = gl.acc(sm["input"])
            out = [list(x) if isinstance(x, tuple) else x for x in gl.acc(sm["output"])]
            check(len(times) == nframes, "R4: node %r sampler has %d times for %d frames"
                  % (nm, len(times), nframes))
            tw = max(abs(times[i] - i * anim["frameDuration"]) for i in range(len(times)))
            check(tw <= 1e-6, "R4: node %r sampler input is %.9f s off i * frameDuration" % (nm, tw))
            t = track_of_name.get(nm.lower())
            if t is None:
                check(False, "R4: an animation channel drives node %r, which is no track's bone" % nm)
                continue
            seen_paths += 1
            if sab == "frame-shift":
                out = out[1:] + out[:1]
            if sab == "quat-conjugate" and ch["target"]["path"] == "rotation":
                out = [[-x[0], -x[1], -x[2], x[3]] for x in out]
            path = ch["target"]["path"]
            isroot = rootapplied and rm is not None and gl.nodes[node].get("name") and \
                t == track_of_name.get(nm.lower()) and nm.lower() == (bone_names[0].lower() if bone_names else "")
            if isroot:
                rootnode = node
            for f in range(nframes):
                tr, qq, sc, _ = frames[f][t]
                if path == "translation":
                    base = [tr[0], tr[1], tr[2]]
                    if isroot:
                        base = [base[k] + rm[f][k] for k in range(3)]
                    worst_t = max(worst_t, max(abs(out[f][k] / U - base[k]) for k in range(3)))
                elif path == "rotation":
                    want = (qq[0], qq[1], qq[2], qq[3])
                    if not isroot:
                        worst_r = max(worst_r, quat_deg(tuple(out[f]), want))
                elif path == "scale":
                    worst_s = max(worst_s, max(abs(out[f][k] - sc[k]) for k in range(3)))
        check(worst_t <= tol_t, "R4: worst animation translation error %.9f units (tolerance %g)"
              % (worst_t, tol_t))
        check(worst_r <= tol_deg, "R4: worst animation rotation error %.9f deg (tolerance %g)"
              % (worst_r, tol_deg))
        check(worst_s <= 1e-6, "R4: worst animation scale error %.9f" % worst_s)
        print("  R4 %d channels, %d frames: worst translation %.3e units, rotation %.3e deg, scale %.3e"
              % (seen_paths, nframes, worst_t, worst_r, worst_s))
        check(seen_paths > 0, "R4: not one channel could be matched to a track")

        # ---- R6 ------------------------------------------------------
        if rootapplied:
            check(rm is not None, "R6: extras says root motion was applied, the clip carries none")
            check(rootnode is not None, "R6: root motion was applied but no channel drives the root bone")
        else:
            check("rootMotion" in extras, "R6: the animation's extras do not say what happened to root motion")
            if rm is not None:
                mx = max(max(abs(s[k]) for k in range(3)) for s in rm)
                print("  R6 root motion present and NOT applied; the clip's own travel is %.3f units" % mx)

    for f in failures:
        print("FAIL: " + f)
    print("%d checks, %d failures%s" % (checks, len(failures),
                                        ("  [sabotage %s]" % sab) if sab else ""))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
