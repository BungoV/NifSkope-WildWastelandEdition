"""survey_nowrld.py - is a World Children group legal WITHOUT its WRLD record?

This is the one question that decides whether the plugin should carry a
Commonwealth WRLD override. survey_wrld2.py showed carrying one is harmful:
DLCCoast adds 167 large references and DLCNukaWorld 75 that Fallout4.esm does
not have, so a WRLD record copied from the master and loaded last would delete
them; and the master's WRLD FULL is a string ID into a .STRINGS table that
does not exist as a loose file here.

So look for a shipping counterexample: any plugin, any worldspace, where a
GRUP type 1 (World Children) appears in the top-level WRLD group with NO
preceding WRLD record of the same FormID in that same plugin. If Bethesda
ships one, omitting the WRLD record is proven safe. If not, this lane cannot
prove it either way and it goes in Open Risks.

Same question one level down for CELL: does any Cell Children group (type 6)
appear without its CELL record in the same plugin?

Read-only.
"""

import os
import struct

import fo4esm as E

DATA = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data'


def scan(path):
    buf = E.load(path)
    name = os.path.basename(path)
    wrld_records = set()
    orphan_wc = []
    cell_records = set()
    orphan_cc = []
    n_wc = n_cc = 0

    for node in E.top_level_groups(buf):
        if node.label != b'WRLD' or node.gtype != E.GT_TOP:
            continue

        def on_rec(off, sig, dsize, flags, formid, tail, stack):
            if sig == b'WRLD':
                wrld_records.add(formid)
            elif sig == b'CELL':
                cell_records.add(formid)

        def on_grp(g, stack):
            pass

        # First pass: collect every WRLD/CELL formid present anywhere.
        E.walk(buf, node.offset + 24, node.offset + node.gsize, [node], on_rec, on_grp)

        # Second pass: check each children group's label against that set.
        def on_grp2(g, stack):
            nonlocal n_wc, n_cc
            if g.gtype == E.GT_WORLD_CHILDREN:
                n_wc += 1
                fid = struct.unpack('<I', g.label)[0]
                if fid not in wrld_records:
                    orphan_wc.append(fid)
            elif g.gtype == E.GT_CELL_CHILDREN:
                n_cc += 1
                fid = struct.unpack('<I', g.label)[0]
                if fid not in cell_records:
                    orphan_cc.append(fid)

        E.walk(buf, node.offset + 24, node.offset + node.gsize, [node],
               lambda *a: None, on_grp2)

    # Interior cells too: a top-level CELL group carries the same type-6 shape.
    for node in E.top_level_groups(buf):
        if node.label != b'CELL' or node.gtype != E.GT_TOP:
            continue
        ic = set()

        def on_rec2(off, sig, dsize, flags, formid, tail, stack):
            if sig == b'CELL':
                ic.add(formid)

        E.walk(buf, node.offset + 24, node.offset + node.gsize, [node], on_rec2)

        def on_grp3(g, stack):
            nonlocal n_cc
            if g.gtype == E.GT_CELL_CHILDREN:
                n_cc += 1
                fid = struct.unpack('<I', g.label)[0]
                if fid not in ic:
                    orphan_cc.append(fid)

        E.walk(buf, node.offset + 24, node.offset + node.gsize, [node],
               lambda *a: None, on_grp3)

    print('%-34s WRLD records=%-4d WorldChildren groups=%-4d ORPHAN=%d %s'
          % (name, len(wrld_records), n_wc, len(orphan_wc),
             ['%08X' % f for f in orphan_wc[:4]] if orphan_wc else ''))
    print('%-34s CELL records=%-6d CellChildren groups=%-6d ORPHAN=%d %s'
          % ('', len(cell_records), n_cc, len(orphan_cc),
             ['%08X' % f for f in orphan_cc[:4]] if orphan_cc else ''))
    return len(orphan_wc), len(orphan_cc)


def main():
    names = sorted(n for n in os.listdir(DATA)
                   if n.lower().endswith(('.esm', '.esp', '.esl')))
    tw = tc = 0
    for n in names:
        p = os.path.join(DATA, n)
        if os.path.getsize(p) < 200:
            continue
        try:
            a, b = scan(p)
            tw += a
            tc += b
        except Exception as exc:
            print('%-34s FAILED %s: %s' % (n, type(exc).__name__, exc))
    print()
    print('TOTAL orphan World Children groups across every shipped plugin: %d' % tw)
    print('TOTAL orphan Cell  Children groups across every shipped plugin: %d' % tc)


if __name__ == '__main__':
    main()
