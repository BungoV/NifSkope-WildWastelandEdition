#!/usr/bin/env python
"""Raw object table + hkaAnimationBinding byte dump for a Havok 2014 animation
packfile, using the repo decoder's own walker (tests/spells/hkxanim_decode.py).

Answers from the BYTES, not from the reader's model: which classes the file
carries, where each object sits, and what the binding's three array
descriptors actually contain -- so an empty transformTrackToBoneIndices can be
told from a parse failure.  Also tallies the root-motion samples.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tests", "spells"))
import hkxanim_decode as D  # noqa: E402


def main(path):
    blob = open(path, "rb").read()
    base, local, glob, objs = D._packfile(blob)
    print("file %s  %d bytes   __data__ base 0x%x" % (os.path.basename(path), len(blob), base))
    print("objects (%d):" % len(objs))
    for off, nm in objs:
        print("   +0x%06x  %s" % (off - base, nm))
    print("local fixups %d, global fixups %d" % (len(local), len(glob)))

    for off, nm in objs:
        if nm != "hkaAnimationBinding":
            continue
        print("--- hkaAnimationBinding at +0x%x ---" % (off - base))
        print("  bytes +0x00..0x58:")
        for r in range(0, 0x58, 16):
            print("    +%02x  %s" % (r, blob[off + r:off + r + 16].hex(" ")))
        for fld, at in (("originalSkeletonName", 0x10), ("animation", 0x18),
                        ("transformTrackToBoneIndices", 0x20),
                        ("floatTrackToFloatSlotIndices", 0x30),
                        ("partitionIndices", 0x40)):
            a = off + at
            ptr, cnt = struct.unpack_from("<qi", blob, a)
            cap = struct.unpack_from("<I", blob, a + 12)[0]
            print("  +0x%02x %-30s rawptr=0x%x count=%d capflags=0x%08x local->%s global->%s"
                  % (at, fld, ptr & 0xFFFFFFFFFFFFFFFF, cnt, cap,
                     ("+0x%x" % (local[a] - base)) if a in local else "-",
                     ("+0x%x" % (glob[a] - base)) if a in glob else "-"))
        print("  +0x50 blendHint = %d" % blob[off + 0x50])

    # root motion movement
    for off, nm in objs:
        if nm != "hkaDefaultAnimatedReferenceFrame":
            continue
        rp, rn = D._arr(blob, local, off + 0x20, 16)
        if rp is None:
            continue
        xs = [struct.unpack_from("<4f", blob, rp + 16 * j) for j in range(rn)]
        mx = max(max(abs(v) for v in s) for s in xs) if xs else 0.0
        print("--- hkaDefaultAnimatedReferenceFrame: %d samples, max |component| = %.6f ---" % (rn, mx))
        print("    up      %s" % (struct.unpack_from("<4f", blob, off + 0x10),))
        print("    forward %s" % (struct.unpack_from("<4f", blob, off + 0x30),))
        print("    first   %s" % (tuple(round(v, 5) for v in xs[0]),))
        print("    last    %s" % (tuple(round(v, 5) for v in xs[-1]),))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1]))
