"""survey_groups.py - MEASURE the group structure Fallout4.esm uses to carry
Commonwealth LAND records, rather than assuming the conventional one.

Answers, all from the master's own bytes:
  * which GRUP types appear on the path WRLD -> ... -> LAND, in order
  * how the exterior block / sub-block group LABELS relate to cell x,y
    (including the byte order of the two int16s and the sign convention of
    the division, which is where a plausible-but-wrong plugin comes from)
  * what a CELL's children group contains, and which of persistent /
    temporary / visible-distant the LAND lives in
  * whether CELL and LAND records are compressed
  * real LAND record sizes

Read-only. Writes nothing but stdout.
"""

import collections
import struct
import sys

import fo4esm as E

ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'


def main():
    buf = E.load(ESM)
    print('master: %d bytes' % len(buf))

    # --- locate the top-level WRLD group -------------------------------
    wrld_top = None
    tops = []
    for node in E.top_level_groups(buf):
        tops.append((node.label, node.gtype, node.gsize))
        if node.label == b'WRLD' and node.gtype == E.GT_TOP:
            wrld_top = node
    print('top-level groups: %d' % len(tops))
    assert wrld_top is not None, 'no top-level WRLD group'
    print('WRLD top group @0x%X size %d' % (wrld_top.offset, wrld_top.gsize))

    # --- find the Commonwealth WRLD record and its World Children ------
    # Structure at this level is measured, not assumed: we record the
    # (record sig, following group type/label) sequence.
    found = {}
    seq = []

    def on_rec(off, sig, dsize, flags, formid, tail, stack):
        if len(stack) == 1 and sig == b'WRLD':
            seq.append(('WRLD', formid))
            if formid == E.COMMONWEALTH:
                found['wrld_off'] = off
                found['wrld_dsize'] = dsize
                found['wrld_flags'] = flags
                found['wrld_tail'] = tail

    def on_grp(node, stack):
        if len(stack) == 1:
            seq.append(('GRUP', node.gtype, node.label, node.offset, node.gsize))

    E.walk(buf, wrld_top.offset + 24, wrld_top.offset + wrld_top.gsize, [wrld_top],
           on_rec, on_grp)

    assert 'wrld_off' in found, 'Commonwealth 0000003C WRLD record not found'
    print('Commonwealth WRLD record @0x%X dsize=%d flags=0x%08X compressed=%s'
          % (found['wrld_off'], found['wrld_dsize'], found['wrld_flags'],
             bool(found['wrld_flags'] & E.COMPRESSED_FLAG)))
    print('WRLD record header tail bytes: %s' % found['wrld_tail'].hex())

    # what immediately follows the Commonwealth WRLD record?
    for i, item in enumerate(seq):
        if item[0] == 'WRLD' and item[1] == E.COMMONWEALTH:
            nxt = seq[i + 1] if i + 1 < len(seq) else None
            print('item right after the Commonwealth WRLD record: %r' % (nxt,))
            assert nxt is not None and nxt[0] == 'GRUP'
            wc_type, wc_label, wc_off, wc_size = nxt[1], nxt[2], nxt[3], nxt[4]
            break
    print('World Children group: type=%d (%s) label=%s (=formid 0x%08X) '
          'offset=0x%X size=%d'
          % (wc_type, E.GT_NAME.get(wc_type, '?'), wc_label.hex(),
             struct.unpack('<I', wc_label)[0], wc_off, wc_size))

    # --- walk the Commonwealth world children --------------------------
    wc_node = E.Node(wc_off, wc_size, wc_label, wc_type, bytes(buf[wc_off + 16:wc_off + 24]))

    paths = collections.Counter()          # tuple of group types -> count of LANDs
    rec_in_group = collections.Counter()   # (parent group type, record sig) -> count
    cell_of_group = {}                     # id(CellChildren node) -> cell (x,y)
    land_rows = []                         # (x, y, cellformid, landformid, dsize, flags, stackinfo)
    cell_rows = []
    group_sizes = collections.Counter()
    group_labels_by_type = collections.defaultdict(set)
    cur_cell = {'xy': None, 'formid': None}
    tails = collections.Counter()
    group_tails = collections.Counter()

    def gpath(stack):
        return tuple(n.gtype for n in stack)

    def on_rec2(off, sig, dsize, flags, formid, tail, stack):
        parent = stack[-1].gtype if stack else -1
        rec_in_group[(parent, sig)] += 1
        if sig == b'CELL':
            payload = E.record_payload(buf, off, dsize, flags)
            xy = None
            for tag, content in E.subrecords(payload):
                if tag == b'XCLC' and len(content) >= 8:
                    xy = struct.unpack_from('<ii', content, 0)
            cur_cell['xy'] = xy
            cur_cell['formid'] = formid
            # remember the enclosing sub-block/block labels for this cell
            blk = next((n for n in stack if n.gtype == E.GT_EXTERIOR_BLOCK), None)
            sub = next((n for n in stack if n.gtype == E.GT_EXTERIOR_SUBBLOCK), None)
            cell_rows.append((xy, formid, dsize, flags, len(payload),
                              blk.label if blk else None, sub.label if sub else None,
                              tail))
            tails[('CELL', tail)] += 1
        elif sig == b'LAND':
            paths[gpath(stack)] += 1
            land_rows.append((cur_cell['xy'], cur_cell['formid'], formid, dsize, flags,
                              stack[-1].gtype))
            tails[('LAND', tail)] += 1

    def on_grp2(node, stack):
        group_sizes[node.gtype] += 1
        group_labels_by_type[node.gtype].add(node.label)
        group_tails[(node.gtype, node.tail)] += 1

    E.walk(buf, wc_off + 24, wc_off + wc_size, [wc_node], on_rec2, on_grp2)

    print()
    print('=== group type census inside Commonwealth World Children ===')
    for gt, n in sorted(group_sizes.items()):
        print('  type %2d %-20s x%-7d distinct labels: %d'
              % (gt, E.GT_NAME.get(gt, '?'), n, len(group_labels_by_type[gt])))

    print()
    print('=== record signature by immediately-enclosing group type ===')
    for (gt, sig), n in sorted(rec_in_group.items(), key=lambda kv: -kv[1])[:20]:
        print('  in type %2d %-20s : %-6s x%d'
              % (gt, E.GT_NAME.get(gt, '?'), sig.decode('latin1'), n))

    print()
    print('=== group-type path from World Children down to each LAND ===')
    for path, n in paths.most_common():
        print('  %s  x%d' % (' -> '.join('%d:%s' % (g, E.GT_NAME.get(g, '?'))
                                         for g in path), n))

    print()
    print('=== LAND records: %d ===' % len(land_rows))
    comp = sum(1 for r in land_rows if r[4] & E.COMPRESSED_FLAG)
    print('  compressed on disk: %d / %d' % (comp, len(land_rows)))
    sizes = sorted(r[3] for r in land_rows)
    print('  on-disk dataSize: min %d  median %d  max %d  total %d'
          % (sizes[0], sizes[len(sizes) // 2], sizes[-1], sum(sizes)))
    print('  CELL records: %d, compressed %d'
          % (len(cell_rows), sum(1 for r in cell_rows if r[3] & E.COMPRESSED_FLAG)))
    csz = sorted(r[2] for r in cell_rows)
    print('  CELL on-disk dataSize: min %d median %d max %d'
          % (csz[0], csz[len(csz) // 2], csz[-1]))
    dsz = sorted(r[4] for r in cell_rows)
    print('  CELL decompressed size: min %d median %d max %d'
          % (dsz[0], dsz[len(dsz) // 2], dsz[-1]))

    print()
    print('=== header tail (timestamp/vcs/version/unknown) values ===')
    for (what, tail), n in tails.most_common(8):
        ts, vcs, ver, unk = struct.unpack('<HHHH', tail)
        print('  %-5s %s  ts=%d vcs=%d ver=%d unk=%d  x%d'
              % (what, tail.hex(), ts, vcs, ver, unk, n))
    print('=== GRUP header tails ===')
    for (gt, tail), n in group_tails.most_common(10):
        ts, vcs, ver, unk = struct.unpack('<HHHH', tail)
        print('  type %2d %s  ts=%d vcs=%d ver=%d unk=%d  x%d'
              % (gt, tail.hex(), ts, vcs, ver, unk, n))

    # --- derive the block / sub-block label formula ---------------------
    print()
    print('=== block / sub-block label vs cell coordinates ===')
    # For each cell we know (x,y) and the labels of its enclosing block and
    # sub-block groups. Test candidate decodings against ALL of them.
    def cand_yx(label):     # (y, x) as two int16, y first
        return struct.unpack('<hh', label)

    def cand_xy(label):     # (x, y) as two int16, x first
        return struct.unpack('<hh', label)[::-1]

    for name, dec in (('label = (int16 y, int16 x)', cand_yx),
                      ('label = (int16 x, int16 y)', cand_xy)):
        ok_blk = ok_sub = 0
        bad_blk = bad_sub = 0
        first_bad = None
        for xy, formid, dsize, flags, plen, blab, slab, tail in cell_rows:
            if xy is None or blab is None or slab is None:
                continue
            x, y = xy
            by, bx = dec(blab)
            sy, sx = dec(slab)
            if (bx, by) == (x // 32, y // 32):
                ok_blk += 1
            else:
                bad_blk += 1
                if first_bad is None:
                    first_bad = (x, y, bx, by, sx, sy)
            if (sx, sy) == (x // 8, y // 8):
                ok_sub += 1
            else:
                bad_sub += 1
        print('  %-28s block floor-div match %d ok / %d bad ; '
              'sub-block %d ok / %d bad'
              % (name, ok_blk, bad_blk, ok_sub, bad_sub))
        if first_bad:
            print('      first mismatch: cell(%d,%d) block(%d,%d) sub(%d,%d)' % first_bad)

    # Also try C-style truncation toward zero, to prove floor is the right one.
    def trunc(v, d):
        q = abs(v) // d
        return -q if v < 0 else q

    ok = bad = 0
    for xy, formid, dsize, flags, plen, blab, slab, tail in cell_rows:
        if xy is None or blab is None:
            continue
        x, y = xy
        by, bx = struct.unpack('<hh', blab)
        if (bx, by) == (trunc(x, 32), trunc(y, 32)):
            ok += 1
        else:
            bad += 1
    print('  truncate-toward-zero /32 (the wrong-looking-right option): %d ok / %d bad'
          % (ok, bad))

    # --- cell children: what is inside, and where does LAND sit? --------
    print()
    print('=== LAND placement inside the cell children ===')
    byparent = collections.Counter(r[5] for r in land_rows)
    for gt, n in byparent.items():
        print('  LAND directly inside group type %d (%s): %d'
              % (gt, E.GT_NAME.get(gt, '?'), n))

    # --- sanity: coordinate coverage -----------------------------------
    xs = [r[0][0] for r in land_rows if r[0]]
    ys = [r[0][1] for r in land_rows if r[0]]
    print()
    print('LAND cell x %d..%d  y %d..%d' % (min(xs), max(xs), min(ys), max(ys)))
    print('LANDs with no owning CELL coord: %d' % sum(1 for r in land_rows if not r[0]))


if __name__ == '__main__':
    main()
