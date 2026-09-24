#!/usr/bin/env python3
"""CELLVIEW3 item 1 -- WHY 154 of 240 Sanctuary placements are grey.

An INDEPENDENT join (ww-independent-placement-check): nothing here asks the
viewer anything.  The `.lodi` is read with the offsets `src/lodifile.cpp:26-32`
DECLARES (H_INSTANCES 0x58, H_OFF_COLD 0x80, H_WEST/SOUTH/EAST/NORTH 0x48..0x4E,
H_CELLS 0x50), and the plugin is walked by `tests/spells/cell_census.py`'s own
GRUP reader.  The viewer's dump is used ONLY as the population of drawn
placements (ref, base, type) -- the join itself, and the has-LOD-model column,
are recomputed from the plugin.

Usage:
  python join_probe.py <dump.txt> <Fallout4.esm> <Commonwealth.lodi> <cx> <cy>
"""
import os
import struct
import sys
from collections import defaultdict

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', 'tests', 'spells'))
from cell_census import Esm, Rec, fields, decompress   # noqa: E402


# ------------------------------------------------------------------ the .lodi

def lodi_read(path):
    b = open(path, 'rb').read()
    magic, version = struct.unpack_from('<II', b, 0x00)
    west, south, east, north = struct.unpack_from('<hhhh', b, 0x48)
    cells, stride = struct.unpack_from('<HH', b, 0x50)
    chunks, insts = struct.unpack_from('<II', b, 0x54)
    off_cold, = struct.unpack_from('<Q', b, 0x80)
    off_inst, = struct.unpack_from('<Q', b, 0x78)
    off_group, = struct.unpack_from('<Q', b, 0x100)
    gcount, = struct.unpack_from('<I', b, 0x108)
    cold = []
    for i in range(insts):
        form, part, ident = struct.unpack_from('<IhH', b, off_cold + 8 * i)
        cold.append((form, part, ident))
    return dict(version=version, west=west, south=south, east=east, north=north,
                cells=cells, chunks=chunks, instances=insts, cold=cold,
                groups=gcount, path=path)


# ------------------------------------------------------------- the base index

WIDE = {b'STAT', b'SCOL', b'MSTT', b'FURN', b'CONT', b'DOOR', b'ACTI', b'TREE',
        b'FLOR', b'LIGH', b'MISC', b'TERM', b'ALCH', b'AMMO', b'ARMO', b'BOOK',
        b'KEYM', b'NOTE', b'WEAP', b'IDLM', b'TXST', b'PROJ', b'HAZD', b'ASPC',
        b'SOUN', b'MSTT'}


def index_bases_lod(esm):
    """form -> (type, modl, edid, has_mnam, mnam_levels, tree_hasdistant)."""
    out = {}
    buf = esm.buf

    def cb(rec, path):
        if rec is None or rec.type not in WIDE:
            return
        if rec.flags & 0x00040000:
            try:
                data = decompress(buf, rec)
            except Exception:
                return
            it = fields(data, Rec(rec.type, len(data), rec.flags, rec.form, 0))
        else:
            it = fields(buf, rec)
        modl = ''
        edid = ''
        levels = 0
        for t, p in it:
            if t == b'EDID':
                edid = p.split(b'\0')[0].decode('cp1252', 'replace')
            elif t == b'MNAM' and rec.type == b'STAT':
                # src/esmdata.cpp:462-479 -- 4 x 260-byte zero-terminated paths
                for lv in range(4):
                    o = lv * 260
                    if o >= len(p):
                        break
                    s = p[o:o + 260].split(b'\0')[0]
                    if s:
                        levels += 1
            elif t == b'MODL' and not modl:
                s = p.split(b'\0')[0].decode('cp1252', 'replace')
                if s.lower().endswith('.nif'):
                    modl = s
        # src/esmdata.cpp:512-514 -- a TREE flagged Has Distant LOD has LOD too
        tree_lod = (rec.type == b'TREE' and bool(rec.flags & 0x00008000))
        out[rec.form] = (rec.type.decode(), modl, edid, levels > 0, levels, tree_lod)

    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), cb)
    return out


# ------------------------------------------------------------------ the cells

def refrs_in_cell(esm, world_edid, cx, cy):
    """[(refForm, baseForm)] for one exterior cell, persistent + temporary."""
    buf = esm.buf
    target = world_edid.encode('cp1252')
    wform = [None]

    def find(rec, path):
        if rec is not None and rec.type == b'WRLD':
            for t, p in fields(buf, rec):
                if t == b'EDID' and p.split(b'\0')[0] == target:
                    wform[0] = rec.form
    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), find)
    if wform[0] is None:
        raise SystemExit('no worldspace ' + world_edid)

    cellxy = {}
    byparent = defaultdict(list)

    def cb(rec, path):
        under = False
        parent = None
        for label, gtype, goff in path:
            if gtype == 1 and struct.unpack_from('<I', label, 0)[0] == wform[0]:
                under = True
            if gtype in (8, 9, 10):
                parent = struct.unpack_from('<I', label, 0)[0]
        if not under or rec is None:
            return
        if rec.type == b'CELL':
            if rec.flags & 0x00040000:
                data = decompress(buf, rec)
                it = fields(data, Rec(rec.type, len(data), 0, rec.form, 0))
            else:
                it = fields(buf, rec)
            for t, p in it:
                if t == b'XCLC' and len(p) >= 8:
                    cellxy[rec.form] = struct.unpack_from('<ii', p, 0)
        elif rec.type == b'REFR' and parent is not None:
            base = None
            if rec.flags & 0x00040000:
                data = decompress(buf, rec)
                it = fields(data, Rec(rec.type, len(data), 0, rec.form, 0))
            else:
                it = fields(buf, rec)
            for t, p in it:
                if t == b'NAME' and len(p) >= 4:
                    base = struct.unpack_from('<I', p, 0)[0]
            byparent[parent].append((rec.form, base, bool(rec.flags & 0x20)))

    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), cb)
    for f, xy in cellxy.items():
        if xy == (cx, cy):
            return byparent.get(f, []), f
    raise SystemExit('cell %d,%d not found' % (cx, cy))


def main():
    dump, esm_path, lodi_path, cx, cy = (sys.argv[1], sys.argv[2], sys.argv[3],
                                         int(sys.argv[4]), int(sys.argv[5]))
    L = lodi_read(lodi_path)
    lodi_forms = set(f for f, _, _ in L['cold'] if f)
    print('.lodi %s' % os.path.basename(L['path']))
    print('  v%d, %d instances, %d distinct refFormIds, %d groups'
          % (L['version'], L['instances'], len(lodi_forms), L['groups']))
    print('  chunks west..east %d..%d, south..north %d..%d, %d cells a chunk'
          % (L['west'], L['east'], L['south'], L['north'], L['cells']))
    print('  => CELLS covered x %d..%d, y %d..%d'
          % (L['west'] * L['cells'], (L['east'] + 1) * L['cells'] - 1,
             L['south'] * L['cells'], (L['north'] + 1) * L['cells'] - 1))
    inside = (L['west'] * L['cells'] <= cx <= (L['east'] + 1) * L['cells'] - 1 and
              L['south'] * L['cells'] <= cy <= (L['north'] + 1) * L['cells'] - 1)
    print('  cell %d,%d is %s the baked rectangle' % (cx, cy, 'INSIDE' if inside else 'OUTSIDE'))

    esm = Esm(esm_path)
    bases = index_bases_lod(esm)
    print('bases indexed: %d' % len(bases))

    # -------- population A: the plugin's own REFRs for this cell
    refs, cellform = refrs_in_cell(esm, 'Commonwealth', cx, cy)
    print('plugin: cell %08X holds %d REFRs (persistent + temporary)'
          % (cellform, len(refs)))

    # -------- population B: the drawn placements, from the viewer's dump
    drawn = []
    for line in open(dump):
        if line.startswith('#'):
            continue
        c = line.split()
        drawn.append((int(c[0], 16), int(c[1], 16), c[2], int(c[3])))
    print('dump: %d drawn placements, %d distinct refFormIds'
          % (len(drawn), len(set(d[0] for d in drawn))))

    # -------- the cross-tab, recomputed here
    tab = defaultdict(lambda: [0, 0])
    joined = notjoined = 0
    notjoined_with_lod = []
    for ref, base, rtype, part in drawn:
        b = bases.get(base)
        if b is None:
            haslod = None
        else:
            haslod = b[3] or b[5]
        key = (rtype, 'has LOD model' if haslod else
               ('no LOD model' if haslod is not None else 'base not indexed'))
        if ref in lodi_forms:
            tab[key][0] += 1
            joined += 1
        else:
            tab[key][1] += 1
            notjoined += 1
            if haslod:
                notjoined_with_lod.append((ref, base, rtype, b[2] if b else '', b[1] if b else ''))
    print('')
    print('| record type | LOD model on the base | joined to a .lodi group | NOT joined |')
    print('|---|---|---|---|')
    for k in sorted(tab):
        print('| %s | %s | %d | %d |' % (k[0], k[1], tab[k][0], tab[k][1]))
    print('| **total** | | **%d** | **%d** |' % (joined, notjoined))

    print('')
    print('NOT joined although the base HAS a LOD model: %d' % len(notjoined_with_lod))
    for r in notjoined_with_lod[:40]:
        print('  ref %08X base %08X %s %s %s' % r)

    # -------- the other direction: what the bake holds for this cell's refs
    ref_set = set(r for r, _, _ in refs)
    print('')
    print('plugin REFRs of this cell that ARE in the bake: %d of %d'
          % (len(ref_set & lodi_forms), len(ref_set)))


if __name__ == '__main__':
    main()
