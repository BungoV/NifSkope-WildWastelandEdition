"""find_ltex.py - which LTEX records exist, and which are actually used as a
Commonwealth base texture. The placeholder used to prove the writer has to be
a REAL landscape texture, not an invented FormID, or the test plugin proves
nothing about what the game would do with it.
"""
import collections
import struct

import fo4esm as E

ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'


def main():
    buf = E.load(ESM)
    ltex = {}
    for node in E.top_level_groups(buf):
        if node.label == b'LTEX' and node.gtype == E.GT_TOP:
            def on(off, sig, ds, fl, fid, tl, st):
                p = E.record_payload(buf, off, ds, fl)
                ed = next((c.rstrip(b'\x00').decode('latin1')
                           for t, c in E.subrecords(p) if t == b'EDID'), '')
                tx = next((struct.unpack('<I', c)[0]
                           for t, c in E.subrecords(p) if t == b'TNAM'), 0)
                ltex[fid] = (ed, tx)
            E.walk(buf, node.offset + 24, node.offset + node.gsize, [node], on)
    print('LTEX records in Fallout4.esm: %d' % len(ltex))

    # count use as a Commonwealth BTXT base
    use = collections.Counter()
    for node in E.top_level_groups(buf):
        if node.label != b'WRLD' or node.gtype != E.GT_TOP:
            continue
        wc = []
        E.walk(buf, node.offset + 24, node.offset + node.gsize, [node],
               lambda *a: None,
               lambda g, s: wc.append(g) if (g.gtype == E.GT_WORLD_CHILDREN and
                                             struct.unpack('<I', g.label)[0]
                                             == E.COMMONWEALTH) else None)
        w = wc[0]

        def on2(off, sig, ds, fl, fid, tl, st):
            if sig != b'LAND':
                return
            for t, c in E.subrecords(E.record_payload(buf, off, ds, fl)):
                if t == b'BTXT' and len(c) >= 8:
                    use[struct.unpack_from('<I', c, 0)[0]] += 1

        E.walk(buf, w.offset + 24, w.offset + w.gsize, [w], on2)
        break

    print()
    print('most-used Commonwealth base textures:')
    for fid, n in use.most_common(12):
        ed, tx = ltex.get(fid, ('<not an LTEX in this file>', 0))
        print('  %08X  x%-5d  %s' % (fid, n, ed))
    print()
    print('every used FormID resolves to an LTEX record: %s'
          % all(f in ltex for f in use))


if __name__ == '__main__':
    main()
