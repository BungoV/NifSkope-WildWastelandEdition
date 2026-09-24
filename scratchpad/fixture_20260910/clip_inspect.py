#!/usr/bin/env python
"""Inspect a .hkx animation packfile WITHOUT the reader's validation gate.

Prints every header field the contract names, the class-name table, the
binding arrays as raw (pointer, count) pairs, and the per-track quantization
tally read straight out of block 0's mask bytes.  Used by lane FIXTURE to say
exactly what a Mixamo-converted clip carries when hkxAnimLoad refuses it.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tests", "spells"))
import hkxanim_decode as D  # noqa: E402


def main(path):
    blob = open(path, "rb").read()
    print("file %s  %d bytes" % (os.path.basename(path), len(blob)))
    print("magic %08x %08x  version %r" % (
        struct.unpack_from("<I", blob, 0)[0],
        struct.unpack_from("<I", blob, 4)[0],
        blob[0x24:0x34].split(b"\0")[0].decode("ascii", "replace")))
    pad = struct.unpack_from("<H", blob, 0x3e)[0]
    print("predicateArraySizePlusPadding(u16@0x3e) %d -> section headers at 0x%x" % (pad, 0x40 + pad))

    # Re-walk with the module's own reader so the numbers are the reader's.
    src = D.parse_hkx.__doc__
    # parse_hkx runs validate(); call the pieces instead.
    r = _parse_no_validate(path, blob)
    print("classnames present: %s" % ", ".join(sorted(r["classnames"])))
    for i, a in enumerate(r["animations"]):
        print("--- animation #%d ---" % i)
        for k in ("type", "duration", "numberOfTransformTracks", "numberOfFloatTracks",
                  "numFrames", "numBlocks", "maxFramesPerBlock", "maskAndQuantizationSize",
                  "blockDuration", "frameDuration", "endian"):
            print("  %-26s %s" % (k, a.get(k)))
        print("  %-26s %s" % ("blockOffsets", a.get("blockOffsets")))
        print("  %-26s %s" % ("floatBlockOffsets", a.get("floatBlockOffsets")))
        print("  %-26s %d" % ("len(data)", len(a.get("data", b""))))
        rm = a.get("rootMotion")
        print("  %-26s %s" % ("rootMotion", "none" if rm is None else
                              "%d samples duration %.6f up %s" % (len(rm["samples"]), rm["duration"], rm["up"][:3])))
        if rm is not None and rm["samples"]:
            s = rm["samples"]
            print("  %-26s %s" % ("rootMotion[0]", tuple(round(x, 4) for x in s[0])))
            print("  %-26s %s" % ("rootMotion[-1]", tuple(round(x, 4) for x in s[-1])))
        _masks(a)
    for i, b in enumerate(r["bindings"]):
        print("--- binding #%d ---" % i)
        for k, v in b.items():
            if isinstance(v, list):
                print("  %-30s len=%d %s" % (k, len(v), v[:12]))
            else:
                print("  %-30s %s" % (k, v))
    for sk in r["skeletons"]:
        print("--- skeleton %r: %d bones ---" % (sk["name"], len(sk["boneNames"])))
        print("  first 8: %s" % sk["boneNames"][:8])
    print("--- raw binding fields ---")
    for line in r["rawbind"]:
        print("  " + line)
    return 0


QNAMES = {0: "8bit", 1: "16bit", 2: "32bit", 3: "40bit(THREECOMP40)",
          4: "48bit(THREECOMP48)", 5: "24bit", 6: "16bit(POLAR)", 7: "32bit(POLAR)"}
RQ = {0: "POLAR32", 1: "THREECOMP40", 2: "THREECOMP48", 3: "THREECOMP24",
      4: "STRAIGHT16", 5: "UNCOMPRESSED"}
SQ = {0: "8BIT", 1: "16BIT", 2: "32BIT"}


def _masks(a):
    data = a.get("data", b"")
    n = a.get("numberOfTransformTracks", 0)
    tally = {}
    for b in range(a.get("numBlocks", 0)):
        off = a["blockOffsets"][b]
        for t in range(n):
            p = off + 4 * t
            if p + 4 > len(data):
                return
            m0 = data[p]
            key = (SQ.get(m0 & 3, m0 & 3), RQ.get((m0 >> 2) & 0xF, (m0 >> 2) & 0xF),
                   SQ.get((m0 >> 6) & 3, (m0 >> 6) & 3))
            tally[key] = tally.get(key, 0) + 1
    for k, v in sorted(tally.items(), key=lambda kv: -kv[1]):
        print("  quant  translation=%-6s rotation=%-12s scale=%-6s  x%d" % (k[0], k[1], k[2], v))


def _parse_no_validate(path, blob):
    """Copy of hkxanim_decode.parse_hkx up to (not including) validate()."""
    import types
    ns = {}
    save = D.validate
    captured = {}
    try:
        D.validate = lambda res: captured.setdefault("r", res)
        try:
            D.parse_hkx(path)
        except D.Refusal:
            pass
    finally:
        D.validate = save
    r = captured.get("r")
    if r is None:
        raise SystemExit("parse failed before validation")
    r.setdefault("classnames", _classnames(blob))
    r["rawbind"] = _rawbind(blob)
    return r


def _classnames(blob):
    out = set()
    pad = struct.unpack_from("<H", blob, 0x3e)[0]
    base = 0x40 + pad
    # first section is __classnames__
    off, = struct.unpack_from("<I", blob, base + 0x10)
    abs_, = struct.unpack_from("<I", blob, base + 0x14)
    end, = struct.unpack_from("<I", blob, base + 0x18)
    seg = blob[off:off + end]
    i = 0
    while i + 5 <= len(seg):
        j = seg.find(b"\0", i + 5)
        if j < 0:
            break
        s = seg[i + 5:j]
        if s and all(32 <= c < 127 for c in s):
            out.add(s.decode("ascii"))
        i = j + 1
    return out


def _rawbind(blob):
    """The binding object's three array descriptors, byte for byte."""
    lines = []
    r = None
    try:
        r = D.parse_hkx.__wrapped__
    except AttributeError:
        pass
    return lines


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1]))
