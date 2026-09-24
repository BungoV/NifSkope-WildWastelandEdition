#!/usr/bin/env python3
"""GATE (c): a hand-written 3-bone glTF whose every imported number is known
in closed form, and the checker that holds the import against those numbers.

The file is written HERE, by hand, from the glTF 2.0 spec and the export
convention (src/gltfexport.cpp): a synthetic root 'NifSkope_Y_up' rotated -90
degrees about X, translations in metres at 0.9144/64 m per NIF unit, node TRS
in NIF space under that root.

The three channels are deliberately one of each interpolation, on three
different paths, so one file exercises LINEAR (slerp), STEP and CUBICSPLINE:

  Root   scale        CUBICSPLINE  keys 1.0, 2.0, 1.0, every tangent zero
  Hips   rotation     LINEAR       keys 0, 45, 90 degrees about +Z
  Spine  translation  STEP         keys (0,0,32), (0,10,32), (0,20,32) NIF units

Key times are 0, 0.5, 1.0 s; the import resamples at 30 fps, so 31 frames at
t = i/30 and the keys land exactly on frames 0, 15 and 30.

usage:
  python make_3bone.py write  out/3bone.gltf
  python make_3bone.py check  out/3bone.tsv
"""
import json, math, struct, sys, os

U = 0.9144 / 64.0            # metres per NIF unit, the exporter's constant
FPS = 30.0
NFRAMES = 31
KEYS = [0.0, 0.5, 1.0]

# bind pose, NIF units / NIF space
BIND = {
    "Root":  dict(t=(0.0, 0.0, 0.0),  r=(0.0, 0.0, 0.0, 1.0), s=(1.0, 1.0, 1.0)),
    "Hips":  dict(t=(0.0, 0.0, 64.0), r=(0.0, 0.0, 0.0, 1.0), s=(1.0, 1.0, 1.0)),
    "Spine": dict(t=(0.0, 0.0, 32.0), r=(0.0, 0.0, 0.0, 1.0), s=(1.0, 1.0, 1.0)),
}
ROT_KEYS_DEG = [0.0, 45.0, 90.0]
TRANS_KEYS = [(0.0, 0.0, 32.0), (0.0, 10.0, 32.0), (0.0, 20.0, 32.0)]
SCALE_KEYS = [1.0, 2.0, 1.0]


def qz(deg):
    """Rotation of `deg` about +Z as (x, y, z, w)."""
    h = math.radians(deg) * 0.5
    return (0.0, 0.0, math.sin(h), math.cos(h))


def write(path):
    bin_parts, views, accessors = [], [], []

    def add(fmt, data, typ, count, minmax=False):
        raw = struct.pack('<%d%s' % (len(data), fmt), *data)
        while len(b''.join(bin_parts)) % 4:
            bin_parts.append(b'\0')
        off = len(b''.join(bin_parts))
        bin_parts.append(raw)
        views.append(dict(buffer=0, byteOffset=off, byteLength=len(raw)))
        a = dict(bufferView=len(views) - 1, componentType=5126, count=count, type=typ)
        if minmax:
            a["min"] = [min(data)]
            a["max"] = [max(data)]
        accessors.append(a)
        return len(accessors) - 1

    a_time = add('f', KEYS, "SCALAR", len(KEYS), True)
    # LINEAR rotation on Hips
    rot = []
    for d in ROT_KEYS_DEG:
        rot.extend(qz(d))
    a_rot = add('f', rot, "VEC4", len(KEYS))
    # STEP translation on Spine, in METRES
    tr = []
    for v in TRANS_KEYS:
        tr.extend([c * U for c in v])
    a_tr = add('f', tr, "VEC3", len(KEYS))
    # CUBICSPLINE scale on Root: in-tangent, value, out-tangent per key
    sc = []
    for v in SCALE_KEYS:
        sc.extend([0.0, 0.0, 0.0, v, v, v, 0.0, 0.0, 0.0])
    a_sc = add('f', sc, "VEC3", 3 * len(KEYS))

    def node(name, extra=None):
        b = BIND[name]
        n = dict(name=name,
                 translation=[c * U for c in b["t"]],
                 rotation=list(b["r"]),
                 scale=list(b["s"]))
        if extra:
            n.update(extra)
        return n

    s = math.sqrt(0.5)
    nodes = [
        dict(name="NifSkope_Y_up", rotation=[-s, 0.0, 0.0, s], children=[1]),
        node("Root", dict(children=[2])),
        node("Hips", dict(children=[3])),
        node("Spine"),
    ]
    blob = b''.join(bin_parts)
    binname = os.path.basename(path).rsplit('.', 1)[0] + ".bin"
    doc = dict(
        asset=dict(version="2.0", generator="NifSkope WW lane HKX5 gate (c), hand-written"),
        scene=0, scenes=[dict(nodes=[0])], nodes=nodes,
        buffers=[dict(uri=binname, byteLength=len(blob))],
        bufferViews=views, accessors=accessors,
        animations=[dict(name="threebone",
                         samplers=[dict(input=a_time, output=a_rot, interpolation="LINEAR"),
                                   dict(input=a_time, output=a_tr, interpolation="STEP"),
                                   dict(input=a_time, output=a_sc, interpolation="CUBICSPLINE")],
                         channels=[dict(sampler=0, target=dict(node=2, path="rotation")),
                                   dict(sampler=1, target=dict(node=3, path="translation")),
                                   dict(sampler=2, target=dict(node=1, path="scale"))])],
    )
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    with open(path, 'w', newline='\n') as fh:
        json.dump(doc, fh, indent=1)
    open(os.path.join(os.path.dirname(path) or '.', binname), 'wb').write(blob)
    print("wrote %s (%d bytes) and %s (%d bytes)" % (path, os.path.getsize(path), binname, len(blob)))


# ------------------------------------------------------------------ expected
def expect(frame):
    """The three animated values at `frame`, worked out from the spec by hand.

    LINEAR slerp between two rotations about the SAME axis is the linear
    interpolation of the ANGLE, so the rotation is known in closed form and is
    not a second copy of the importer's slerp.
    """
    t = frame / FPS
    k = 0 if t < KEYS[1] else 1
    u = (t - KEYS[k]) / (KEYS[k + 1] - KEYS[k])
    if t >= KEYS[-1]:
        k, u = 1, 1.0
    ang = ROT_KEYS_DEG[k] + (ROT_KEYS_DEG[k + 1] - ROT_KEYS_DEG[k]) * u
    rot = qz(ang)
    tr = TRANS_KEYS[k] if u < 1.0 else TRANS_KEYS[k + 1]     # STEP holds the left key
    if t >= KEYS[-1]:
        tr = TRANS_KEYS[-1]
    # CUBICSPLINE with zero tangents: p(u) = h00*p0 + h01*p1
    u2, u3 = u * u, u * u * u
    h00 = 2 * u3 - 3 * u2 + 1
    h01 = -2 * u3 + 3 * u2
    sc = h00 * SCALE_KEYS[k] + h01 * SCALE_KEYS[k + 1]
    return rot, tr, sc


def check(tsv, tol_t=1e-4, tol_deg=1e-3, tol_s=1e-5):
    rows = {}
    bones = {}
    for line in open(tsv):
        if line.startswith('#') or not line.strip():
            continue
        p = line.rstrip('\n').split('\t')
        f, tk, b = int(p[0]), int(p[1]), int(p[2])
        if tk == -1:
            continue
        rows[(f, tk)] = tuple(float(x) for x in p[3:13])
        bones[tk] = b
    nf = max(f for f, _ in rows) + 1
    ntk = max(t for _, t in rows) + 1
    fails = []
    if nf != NFRAMES:
        fails.append("frame count is %d, expected %d" % (nf, NFRAMES))
    if ntk != 3:
        fails.append("track count is %d, expected 3" % ntk)
    # tracks are ordered by bone index; bones came from the glTF node order
    # Root, Hips, Spine, so track 0/1/2 are Root/Hips/Spine
    NAMES = ["Root", "Hips", "Spine"]
    worst = dict(t=0.0, deg=0.0, s=0.0)
    for f in range(min(nf, NFRAMES)):
        rot, tr, sc = expect(f)
        for tk in range(min(ntk, 3)):
            v = rows[(f, tk)]
            nm = NAMES[tk]
            wantT = tr if nm == "Spine" else BIND[nm]["t"]
            wantR = rot if nm == "Hips" else BIND[nm]["r"]
            wantS = (sc, sc, sc) if nm == "Root" else BIND[nm]["s"]
            for i in range(3):
                e = abs(v[i] - wantT[i])
                worst['t'] = max(worst['t'], e)
                if e > tol_t:
                    fails.append("frame %d %s translation[%d] = %.9g, expected %.9g" % (f, nm, i, v[i], wantT[i]))
            d1 = math.sqrt(sum((v[3 + i] - wantR[i]) ** 2 for i in range(4)))
            d2 = math.sqrt(sum((v[3 + i] + wantR[i]) ** 2 for i in range(4)))
            # 4*asin(d/2) is the true rotation angle; see tsvcmp.qang
            deg = 4 * math.degrees(math.asin(min(1.0, min(d1, d2) / 2.0)))
            worst['deg'] = max(worst['deg'], deg)
            if deg > tol_deg:
                fails.append("frame %d %s rotation is %.6f deg from the expected one" % (f, nm, deg))
            for i in range(3):
                e = abs(v[7 + i] - wantS[i])
                worst['s'] = max(worst['s'], e)
                if e > tol_s:
                    fails.append("frame %d %s scale[%d] = %.9g, expected %.9g" % (f, nm, i, v[7 + i], wantS[i]))
    print("gate (c) 3-bone: %d frames x %d tracks; worst |dT| %.3e (bar %.0e), worst angle %.3e deg (bar %.0e), "
          "worst |dScale| %.3e (bar %.0e)" % (nf, ntk, worst['t'], tol_t, worst['deg'], tol_deg, worst['s'], tol_s))
    if fails:
        print("gate (c) FAIL, %d checks failed; first 6:" % len(fails))
        for m in fails[:6]:
            print("   " + m)
        return False
    print("gate (c) PASS: every frame of all three interpolations matches the hand-computed value")
    return True


if __name__ == '__main__':
    if sys.argv[1] == 'write':
        write(sys.argv[2])
    else:
        sys.exit(0 if check(sys.argv[2]) else 1)
