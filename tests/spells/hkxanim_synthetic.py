#!/usr/bin/env python3
"""Gate (d): a known-answer clip built by hand -- one bone, a static translation
of (1.5, 0, 0) and a rotation of exactly 90 degrees about Z over 10 frames,
written as an HKXPACK-style XML (cloned from the real T-pose unpack so every
other object is genuine), packed to .hkx by HKXPACK, and decoded by BOTH
decoders on BOTH routes. Expected: frame i has yaw 10*i degrees within 0.05
(the 40-bit quantization step is 3.5e-4 per component, ~0.03 deg), frame 9 is
90.00, translation (1.5, 0, 0) on every frame, scale (1, 1, 1).

Usage: hkxanim_synthetic.py [--dir scratchpad/hkx1_20260910] [--dump release/hkxanim_dump.exe]
"""
import math
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hkxanim_decode as H  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
DIR = os.path.join(REPO, "scratchpad", "hkx1_20260910")
DUMP = os.path.join(REPO, "release", "hkxanim_dump.exe")
JAR = r"E:\Tools\Fallout 4\HKXPACK\hkxpack-cli.jar"

checks = failures = 0

def check(ok, text):
    global checks, failures
    checks += 1
    failures += (not ok)
    print(("  ok   " if ok else "  FAIL ") + text)

def pack40(q):
    """Inverse of docs/HKX_ANIMATION_FORMAT.md 4.6 (THREECOMP40)."""
    missing = max(range(4), key=lambda i: abs(q[i]))
    rest = [q[i] for i in range(4) if i != missing]
    f = math.sqrt(0.5) / 2047.0
    vals = [min(4095, max(0, int(round(x / f + 2047)))) for x in rest]
    v = vals[0] | (vals[1] << 12) | (vals[2] << 24) | (missing << 36) | ((1 if q[missing] < 0 else 0) << 38)
    return v.to_bytes(5, "little")

def build_data(n_frames):
    data = bytearray([0x45, 0x01, 0xF0, 0x00])          # 16-bit / THREECOMP40 / 16-bit; static X; spline rotation
    data += struct.pack("<f", 1.5)                       # static translation X (align 4 holds: 4)
    # rotation: u16 n, u8 degree, knots n+degree+2, then (n+1) packed quaternions
    n = n_frames - 1
    data += struct.pack("<HB", n, 1)
    data += bytes([0] + list(range(n + 1)) + [n])
    for i in range(n_frames):
        th = math.radians(90.0 * i / n)
        data += pack40((0.0, 0.0, math.sin(th / 2), math.cos(th / 2)))
    while len(data) % 4:
        data.append(0)
    return bytes(data)

def build_xml(template_path, n_frames, fd):
    x = open(template_path, encoding="ascii").read()
    data = build_data(n_frames)
    dur = (n_frames - 1) * fd
    def setp(name, value, s):
        pat = re.compile(r'(<hkparam name="%s">)[^<]*(</hkparam>)' % name)
        assert len(pat.findall(s)) >= 1, name
        return pat.sub(lambda m: m.group(1) + value + m.group(2), s, count=1)
    # the animation object
    a0 = x.index('class="hkaSplineCompressedAnimation"')
    a1 = x.index("</hkobject>", x.index('<hkparam name="endian">', a0))
    anim = x[a0:a1]
    anim = setp("duration", repr(dur), anim)
    anim = setp("numberOfTransformTracks", "1", anim)
    anim = anim[:anim.index('<hkparam name="annotationTracks"')] + \
        '<hkparam name="annotationTracks" numelements="1">\n<hkobject>\n<hkparam name="trackName"/>\n<hkparam name="annotations" numelements="0"/>\n</hkobject>\n</hkparam>\n' + \
        anim[anim.index('<hkparam name="numFrames">'):]
    anim = setp("numFrames", str(n_frames), anim)
    anim = setp("numBlocks", "1", anim)
    anim = setp("maxFramesPerBlock", "256", anim)
    anim = setp("maskAndQuantizationSize", "4", anim)
    anim = setp("blockDuration", repr(255 * fd), anim)
    anim = setp("blockInverseDuration", repr(1.0 / (255 * fd)), anim)
    anim = setp("frameDuration", repr(fd), anim)
    anim = re.sub(r'<hkparam name="blockOffsets" numelements="\d+">[^<]*<', '<hkparam name="blockOffsets" numelements="1">0<', anim)
    anim = re.sub(r'<hkparam name="floatBlockOffsets" numelements="\d+">[^<]*<', '<hkparam name="floatBlockOffsets" numelements="1">%d<' % len(data), anim)
    anim = re.sub(r'<hkparam name="data" numelements="\d+">[^<]*<', '<hkparam name="data" numelements="%d">%s<' % (len(data), " ".join(str(b) for b in data)), anim)
    x = x[:a0] + anim + x[a1:]
    # the reference frame: n_frames zero samples
    m0 = x.index('class="hkaDefaultAnimatedReferenceFrame"')
    seg = x[m0:x.index("</hkobject>", m0)]
    seg2 = setp("duration", repr(dur), seg)
    seg2 = re.sub(r'<hkparam name="referenceFrameSamples" numelements="\d+">[^<]*<',
                  '<hkparam name="referenceFrameSamples" numelements="%d">%s<' % (n_frames, " ".join("(0.0 0.0 0.0 0.0)" for _ in range(n_frames))), seg2)
    x = x.replace(seg, seg2)
    # the binding: one track -> bone 0
    b0 = x.index('class="hkaAnimationBinding"')
    seg = x[b0:x.index("</hkobject>", b0)]
    seg2 = re.sub(r'<hkparam name="transformTrackToBoneIndices" numelements="\d+">[^<]*<',
                  '<hkparam name="transformTrackToBoneIndices" numelements="1">0<', seg)
    x = x.replace(seg, seg2)
    return x

def yaw_deg(q):
    x, y, z, w = q
    return math.degrees(2.0 * math.atan2(z, w))

def decode_rows(path):
    if path.endswith(".xml"):
        r = H.parse_xml(path)
    else:
        r = H.parse_hkx(path)
    frames, _ = H.decode(r["animations"][0])
    return [(tr, q, sc) for (tr, q, sc, _) in (f[0] for f in frames)]

def read_cc(path):
    out = os.path.join(DIR, "synthetic_cc_%s.tsv" % os.path.basename(path).replace(".", "_"))
    r = subprocess.run([DUMP, path, "--out", out], capture_output=True, text=True)
    if r.returncode != 0:
        return None, r.stdout
    rows = []
    for line in open(out):
        if line.startswith("#"):
            continue
        p = line.rstrip("\n").split("\t")
        if int(p[1]) == -1:
            continue
        v = [float(t) for t in p[3:13]]
        rows.append((tuple(v[0:3]), tuple(v[3:7]), tuple(v[7:10])))
    return rows, r.stdout

def judge(label, rows, n_frames):
    if rows is None:
        check(False, "%s: refused" % label)
        return
    check(len(rows) == n_frames, "%s: %d frames decoded" % (label, len(rows)))
    worst = 0.0
    for i, (tr, q, sc) in enumerate(rows):
        expect = 90.0 * i / (n_frames - 1)
        worst = max(worst, abs(yaw_deg(q) - expect))
        if i == n_frames - 1:
            check(abs(yaw_deg(q) - 90.0) <= 0.05, "%s: last frame yaw %.4f deg (expected 90.00 +- 0.05)" % (label, yaw_deg(q)))
        if i == 0:
            check(abs(yaw_deg(q)) <= 0.05, "%s: first frame yaw %.4f deg (expected 0.00 +- 0.05)" % (label, yaw_deg(q)))
    check(worst <= 0.05, "%s: worst yaw error over %d frames %.4f deg <= 0.05" % (label, n_frames, worst))
    check(all(abs(tr[0] - 1.5) <= 1e-6 and tr[1] == 0.0 and tr[2] == 0.0 for tr, q, sc in rows), "%s: translation (1.5, 0, 0) on every frame" % label)
    check(all(sc == (1.0, 1.0, 1.0) for tr, q, sc in rows), "%s: scale (1, 1, 1) on every frame" % label)
    check(all(abs(q[0]) <= 1e-3 and abs(q[1]) <= 1e-3 for tr, q, sc in rows), "%s: no x/y component beyond quantization" % label)

def main(argv):
    global DIR, DUMP
    if "--dir" in argv:
        DIR = argv[argv.index("--dir") + 1]
    if "--dump" in argv:
        DUMP = argv[argv.index("--dump") + 1]
    n_frames = 10
    fd = 1.0 / 30.0
    xml = build_xml(os.path.join(DIR, "clips", "tpose_idle.xml"), n_frames, fd)
    xml_path = os.path.join(DIR, "synthetic.xml")
    hkx_path = os.path.join(DIR, "synthetic.hkx")
    with open(xml_path, "w", newline="\n", encoding="ascii") as fh:
        fh.write(xml)
    print("wrote %s (%d bytes of data in the clip)" % (xml_path, len(build_data(n_frames))))
    r = subprocess.run(["java", "-jar", JAR, "pack", xml_path, "-o", hkx_path], capture_output=True, text=True)
    check(r.returncode == 0 and os.path.exists(hkx_path), "HKXPACK packed the synthetic XML (rc=%d) %s" % (r.returncode, (r.stdout + r.stderr).strip()[-120:]))
    try:
        judge("python xml", decode_rows(xml_path), n_frames)
        if os.path.exists(hkx_path):
            judge("python hkx", decode_rows(hkx_path), n_frames)
    except H.Refusal as e:
        check(False, "python refused: %s" % e)
    rows, txt = read_cc(xml_path)
    judge("c++ xml", rows, n_frames)
    if os.path.exists(hkx_path):
        rows, txt = read_cc(hkx_path)
        judge("c++ hkx", rows, n_frames)
    print("%d checks, %d failures" % (checks, failures))
    print("PASS" if failures == 0 else "FAIL")
    return 0 if failures == 0 else 1

if __name__ == "__main__":
    sys.exit(main(sys.argv))
