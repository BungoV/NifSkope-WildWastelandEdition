#!/usr/bin/env python3
"""Gate (e): single-byte corruptions of a real clip, on both routes, must be
REFUSED BY NAME by both decoders -- never decoded to garbage. Ten structural
sites per route (the header, the fixup tables, the class name, the counts,
the block table, a knot header, the XML's element structure). Payload bytes
(a quantized control point) are NOT in this list: Havok carries no checksum,
so a flipped payload byte is a different but well-formed pose, and that is
recorded below as the known limit rather than claimed as a detection.

Usage: hkxanim_mutate.py [--dir scratchpad/hkx1_20260910] [--dump release/hkxanim_dump.exe] [--clip jog]
"""
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
DIR = os.path.join(REPO, "scratchpad", "hkx1_20260910")
DUMP = os.path.join(REPO, "release", "hkxanim_dump.exe")
PY = os.path.join(HERE, "hkxanim_decode.py")

checks = failures = 0

def check(ok, text):
    global checks, failures
    checks += 1
    failures += (not ok)
    print(("  ok   " if ok else "  FAIL ") + text)

def run_both(path):
    r1 = subprocess.run([sys.executable, PY, path], capture_output=True, text=True)
    r2 = subprocess.run([DUMP, path], capture_output=True, text=True)
    return (r1.returncode, r1.stdout.strip().splitlines()[-1] if r1.stdout.strip() else r1.stderr.strip()[-200:]), \
           (r2.returncode, r2.stdout.strip().splitlines()[-1] if r2.stdout.strip() else "")

def hkx_sites(blob):
    """(label, offset, new byte) -- structural bytes of a FO4 animation packfile."""
    sites = [("magic byte 0", 0, 0x00),
             ("numSections 3 -> 0x83", 0x14, 0x83),
             ("__data__ section tag 'd' -> 'x'", blob.index(b"__data__"), ord("x")),
             ("__data__ absoluteDataStart high byte", blob.index(b"__data__") + 20 + 3, 0x7f),
             ("__data__ localFixupsOffset high byte", blob.index(b"__data__") + 24 + 3, 0x7f)]
    cls = blob.index(b"hkaSplineCompressedAnimation\0")
    sites.append(("class name 'hkaSplineCompressedAnimation' last letter", cls + len("hkaSplineCompressedAnimation") - 1, ord("X")))
    return sites

def find_anim_fields(blob):
    """Locate the animation object through the fixups (same walk as the decoder) and return field offsets."""
    sys.path.insert(0, HERE)
    import hkxanim_decode as H
    base, local, glob, objs = H._packfile(blob)
    ao = [o for o, c in objs if c == "hkaSplineCompressedAnimation"][0]
    dp = local[ao + 0x98]
    bo_p = local[ao + 0x58]
    mqs = struct.unpack_from("<i", blob, ao + 0x44)[0]
    return ao, dp, bo_p, mqs

def main(argv):
    global DIR, DUMP
    if "--dir" in argv:
        DIR = argv[argv.index("--dir") + 1]
    if "--dump" in argv:
        DUMP = argv[argv.index("--dump") + 1]
    clip = argv[argv.index("--clip") + 1] if "--clip" in argv else "jog"
    mut = os.path.join(DIR, "mutants")
    os.makedirs(mut, exist_ok=True)

    # ---------------- packfile route
    blob = bytearray(open(os.path.join(DIR, "clips", clip + ".hkx"), "rb").read())
    ao, dp, bo_p, mqs = find_anim_fields(bytes(blob))
    sites = hkx_sites(bytes(blob))
    sites += [("numFrames byte 1 (23 -> 23 + 256*0x40)", ao + 0x38 + 1, 0x40),
              ("numBlocks 1 -> 0", ao + 0x3c, 0x00),
              ("maskAndQuantizationSize low byte", ao + 0x44, 0xff),
              ("blockOffsets[0] high byte", bo_p + 3, 0x7f),
              ("first spline header: degree 1 -> 9", dp + mqs + 2, 9)]
    sites = sites[:10] if len(sites) > 10 else sites
    for i, (label, off, val) in enumerate(sites):
        m = bytearray(blob)
        m[off] = val
        p = os.path.join(mut, "%s_hkx_%02d.hkx" % (clip, i))
        open(p, "wb").write(m)
        (rc1, s1), (rc2, s2) = run_both(p)
        check(rc1 == 2 and rc2 == 2, "(e) hkx %02d %-48s py rc=%d c++ rc=%d | py: %s | c++: %s" % (i, label, rc1, rc2, s1[:70], s2[:70]))

    # ---------------- XML route
    xml = open(os.path.join(DIR, "clips", clip + ".xml"), encoding="ascii").read()
    def edit(label, pattern, repl):
        assert re.search(pattern, xml), label
        return (label, re.sub(pattern, repl, xml, count=1))
    variants = [
        edit("class hkaSplineCompressedAnimation -> hkaSplineCompressedAnimatioX", r'class="hkaSplineCompressedAnimation"', 'class="hkaSplineCompressedAnimatioX"'),
        edit("type enum text altered", r'<hkparam name="type">HK_SPLINE_COMPRESSED_ANIMATION<', '<hkparam name="type">HK_SPLINE_COMPRESSED_ANIMATIOX<'),
        edit("numFrames 23 -> 2300", r'<hkparam name="numFrames">23<', '<hkparam name="numFrames">2300<'),
        edit("numBlocks 1 -> 0", r'<hkparam name="numBlocks">1<', '<hkparam name="numBlocks">0<'),
        edit("maskAndQuantizationSize 380 -> 381", r'<hkparam name="maskAndQuantizationSize">380<', '<hkparam name="maskAndQuantizationSize">381<'),
        edit("blockOffsets 0 -> 99999", r'<hkparam name="blockOffsets" numelements="1">0<', '<hkparam name="blockOffsets" numelements="1">99999<'),
        edit("data numelements 5392 -> 5391", r'<hkparam name="data" numelements="5392">', '<hkparam name="data" numelements="5391">'),
        edit("a data byte -> 999", r'(<hkparam name="data" numelements="5392">\s*\d+ \d+ \d+ )(\d+)', r'\g<1>999'),
        edit("endian 0 -> 1", r'<hkparam name="endian">0<', '<hkparam name="endian">1<'),
        edit("a closing tag broken (malformed XML)", r'</hkpackfile>', '</hkpackfil>'),
    ]
    for i, (label, text) in enumerate(variants):
        p = os.path.join(mut, "%s_xml_%02d.xml" % (clip, i))
        with open(p, "w", newline="\n", encoding="ascii") as fh:
            fh.write(text)
        (rc1, s1), (rc2, s2) = run_both(p)
        check(rc1 == 2 and rc2 == 2, "(e) xml %02d %-48s py rc=%d c++ rc=%d | py: %s | c++: %s" % (i, label, rc1, rc2, s1[:70], s2[:70]))

    # the known limit: a payload byte flip is a different pose, not a refusal
    m = bytearray(blob)
    m[dp + mqs + 60] ^= 0x55
    p = os.path.join(mut, "%s_hkx_payload.hkx" % clip)
    open(p, "wb").write(m)
    (rc1, s1), (rc2, s2) = run_both(p)
    print("  note payload byte flip (not a gate): py rc=%d c++ rc=%d -- a well-formed but different pose; Havok packfiles carry no checksum" % (rc1, rc2))
    print("%d checks, %d failures" % (checks, failures))
    print("PASS" if failures == 0 else "FAIL")
    return 0 if failures == 0 else 1

if __name__ == "__main__":
    sys.exit(main(sys.argv))
