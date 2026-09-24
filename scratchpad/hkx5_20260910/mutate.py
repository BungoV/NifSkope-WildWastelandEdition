#!/usr/bin/env python3
"""GATE (d): ten corruptions of a glTF and ten of a written .hkx, each of which
must be REFUSED BY NAME, plus the FLOOR that shows the round-trip comparator
going red on a file that is structurally valid but wrong.

The glTF side is judged by OUR importer (release/hkxwrite_dump.exe import);
the .hkx side by the independent decoder (interleaved_decode.py), because lane
HKX1's C++ reader refuses every interleaved file by class name and so cannot
tell a good one from a broken one.

A Havok packfile has NO CHECKSUM (docs/HKX_ANIMATION_FORMAT.md section 8): a
flipped payload byte is a different valid pose, not a refusal.  Every .hkx
mutation below therefore targets STRUCTURE -- a header field, a count, a class
name, a fixup -- and the one payload flip is the FLOOR, expected to be accepted
by the decoder and caught by the comparator instead.

usage: python mutate.py GOOD.gltf GOOD.hkx GOODCLIP.tsv
"""
import json, os, shutil, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
EXE = os.path.join(REPO, 'release', 'hkxwrite_dump.exe')
WORK = os.path.join(HERE, 'out', 'mutants')


def run(args):
    p = subprocess.run(args, capture_output=True, text=True, cwd=REPO)
    return p.returncode, (p.stdout or '') + (p.stderr or '')


# ------------------------------------------------------------------ glTF side
def gltf_mutations(doc):
    """Each returns (name, mutated doc, the words the refusal must contain)."""
    def cp():
        return json.loads(json.dumps(doc))

    out = []

    d = cp(); d["asset"]["version"] = "1.0"
    out.append(("asset.version 1.0", d, ["asset.version", "1.0"]))

    d = cp(); d["accessors"][1]["componentType"] = 9999
    out.append(("componentType 9999", d, ["componentType", "9999"]))

    d = cp(); d["accessors"][1]["type"] = "VEC7"
    out.append(("accessor type VEC7", d, ["VEC7"]))

    d = cp(); d["accessors"][0]["count"] = 0
    out.append(("accessor count 0", d, ["count", "0"]))

    d = cp(); d["accessors"][1]["bufferView"] = 99
    out.append(("bufferView 99", d, ["bufferView", "99"]))

    d = cp(); d["accessors"][2]["count"] = 9999
    out.append(("accessor overruns its view", d, ["accessor", "bytes"]))

    d = cp(); d["animations"][0]["samplers"][0]["interpolation"] = "BEZIER"
    out.append(("interpolation BEZIER", d, ["BEZIER", "LINEAR"]))

    d = cp(); d["animations"][0]["samplers"][2]["interpolation"] = "LINEAR"
    out.append(("CUBICSPLINE output count under LINEAR", d, ["key times", "output elements"]))

    d = cp(); d["animations"][0]["channels"][0]["target"]["node"] = 42
    out.append(("channel targets node 42", d, ["targets node", "42"]))

    d = cp(); d["nodes"][0]["children"] = [1, 1]
    out.append(("one node with two parents", d, ["two parents"]))

    d = cp(); del d["animations"]
    out.append(("no animation at all", d, ["no animation"]))

    d = cp(); d["nodes"][1]["matrix"] = [1, 0, 0, 0, 0.3, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    d["nodes"][1].pop("translation", None); d["nodes"][1].pop("rotation", None); d["nodes"][1].pop("scale", None)
    out.append(("sheared node matrix", d, ["sheared"]))

    d = cp(); d["accessors"][0]["sparse"] = dict(count=1)
    out.append(("sparse accessor", d, ["sparse"]))
    return out


def gate_gltf(good):
    os.makedirs(WORK, exist_ok=True)
    doc = json.load(open(good))
    binname = doc["buffers"][0]["uri"]
    shutil.copy(os.path.join(os.path.dirname(good), binname), os.path.join(WORK, binname))
    muts = gltf_mutations(doc)
    npass = 0
    print("\n== gate (d) glTF: %d corruptions, each must be refused by name ==" % len(muts))
    for i, (name, d, words) in enumerate(muts):
        p = os.path.join(WORK, "g%02d.gltf" % i)
        json.dump(d, open(p, 'w', newline='\n'), indent=1)
        rc, out = run([EXE, 'import', p, os.path.join(WORK, "g%02d.hkx" % i), '--fps', '30'])
        line = [l for l in out.splitlines() if 'REFUSED' in l]
        refused = rc == 2 and line
        named = refused and all(w.lower() in line[0].lower() for w in words)
        ok = refused and named
        npass += 1 if ok else 0
        print("  %-42s %s  %s" % (name, "PASS" if ok else "FAIL",
                                  line[0][:130] if line else "ACCEPTED (rc=%d)" % rc))
    print("gate (d) glTF: %d/%d refused by name" % (npass, len(muts)))
    return npass, len(muts)


# ------------------------------------------------------------------- hkx side
def sec_of(blob):
    n, = struct.unpack_from('<i', blob, 20)
    sh = 0x40 + struct.unpack_from('<H', blob, 0x3e)[0]
    s = {}
    for i in range(n):
        o = sh + i * 0x40
        s[blob[o:o + 19].split(b'\0')[0].decode('latin-1')] = (o, struct.unpack_from('<7i', blob, o + 20))
    return s


def find_anim(blob):
    """Section offset of the hkaInterleavedUncompressedAnimation object."""
    s = sec_of(blob)
    cn = s['__classnames__'][1]
    names = {}
    p, end = cn[0], cn[0] + cn[1]
    while p + 5 < end and blob[p + 4] == 0x09:
        e = blob.index(b'\0', p + 5)
        names[p + 5 - cn[0]] = blob[p + 5:e].decode('latin-1')
        p = e + 1
    dt = s['__data__'][1]
    base = dt[0]
    for p in range(base + dt[3], base + dt[4] - 11, 12):
        src, _, cno = struct.unpack_from('<iii', blob, p)
        if src != -1 and names.get(cno) == 'hkaInterleavedUncompressedAnimation':
            return base, src
    raise SystemExit("the good file has no interleaved animation")


def hkx_mutations(blob):
    base, ao = find_anim(blob)
    s = sec_of(blob)
    out = []

    def m(name, edits, words):
        b = bytearray(blob)
        for off, val in edits:
            b[off:off + len(val)] = val
        out.append((name, bytes(b), words))

    m("magic byte flipped", [(3, b'\x58')], ["magic", "packfile"])
    m("fileVersion 12", [(0x0c, struct.pack('<i', 12))], ["fileversion", "12"])
    m("numSections 4", [(0x14, struct.pack('<i', 4))], ["numsections", "4"])
    m("animation type 3 (spline)", [(base + ao + 0x10, struct.pack('<i', 3))], ["type is 3", "interleaved"])
    m("numberOfTransformTracks 0", [(base + ao + 0x18, struct.pack('<i', 0))], ["numberoftransformtracks", "0"])
    m("numberOfTransformTracks 7", [(base + ao + 0x18, struct.pack('<i', 7))], ["annotationtracks", "7 transform tracks"])
    m("numberOfFloatTracks 5", [(base + ao + 0x1c, struct.pack('<i', 5))], ["numberoffloattracks", "5"])
    m("transforms size 1", [(base + ao + 0x40, struct.pack('<i', 1))], ["whole multiple"])
    m("transforms size negative", [(base + ao + 0x40, struct.pack('<i', -3))], ["transforms", "size -3"])
    m("annotationTracks size 5", [(base + ao + 0x30, struct.pack('<i', 5))], ["annotationtracks has 5"])
    # the class-name table: rename the interleaved class so nothing dispatches
    cn = s['__classnames__'][1]
    p, end = cn[0], cn[0] + cn[1]
    ren = None
    while p + 5 < end and blob[p + 4] == 0x09:
        e = blob.index(b'\0', p + 5)
        if blob[p + 5:e] == b'hkaInterleavedUncompressedAnimation':
            ren = p + 5
            break
        p = e + 1
    if ren:
        m("class renamed to hkaXnterleaved...", [(ren + 3, b'X')], ["no hkainterleaveduncompressedanimation"])
    m("__data__ virtual-fixup offset past the file",
      [(s['__data__'][0] + 20 + 12, struct.pack('<i', 0x7ffffff0))], ["__data__"])
    return out


def gate_hkx(good):
    os.makedirs(WORK, exist_ok=True)
    blob = open(good, 'rb').read()
    muts = hkx_mutations(blob)
    npass = 0
    print("\n== gate (d) .hkx: %d corruptions, each must be refused by name ==" % len(muts))
    for i, (name, b, words) in enumerate(muts):
        p = os.path.join(WORK, "h%02d.hkx" % i)
        open(p, 'wb').write(b)
        rc, out = run([sys.executable, os.path.join(HERE, 'interleaved_decode.py'), p, '--quiet'])
        line = [l for l in out.splitlines() if 'REFUSED' in l]
        refused = rc == 2 and line
        named = refused and all(w.lower() in line[0].lower() for w in words)
        ok = refused and named
        npass += 1 if ok else 0
        print("  %-42s %s  %s" % (name, "PASS" if ok else "FAIL",
                                  line[0][:130] if line else "ACCEPTED (rc=%d) %s" % (rc, out.strip()[:90])))
    print("gate (d) .hkx: %d/%d refused by name" % (npass, len(muts)))
    return npass, len(muts)


# ---------------------------------------------------------------------- floor
def floor(good, goodtsv):
    """A packfile has no checksum, so a flipped PAYLOAD byte is a valid file
    with a wrong pose.  The decoder must accept it and the comparator must go
    RED -- which is the proof that the round-trip comparison can fail at all
    (CONSTITUTION rule 4)."""
    import interleaved_decode as dec
    base, ao = find_anim(open(good, 'rb').read())
    blob = bytearray(open(good, 'rb').read())
    s = sec_of(bytes(blob))
    dt = s['__data__'][1]
    # transforms payload = the local fixup from the object's +0x38
    trp = None
    for p in range(dt[0] + dt[1], dt[0] + dt[2] - 7, 8):
        src, dst = struct.unpack_from('<ii', blob, p)
        if src == ao + 0x38:
            trp = dst
    assert trp is not None, "no local fixup for transforms"
    # nudge the X of frame 0, track 0 by 1.0 unit
    off = dt[0] + trp
    v, = struct.unpack_from('<f', blob, off)
    struct.pack_into('<f', blob, off, v + 1.0)
    p = os.path.join(WORK, "floor_one_unit.hkx")
    open(p, 'wb').write(bytes(blob))
    rc, out = run([sys.executable, os.path.join(HERE, 'interleaved_decode.py'), p,
                   '--out', os.path.join(WORK, 'floor.tsv'), '--quiet'])
    accepted = rc == 0
    rc2, out2 = run([sys.executable, os.path.join(HERE, 'tsvcmp.py'), os.path.abspath(goodtsv),
                     os.path.join(WORK, 'floor.tsv'), '--label', 'FLOOR (one unit on frame 0 track 0)'])
    print("\n== the floor ==")
    print("  decoder accepted the flipped payload: %s (a packfile has no checksum)" % ("yes" if accepted else "NO -- unexpected"))
    print("  " + out2.strip().replace("\n", "\n  "))
    # a CRASH is not a red gate: the comparator must have run and said FAIL
    red = rc2 == 1 and "FAIL" in out2 and "Traceback" not in out2
    print("  comparator went red: %s" % ("yes -- the round-trip gate CAN fail" if red else "NO -- the gate proves nothing"))
    return accepted and red


if __name__ == '__main__':
    g, h, tsv = sys.argv[1], sys.argv[2], sys.argv[3]
    a, an = gate_gltf(g)
    b, bn = gate_hkx(h)
    f = floor(h, tsv)
    print("\ngate (d): glTF %d/%d, .hkx %d/%d, floor %s" % (a, an, b, bn, "PASS" if f else "FAIL"))
    sys.exit(0 if (a == an and b == bn and f) else 1)
