"""Two orderings the first probe left open.

  1. __classnames__ order is file-specific (18 distinct lists over 37 files, the
     same 10 names appearing in different orders). Test: is it order of FIRST USE,
     i.e. the order the classes first appear walking objects up __data__?

  2. the global fixup table is NOT ascending by source on any of the 37, while
     local and virtual both are. Test what it IS sorted by.

Also checks the __types__ section really is empty and the section offsets follow
the same start/end chain the compressed-mesh writer assumes.
"""
import os
import struct
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.environ.get("HKPACK_DIR", HERE))
from hkpack import Pack, extract, skeletons
from pack_layout import classnames, sections


def main():
    tmp = os.path.abspath("_po.bin")
    cnFirstUse = Counter()
    globBy = Counter()
    typesEmpty = Counter()
    chain = Counter()
    rootFirst = Counter()
    skelLast = Counter()
    n = 0

    for nif in skeletons():
        blob = extract(nif, tmp)
        if not blob:
            continue
        try:
            pk = Pack(blob)
        except Exception:
            continue
        n += 1

        cn = [name for _, _, name in classnames(blob)]
        objs = sorted(pk.virt)
        seen, order = set(), []
        for _, cls in objs:
            if cls not in seen:
                seen.add(cls)
                order.append(cls)
        # the four hk* reflection classes lead every list and own no object
        cnFirstUse[tuple(cn[4:]) == tuple(order)] += 1
        rootFirst[objs[0][1]] += 1
        skelLast[objs[-1][1]] += 1

        sec = sections(blob)
        dt = [s for s in sec if s[0] == "__data__"][0]
        base = dt[1]
        g = []
        p = base + dt[3]
        while p + 12 <= base + dt[4]:
            src, s2, dst = struct.unpack_from("<iii", blob, p)
            p += 12
            if src == -1:
                break
            g.append((src, dst))
        globBy["by target"] += all(g[i][1] < g[i + 1][1] for i in range(len(g) - 1))
        globBy["by target, ties by source"] += all(
            (g[i][1], g[i][0]) < (g[i + 1][1], g[i + 1][0]) for i in range(len(g) - 1))
        globBy["n"] += 1

        typesEmpty[(sec[1][2], sec[1][3], sec[1][4], sec[1][5])] += 1
        chain[(sec[0][1] == 0x100, sec[1][1] == sec[0][1] + sec[0][5],
               sec[2][1] == sec[1][1] + sec[1][5])] += 1

    print("%d files\n" % n)
    print("__classnames__[4:] == order of first use in __data__:  %s" % dict(cnFirstUse))
    print("first object class: %s" % dict(rootFirst))
    print("last object class:  %s" % dict(skelLast))
    print("\nglobal fixups: %s" % dict(globBy))
    print("\n__types__ (localOff, globalOff, virtualOff, exportsOff): %s" % dict(typesEmpty))
    print("section start chain (cn@0x100, types after cn, data after types): %s" % dict(chain))


if __name__ == "__main__":
    main()
