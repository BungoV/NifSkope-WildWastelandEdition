"""Packfile-LEVEL layout: what the assembler has to reproduce around the objects.

Every individual object now writes byte-exact. What is not measured is the file
they sit in: the class-name table, the order objects appear in __data__, whether
the fixup tables are sorted, how much padding sits between objects, and whether
the header constants the existing compressed-mesh writer emits match what a
vanilla ragdoll carries.

Reports raw hex and exact counts, never formatted floats -- four fields have
already been misread this session because a probe printed them prettily.
"""
import os
import struct
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.environ.get("HKPACK_DIR", HERE))
from hkpack import Pack, extract, skeletons


def sections(blob):
    ns, = struct.unpack_from("<i", blob, 20)
    out = []
    for s in range(ns):
        o = 0x40 + s * 0x40
        out.append((blob[o:o + 19].split(b"\x00")[0].decode(),)
                   + struct.unpack_from("<7i", blob, o + 20))
    return out


def classnames(blob):
    """(offset, hash, name) in file order."""
    cn = sections(blob)[0]
    out, p, endp = [], cn[1], cn[1] + cn[2]
    while p < endp - 5:
        if blob[p + 4] != 0x09:
            break
        e = blob.index(b"\x00", p + 5)
        h, = struct.unpack_from("<I", blob, p)
        out.append((p + 5 - cn[1], h, blob[p + 5:e].decode()))
        p = e + 1
    return out


def main():
    tmp = os.path.abspath("_pl.bin")
    cnLists = Counter()
    hashes = defaultdict(set)
    orders = Counter()
    padHist = Counter()
    hdrWords = defaultdict(Counter)
    secWords = defaultdict(Counter)
    localSorted = Counter()
    globSorted = Counter()
    virtSorted = Counter()
    cnStart = Counter()
    tailPad = Counter()
    nfile = 0

    for nif in skeletons():
        blob = extract(nif, tmp)
        if not blob:
            continue
        try:
            pk = Pack(blob)
        except Exception:
            continue
        nfile += 1

        cn = classnames(blob)
        cnLists[tuple(n for _, _, n in cn)] += 1
        for _, h, n in cn:
            hashes[n].add(h)
        sec = sections(blob)
        cnStart[sec[0][1]] += 1
        # 0x100 is where the compressed-mesh writer starts __classnames__

        # object order by class, and the gap before each object
        objs = sorted(pk.virt)
        orders[tuple(c for _, c in objs)] += 1
        for i in range(len(objs) - 1):
            size = objs[i + 1][0] - objs[i][0]
            padHist[(objs[i][1], size % 16)] += 1
        # trailing pad: from the last object's end to the local-fixup table
        dt = [s for s in sec if s[0] == "__data__"][0]
        tailPad[(dt[2] - (objs[-1][0] + 0)) % 16] += 1

        # are the three fixup tables written in ascending source order?
        def ascending(p, end, stride):
            prev, ok = -1, True
            while p + stride <= end:
                src, = struct.unpack_from("<i", blob, p)
                p += stride
                if src == -1:
                    break
                ok &= src > prev
                prev = src
            return ok
        base = dt[1]
        localSorted[ascending(base + dt[2], base + dt[3], 8)] += 1
        globSorted[ascending(base + dt[3], base + dt[4], 12)] += 1
        virtSorted[ascending(base + dt[4], base + dt[5], 12)] += 1

        for o in range(0, 0x40, 4):
            hdrWords[o][blob[o:o + 4].hex()] += 1
        for si, s in enumerate(sec):
            o = 0x40 + si * 0x40
            for w in list(range(0, 0x14, 4)) + list(range(0x30, 0x40, 4)):
                secWords[(si, w)][blob[o + w:o + w + 4].hex()] += 1

    print("%d ragdoll packfiles\n" % nfile)

    print("__classnames__ lists: %d distinct" % len(cnLists))
    for lst, n in cnLists.most_common(3):
        print("  x%-3d %d names: %s" % (n, len(lst), ", ".join(lst)))
    print("\nnames with more than one hash: %s"
          % ([n for n, h in hashes.items() if len(h) > 1] or "none"))
    print("__classnames__ start offsets: %s" % dict(cnStart))

    print("\nobject orders: %d distinct" % len(orders))
    for lst, n in orders.most_common(2):
        c = Counter(lst)
        print("  x%-3d %d objects: %s" % (n, len(lst), dict(c)))
        print("       first 12: %s" % ", ".join(lst[:12]))

    print("\nsize %% 16 after each object class:")
    byClass = defaultdict(Counter)
    for (cls, m), n in padHist.items():
        byClass[cls][m] += n
    for cls in sorted(byClass):
        print("  %-34s %s" % (cls, dict(byClass[cls])))
    print("tail gap %% 16: %s" % dict(tailPad))

    print("\nfixup tables ascending by source?  local=%s global=%s virtual=%s"
          % (dict(localSorted), dict(globSorted), dict(virtSorted)))

    print("\nfile header words that vary:")
    for o in sorted(hdrWords):
        c = hdrWords[o]
        print("  +0x%02x  %s" % (o, ("CONST " + list(c)[0]) if len(c) == 1
                                 else "%d distinct: %s" % (len(c), dict(c))))

    print("\nsection header constants (start/offsets excluded where they must vary):")
    for (si, w) in sorted(secWords):
        c = secWords[(si, w)]
        if len(c) == 1:
            print("  sec%d +0x%02x  CONST %s" % (si, w, list(c)[0]))
        else:
            print("  sec%d +0x%02x  %d distinct" % (si, w, len(c)))


if __name__ == "__main__":
    main()
