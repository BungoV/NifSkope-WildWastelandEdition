"""survey_details.py - the measurements the writer's correctness depends on.

1. ORDERING. In what order does the master lay out exterior blocks,
   sub-blocks, and the CELLs inside a sub-block? A plugin that loads but
   orders cells differently from Bethesda is the exact "silently wrong"
   failure the brief warns about, so copy their order rather than invent one.

2. THE LOCALIZATION TRAP. Fallout4.esm sets the Localized flag, which means
   every localizable subrecord (FULL, DESC, ...) holds a u32 STRING ID into
   Fallout4.esm's own .STRINGS files, not text. If we copy a CELL payload
   verbatim into a plugin that is NOT flagged localized, the engine will read
   those 4 bytes as a zero-terminated string. Count how many Commonwealth
   CELLs carry such a subrecord - that decides whether this matters at all.

3. WHAT THE UNPAINTED LANDS ACTUALLY CONTAIN. The whole premise is that
   32,909 cells have terrain but no texture. Measure the subrecord shape and
   the on-disk size of those LANDs, because that is what sets the size of the
   finished plugin.

4. THE BTXT "unknown" BYTE. esmdata.cpp calls byte 5 of BTXT a pad; DLCRobot
   changed it (0xC7 -> 0x4E) while keeping everything else, so it is not a
   pad. Measure its real distribution so a synthesised BTXT uses a value the
   game has actually seen.

Read-only.
"""

import collections
import struct

import fo4esm as E

ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'

# Localizable subrecords on CELL in FO4. FULL is the only one a CELL has.
LOCALIZED_TAGS = {b'FULL', b'DESC', b'SHRT', b'RNAM', b'ITXT'}


def main():
    buf = E.load(ESM)

    wc = None
    for node in E.top_level_groups(buf):
        if node.label == b'WRLD' and node.gtype == E.GT_TOP:
            def find_wc(n, stack):
                global_wc.append(n) if (n.gtype == E.GT_WORLD_CHILDREN and
                                        n.label == b'\x3c\x00\x00\x00') else None
            global_wc = []
            E.walk(buf, node.offset + 24, node.offset + node.gsize, [node],
                   lambda *a: None, find_wc)
            wc = global_wc[0]
            break
    assert wc is not None
    print('Commonwealth World Children @0x%X size %d' % (wc.offset, wc.gsize))

    # ---------------- 1. ordering -----------------------------------
    block_order = []
    sub_order = collections.defaultdict(list)
    cell_order = collections.defaultdict(list)
    cur = {'blk': None, 'sub': None}

    # cell payload census
    full_cells = 0
    cell_tag_census = collections.Counter()
    xclc_third = collections.Counter()

    # land census
    land_shapes = collections.Counter()
    land_size_by_shape = collections.defaultdict(list)
    btxt_unknown = collections.Counter()
    btxt_layer = collections.Counter()
    btxt_quadrant = collections.Counter()
    land_rows = []

    def on_grp(node, stack):
        if node.gtype == E.GT_EXTERIOR_BLOCK:
            y, x = struct.unpack('<hh', node.label)
            block_order.append((x, y))
            cur['blk'] = (x, y)
        elif node.gtype == E.GT_EXTERIOR_SUBBLOCK:
            y, x = struct.unpack('<hh', node.label)
            sub_order[cur['blk']].append((x, y))
            cur['sub'] = (x, y)

    cell_xy = {'xy': None, 'formid': None}

    def on_rec(off, sig, dsize, flags, formid, tail, stack):
        nonlocal full_cells
        if sig == b'CELL' and stack[-1].gtype == E.GT_EXTERIOR_SUBBLOCK:
            payload = E.record_payload(buf, off, dsize, flags)
            xy = None
            has_full = False
            for tag, content in E.subrecords(payload):
                cell_tag_census[tag] += 1
                if tag in LOCALIZED_TAGS:
                    has_full = True
                if tag == b'XCLC' and len(content) >= 12:
                    xy = struct.unpack_from('<ii', content, 0)
                    xclc_third[struct.unpack_from('<I', content, 8)[0]] += 1
            if has_full:
                full_cells += 1
            cell_order[cur['sub']].append((xy, formid))
            cell_xy['xy'] = xy
            cell_xy['formid'] = formid
        elif sig == b'LAND':
            payload = E.record_payload(buf, off, dsize, flags)
            shape = []
            nb = na = 0
            for tag, content in E.subrecords(payload):
                shape.append(tag)
                if tag == b'BTXT':
                    nb += 1
                    btxt_unknown[content[5]] += 1
                    btxt_layer[struct.unpack_from('<h', content, 6)[0]] += 1
                    btxt_quadrant[content[4]] += 1
                elif tag == b'ATXT':
                    na += 1
            key = tuple(sorted(set(shape)))
            land_shapes[key] += 1
            land_size_by_shape[key].append(dsize)
            land_rows.append((cell_xy['xy'], dsize, len(payload), nb, na))

    E.walk(buf, wc.offset + 24, wc.offset + wc.gsize, [wc], on_rec, on_grp)

    print()
    print('=== 1. ORDERING ===')
    print('block order as written (x,y), first 12: %s' % (block_order[:12],))
    print('blocks sorted by (y,x)? %s   by (x,y)? %s'
          % (block_order == sorted(block_order, key=lambda t: (t[1], t[0])),
             block_order == sorted(block_order, key=lambda t: (t[0], t[1]))))
    b0 = block_order[0]
    print('sub-blocks in block %s, as written: %s' % (b0, sub_order[b0]))
    allsub_yx = all(v == sorted(v, key=lambda t: (t[1], t[0])) for v in sub_order.values())
    allsub_xy = all(v == sorted(v, key=lambda t: (t[0], t[1])) for v in sub_order.values())
    print('every block: sub-blocks sorted by (y,x)? %s   by (x,y)? %s'
          % (allsub_yx, allsub_xy))
    s0 = sub_order[b0][0]
    print('cells in sub-block %s, as written (xy, formid), first 10:' % (s0,))
    for xy, fid in cell_order[s0][:10]:
        print('    %s  %08X' % (xy, fid))
    ok_fid = ok_yx = ok_xy = 0
    tot = 0
    for k, v in cell_order.items():
        tot += 1
        fids = [f for _, f in v]
        xys = [c for c, _ in v]
        if fids == sorted(fids):
            ok_fid += 1
        if xys == sorted(xys, key=lambda t: (t[1], t[0])):
            ok_yx += 1
        if xys == sorted(xys, key=lambda t: (t[0], t[1])):
            ok_xy += 1
    print('sub-blocks (%d) whose cells are in ascending FormID order: %d' % (tot, ok_fid))
    print('   ... ascending (y,x): %d      ascending (x,y): %d' % (ok_yx, ok_xy))
    print('cells per sub-block: %s'
          % collections.Counter(len(v) for v in cell_order.values()).most_common(5))

    print()
    print('=== 2. LOCALIZATION TRAP ===')
    print('Commonwealth exterior CELLs carrying a localizable subrecord: %d / %d'
          % (full_cells, sum(cell_order and len(v) for v in cell_order.values())))
    print('CELL subrecord tag census: %s'
          % [(t.decode('latin1'), n) for t, n in cell_tag_census.most_common()])
    print('distinct XCLC[8:12] values: %d ; top: %s'
          % (len(xclc_third), [('0x%08X' % k, n) for k, n in xclc_third.most_common(5)]))

    print()
    print('=== 3. WHAT THE LANDS CONTAIN ===')
    for shape, n in land_shapes.most_common(12):
        sizes = land_size_by_shape[shape]
        print('  %-52s x%-6d on-disk min %d med %d max %d  total %s'
              % ('+'.join(t.decode('latin1') for t in shape), n,
                 min(sizes), sorted(sizes)[len(sizes) // 2], max(sizes),
                 '{:,}'.format(sum(sizes))))
    notex = [r for r in land_rows if r[3] == 0 and r[4] == 0]
    withtex = [r for r in land_rows if r[3] or r[4]]
    print('  LANDs with NO BTXT and NO ATXT: %d' % len(notex))
    print('  LANDs with some texture data  : %d' % len(withtex))
    if notex:
        s = sorted(r[1] for r in notex)
        p = sorted(r[2] for r in notex)
        print('    untextured: on-disk min %d med %d max %d total %s'
              % (s[0], s[len(s) // 2], s[-1], '{:,}'.format(sum(s))))
        print('    untextured: payload  min %d med %d max %d total %s'
              % (p[0], p[len(p) // 2], p[-1], '{:,}'.format(sum(p))))

    print()
    print('=== 4. BTXT FIELDS ===')
    print('  quadrant byte[4] : %s' % sorted(btxt_quadrant.items()))
    print('  layer  s16[6:8]  : %s' % sorted(btxt_layer.items()))
    print('  "unknown" byte[5]: %d distinct values; top 10 %s'
          % (len(btxt_unknown), btxt_unknown.most_common(10)))
    tot_b = sum(btxt_unknown.values())
    print('  byte[5] == 0 in %d / %d BTXTs (%.1f%%)'
          % (btxt_unknown.get(0, 0), tot_b, 100.0 * btxt_unknown.get(0, 0) / tot_b))


if __name__ == '__main__':
    main()
