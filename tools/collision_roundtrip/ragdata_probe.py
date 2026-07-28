"""hknpRagdollData: find every hkArray, where it points, and how the size follows.

The root object of a ragdoll. 37 instances, 23 distinct sizes, so it scales with
body/bone count like hkaSkeleton did.

Scans the WHOLE header and reports raw hex, not formatted floats. Both of those are
in response to real misses today: a probe that stopped at the end of what it already
understood missed an AABB in the compound and four negative zeros in the skeleton,
and %.4f formatting hid a scale of 0.99999994.
"""
import os
import struct
import sys
from collections import Counter, defaultdict

# hkpack.py is the scratch packfile reader; point at it with HKPACK_DIR if this
# script is run from the repo rather than beside it.
sys.path.insert(0, os.environ.get("HKPACK_DIR", os.path.dirname(os.path.abspath(__file__))))
from hkpack import Pack, extract, skeletons


def main():
    tmp = os.path.abspath("_rg.bin")
    rows = []
    words = defaultdict(Counter)

    for nif in skeletons():
        blob = extract(nif, tmp)
        if not blob:
            continue
        try:
            pk = Pack(blob)
        except Exception:
            continue
        for off, cls in sorted(pk.virt):
            if cls != "hknpRagdollData":
                continue
            size = pk.objEnd(off) - off

            # every hkArray-looking descriptor: count at o, count|0x80000000 at o+4,
            # pointer 8 bytes before, patched by a local fixup
            arrays = []
            for o in range(0x08, 0x100, 8):
                cnt = pk.u32(off + o)
                cap = pk.u32(off + o + 4)
                if cap == (cnt | 0x80000000) and 0 < cnt < 8192:
                    tgt = pk.local.get(off + o - 8)
                    arrays.append((o - 8, cnt, (tgt - off) if tgt is not None else None))

            # global fixups leaving the object, by source offset
            glo = sorted((s - off, pk.objclass.get(d, "?"))
                         for s, d in pk.glob.items() if off <= s < off + size)
            rows.append((os.path.basename(nif), size, arrays, glo))
            for o in range(0, min(size, 0x100), 4):
                words[o][pk.raw(off + o, 4).hex()] += 1

    print("%d hknpRagdollData objects\n" % len(rows))
    print("%-22s %6s  arrays (descOff, count, payloadOff)" % ("file", "size"))
    for name, size, arrays, glo in rows[:8]:
        print("%-22s %6d  %s" % (name[:22], size,
              " ".join("(%#x,%d,%s)" % (a, c, ("%#x" % p) if p is not None else "-")
                       for a, c, p in arrays)))
    print()
    print("global fixups out of the first object:")
    for o, cls in rows[0][3][:8]:
        print("   +%#06x -> %s" % (o, cls))
    print("   ... %d total" % len(rows[0][3]))

    print("\nheader words +0x00..+0xff that are not constant zero:")
    for o in sorted(words):
        c = words[o]
        if len(c) == 1 and list(c)[0] == "00000000":
            continue
        tag = "CONST " + list(c)[0] if len(c) == 1 else "%d distinct" % len(c)
        print("   +0x%02x  %s" % (o, tag))


if __name__ == "__main__":
    main()
