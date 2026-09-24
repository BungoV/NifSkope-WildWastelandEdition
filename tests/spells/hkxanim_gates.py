#!/usr/bin/env python3
"""Lane HKX1's pre-registered gates over the reader (src/hkxanim.cpp via
release/hkxanim_dump.exe) and the oracle (tests/spells/hkxanim_decode.py).

  (a) C++ vs Python, every bone every frame: translation <= 1e-4, rotation
      <= 0.01 deg, scale exact -- on both routes (xml, packfile)
  (b) skeleton.hkx reference pose vs skeleton.nif NiNode bind pose: bone-name
      sets (differences listed), parents by name, local transforms <= 1e-3
  (c) the T-pose clip's frame 0 vs the hkaSkeleton reference pose, per bone
      (translation <= 1e-3, quaternion components <= 1e-3)
  (f) frame count and duration vs the file's header
  (g) block-overlap frames decode identically from both blocks
Gates (d) synthetic and (e) mutation live in hkxanim_synthetic.py and
hkxanim_mutate.py.

Usage: hkxanim_gates.py [--dump release/hkxanim_dump.exe] [--dir scratchpad/hkx1_20260910]
Prints one line per check and `N checks, M failures` then PASS/FAIL.
"""
import math
import os
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hkxanim_decode as H  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
DUMP = os.path.join(REPO, "release", "hkxanim_dump.exe")
DIR = os.path.join(REPO, "scratchpad", "hkx1_20260910")
NIF = r"E:\Tools\Fallout 4\DataUnpacked\Data\meshes\actors\character\CharacterAssets\skeleton.nif"
CLIPS = ["tpose_idle", "jog", "turn", "twoblock", "q48"]

checks = 0
failures = 0

def check(ok, text):
    global checks, failures
    checks += 1
    if not ok:
        failures += 1
    print(("  ok   " if ok else "  FAIL ") + text)

def qangle(q1, q2):
    """Angle between two unit quaternions, sign-free, exact for tiny differences
    (acos(dot) loses everything below ~0.03 deg to rounding)."""
    dm = math.sqrt(sum((x - y) ** 2 for x, y in zip(q1, q2)))
    dp = math.sqrt(sum((x + y) ** 2 for x, y in zip(q1, q2)))
    d = min(dm, dp)
    return math.degrees(2.0 * math.asin(min(1.0, d / 2.0)))

def read_tsv(path):
    rows = {}
    motion = {}
    for line in open(path):
        if line.startswith("#"):
            continue
        p = line.rstrip("\n").split("\t")
        f, t = int(p[0]), int(p[1])
        if t == -1:
            motion[f] = tuple(float(x) for x in p[3:7])
        else:
            rows[(f, t)] = (int(p[2]), tuple(float(x) for x in p[3:13]))
    return rows, motion

# ------------------------------------------------------------------ (a)
def gate_a(clip):
    for route in ("xml", "hkx"):
        src = os.path.join(DIR, "clips", clip + "." + route)
        py = os.path.join(DIR, "gate_py_%s_%s.tsv" % (clip, route))
        cc = os.path.join(DIR, "gate_cc_%s_%s.tsv" % (clip, route))
        r1 = subprocess.run([sys.executable, os.path.join(HERE, "hkxanim_decode.py"), src, "--out", py, "--block-overlap"],
                            capture_output=True, text=True)
        r2 = subprocess.run([DUMP, src, "--out", cc], capture_output=True, text=True)
        check(r1.returncode == 0 and r2.returncode == 0, "(a) %s %s: both decoders ran (py rc=%d, c++ rc=%d)" % (clip, route, r1.returncode, r2.returncode))
        if r1.returncode or r2.returncode:
            print(r1.stdout[-300:], r2.stdout[-300:])
            continue
        h1 = r1.stdout.splitlines()[0]
        h2 = r2.stdout.splitlines()[0]
        check(h1 == h2, "(a) %s %s: header lines identical" % (clip, route))
        a, ma = read_tsv(py)
        b, mb = read_tsv(cc)
        check(set(a) == set(b) and len(a) > 0, "(a) %s %s: same (frame, track) set, %d rows" % (clip, route, len(a)))
        wt = wr = ws = 0.0
        bone_mismatch = 0
        for k in a:
            if k not in b:
                continue
            (ba, va), (bb, vb) = a[k], b[k]
            bone_mismatch += (ba != bb)
            wt = max(wt, max(abs(va[i] - vb[i]) for i in range(3)))
            wr = max(wr, qangle(va[3:7], vb[3:7]))
            ws = max(ws, max(abs(va[i] - vb[i]) for i in range(7, 10)))
        check(bone_mismatch == 0, "(a) %s %s: track->bone identical on every row" % (clip, route))
        check(wt <= 1e-4, "(a) %s %s: worst translation difference %.3g <= 1e-4" % (clip, route, wt))
        check(wr <= 0.01, "(a) %s %s: worst rotation difference %.4f deg <= 0.01" % (clip, route, wr))
        check(ws == 0.0, "(a) %s %s: scale identical (worst %.3g)" % (clip, route, ws))
        wm = 0.0
        for f in ma:
            wm = max(wm, max(abs(x - y) for x, y in zip(ma[f], mb.get(f, (9e9,) * 4))))
        check(len(ma) == len(mb) and wm <= 1e-6, "(a) %s %s: root motion %d frames identical (worst %.3g)" % (clip, route, len(ma), wm))
        # (g) from the C++ diagnostics line
        for line in r2.stdout.splitlines():
            if line.startswith("route "):
                parts = line.split()
                ov = int(parts[parts.index("blockOverlapFrames") + 1])
                worst = float(parts[parts.index("blockOverlapWorst") + 1])
                dev = float(parts[parts.index("worstQuatLengthDeviation") + 1])
                if clip in ("twoblock", "q48"):
                    check(ov >= 1 and worst <= 1e-3, "(g) %s %s: %d block-boundary frames decoded from both blocks, worst component difference %.3g <= 1e-3" % (clip, route, ov, worst))
                check(dev <= 0.05, "(a) %s %s: worst spline quaternion length deviation %.3g <= 0.05" % (clip, route, dev))
            if line.startswith("block ") and "walk ended" in line:
                p = line.split()
                check(p[4] == p[8].rstrip(","), "(f) %s %s: %s" % (clip, route, line))
    return

# ------------------------------------------------------------------ (f)
def gate_f(clip):
    r = H.parse_xml(os.path.join(DIR, "clips", clip + ".xml"))
    a = r["animations"][0]
    frames, _ = H.decode(a)
    check(len(frames) == a["numFrames"], "(f) %s: decoded %d frames, header numFrames %d" % (clip, len(frames), a["numFrames"]))
    check(abs(a["duration"] - (a["numFrames"] - 1) * a["frameDuration"]) <= 1e-5 * max(1.0, a["duration"]),
          "(f) %s: duration %.6f == (numFrames-1)*frameDuration %.6f" % (clip, a["duration"], (a["numFrames"] - 1) * a["frameDuration"]))
    if a["rootMotion"]:
        check(len(a["rootMotion"]["samples"]) == a["numFrames"] and abs(a["rootMotion"]["duration"] - a["duration"]) <= 1e-6,
              "(f) %s: %d root-motion samples, motion duration %.6f" % (clip, len(a["rootMotion"]["samples"]), a["rootMotion"]["duration"]))

# ------------------------------------------------------------------ the NIF skeleton (gate b)
def read_nif_nodes(path):
    """NiNode/BSFadeNode blocks of a NIF 20.2.0.7 BSVER 130 file: name, parent name, translation, 3x3 rotation, scale."""
    data = open(path, "rb").read()
    nl = data.index(b"\x0a")
    pos = nl + 1
    ver, = struct.unpack_from("<I", data, pos); pos += 4
    pos += 1
    uver, = struct.unpack_from("<I", data, pos); pos += 4
    nb, = struct.unpack_from("<I", data, pos); pos += 4
    bsver, = struct.unpack_from("<I", data, pos); pos += 4
    if ver != 0x14020007 or bsver != 130:
        raise RuntimeError("not a FO4 NIF: version %#x bsver %d" % (ver, bsver))
    for _ in range(4):
        slen = data[pos]; pos += 1 + slen
    ntypes = struct.unpack_from("<H", data, pos)[0]; pos += 2
    btypes = []
    for _ in range(ntypes):
        slen = struct.unpack_from("<I", data, pos)[0]; pos += 4
        btypes.append(data[pos:pos + slen].decode()); pos += slen
    tidx = list(struct.unpack_from("<%dH" % nb, data, pos)); pos += nb * 2
    bsize = list(struct.unpack_from("<%dI" % nb, data, pos)); pos += nb * 4
    nstr = struct.unpack_from("<I", data, pos)[0]; pos += 4
    pos += 4
    strings = []
    for _ in range(nstr):
        slen = struct.unpack_from("<I", data, pos)[0]; pos += 4
        strings.append(data[pos:pos + slen].decode("latin-1")); pos += slen
    ngrp = struct.unpack_from("<I", data, pos)[0]; pos += 4 + ngrp * 4
    nodes = {}
    children_of = {}
    off = pos
    for i in range(nb):
        tname = btypes[tidx[i]]
        if tname in ("NiNode", "BSFadeNode"):
            p = off
            nameIdx, = struct.unpack_from("<I", data, p); p += 4
            nExtra, = struct.unpack_from("<I", data, p); p += 4 + 4 * nExtra
            p += 4  # controller
            p += 4  # flags (u32 at BSVER 130)
            t = struct.unpack_from("<3f", data, p); p += 12
            m = struct.unpack_from("<9f", data, p); p += 36
            s, = struct.unpack_from("<f", data, p); p += 4
            p += 4  # collision
            nCh, = struct.unpack_from("<I", data, p); p += 4
            ch = list(struct.unpack_from("<%dI" % nCh, data, p)); p += 4 * nCh
            if p - off != bsize[i]:
                raise RuntimeError("block %d %s: parsed %d bytes, block size %d" % (i, tname, p - off, bsize[i]))
            name = strings[nameIdx] if nameIdx != 0xFFFFFFFF else ""
            nodes[i] = dict(name=name, t=t, m=m, s=s, children=ch, parent=None)
        off += bsize[i]
    for i, n in nodes.items():
        for c in n["children"]:
            if c in nodes:
                nodes[c]["parent"] = n["name"]
    return {n["name"]: n for n in nodes.values()}

def quat_to_mat(q):
    x, y, z, w = q
    return (1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
            2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
            2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y))

def transpose(m):
    return (m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8])

def gate_b():
    root = ET.parse(os.path.join(DIR, "clips", "skeleton.xml")).getroot()
    skels = [H._skeleton_from_xml(o) for o in root.iter("hkobject") if o.get("class") == "hkaSkeleton"]
    S = [s for s in skels if s["name"] == "Root"][0]
    nif = read_nif_nodes(NIF)
    hk = set(S["boneNames"])
    nf = set(nif)
    only_hk = sorted(hk - nf)
    only_nif = sorted(nf - hk)
    print("       exact-case: hkx bones without a NiNode of the same spelling: %d (%s)" % (len(only_hk), ", ".join(only_hk)))
    lower = {n.lower(): n for n in nif}
    case_only = sorted(n for n in only_hk if n.lower() in lower)
    truly_missing = sorted(n for n in only_hk if n.lower() not in lower)
    print("       case-only differences (hkx -> nif): %s" % ", ".join("%s->%s" % (n, lower[n.lower()]) for n in case_only))
    check(not truly_missing, "(b) every hkaSkeleton bone exists as a NiNode in skeleton.nif, case-insensitively (missing: %s)" % (truly_missing or "none"))
    nif = {lower.get(k.lower(), k): v for k, v in nif.items()}
    nif.update({n.lower(): v for n, v in list(nif.items())})
    nif = {**{k.lower(): v for k, v in nif.items()}}
    S = dict(S)
    S["boneNames"] = [n.lower() for n in S["boneNames"]]
    nifp = {}
    for k, v in nif.items():
        v = dict(v)
        v["parent"] = v["parent"].lower() if v["parent"] else v["parent"]
        nifp[k] = v
    nif = nifp
    print("       NiNodes not in the hkaSkeleton: %d (%s%s)" % (len(only_nif), ", ".join(only_nif[:8]), ", ..." if len(only_nif) > 8 else ""))
    parent_bad = []
    wt = 0.0
    wt_bone = None
    wr_direct = 0.0
    wr_transposed = 0.0
    ws = 0.0
    for i, name in enumerate(S["boneNames"]):
        if name not in nif:
            continue
        n = nif[name]
        hp = S["boneNames"][S["parents"][i]] if S["parents"][i] >= 0 else None
        np_ = n["parent"]
        if np_ == "camtargetparent" and hp == "root":
            np_ = "root"  # the NIF interposes CamTargetParent; the hkx parents CamTarget to Root
        if hp != np_ and not (hp is None and np_ == "skeleton.nif"):
            parent_bad.append((name, hp, np_))
        rt, rq, rs = S["referencePose"][i]
        dt = max(abs(a - b) for a, b in zip(rt[:3], n["t"]))
        if dt > wt:
            wt, wt_bone = dt, (name, rt[:3], n["t"])
        m = quat_to_mat(rq)
        wr_direct = max(wr_direct, max(abs(a - b) for a, b in zip(m, n["m"])))
        wr_transposed = max(wr_transposed, max(abs(a - b) for a, b in zip(transpose(m), n["m"])))
        ws = max(ws, max(abs(x - n["s"]) for x in rs[:3]))
    check(not parent_bad, "(b) parents agree by name on every shared bone (%d disagree: %s)" % (len(parent_bad), parent_bad[:4]))
    check(wt <= 1e-3, "(b) worst reference-pose translation vs NiNode translation %.3g <= 1e-3 (bone %s)" % (wt, wt_bone))
    conv = "hkx quaternion -> matrix equals the NIF matrix" if wr_direct <= wr_transposed else "hkx quaternion -> matrix equals the NIF matrix TRANSPOSED"
    check(min(wr_direct, wr_transposed) <= 1e-3, "(b) worst rotation-matrix element difference %.3g (direct) / %.3g (transposed) <= 1e-3; %s" % (wr_direct, wr_transposed, conv))
    check(ws <= 1e-3, "(b) worst scale difference %.3g <= 1e-3" % ws)
    return S

# ------------------------------------------------------------------ (c)
def gate_c(S):
    r = H.parse_xml(os.path.join(DIR, "clips", "tpose_idle.xml"))
    a = r["animations"][0]
    bind = r["bindings"][0]
    frames, _ = H.decode(a)
    rows = []
    for t, (tr, q, sc, _) in enumerate(frames[0]):
        b = bind["transformTrackToBoneIndices"][t]
        rt, rq, rs = S["referencePose"][b]
        dt = max(abs(x - y) for x, y in zip(tr, rt[:3]))
        dq = min(max(abs(x - y) for x, y in zip(q, rq)), max(abs(x + y) for x, y in zip(q, rq)))
        ds = max(abs(x - y) for x, y in zip(sc, rs[:3]))
        rows.append((S["boneNames"][b], dt, dq, ds))
    match = [r for r in rows if r[1] <= 1e-3 and r[2] <= 1e-3 and r[3] <= 1e-3]
    diff = [r for r in rows if not (r[1] <= 1e-3 and r[2] <= 1e-3 and r[3] <= 1e-3)]
    check(len(match) + len(diff) == a["numberOfTransformTracks"], "(c) %d tracks compared against the reference pose" % len(rows))
    print("       bones equal to the bind pose within 1e-3 (t, q, s): %d of %d" % (len(match), len(rows)))
    print("       bones that differ: %d -- %s" % (len(diff), ", ".join("%s(t%.2g,q%.2g)" % (n, dt, dq) for n, dt, dq, ds in diff[:12]) + (", ..." if len(diff) > 12 else "")))
    check(len(match) == len(rows), "(c) T-pose clip frame 0 == reference pose on every bone (PRE-REGISTERED; the furniture 'Tpose' clip is not the bind pose on %d bones, see the report)" % len(diff))
    # the refuter for the decoder: on the matched bones the agreement is at quantization precision
    if match:
        wq = max(r[2] for r in match)
        check(wq <= 1e-3, "(c) on the %d matching bones the worst quaternion component difference is %.3g (40-bit step 3.5e-4)" % (len(match), wq))
    return rows

def main(argv):
    global DUMP, DIR
    if "--dump" in argv:
        DUMP = argv[argv.index("--dump") + 1]
    if "--dir" in argv:
        DIR = argv[argv.index("--dir") + 1]
    print("hkxanim gates: dump=%s dir=%s" % (DUMP, DIR))
    for c in CLIPS:
        gate_a(c)
        gate_f(c)
    S = gate_b()
    gate_c(S)
    # skeleton.hkx through both decoders
    r = subprocess.run([DUMP, os.path.join(DIR, "clips", "skeleton.hkx")], capture_output=True, text=True)
    check(r.returncode == 0 and "skeleton Root bones 95" in r.stdout and "skeleton Ragdoll_NPC COM bones 18" in r.stdout,
          "(b) C++ reader loads skeleton.hkx: %s" % " | ".join(r.stdout.splitlines()[:2]))
    # the lossless refusal
    for route in ("xml", "hkx"):
        r = subprocess.run([DUMP, os.path.join(DIR, "clips", "lossless." + route)], capture_output=True, text=True)
        check(r.returncode == 2 and "hkaLosslessCompressedAnimation" in r.stdout, "(refusal) lossless %s: %s" % (route, r.stdout.strip()[:100]))
    print("%d checks, %d failures" % (checks, failures))
    print("PASS" if failures == 0 else "FAIL")
    return 0 if failures == 0 else 1

if __name__ == "__main__":
    sys.exit(main(sys.argv))
