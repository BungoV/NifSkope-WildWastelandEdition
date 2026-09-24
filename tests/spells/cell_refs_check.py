"""Lane CELLWORK1 -- the Cell workspace's reference list, checked against the
plugin itself.

The workspace's list is the REFERENCE MODEL (`CellRefTable`), not the draw data,
so the number to check it against is "every REFR the plugin places in this cell",
and that number has two sources in a Fallout 4 worldspace:

  1. the cell's own child group -- the temporary and persistent children of the
     CELL record at (cx,cy);
  2. the WORLDSPACE's persistent cell (the CELL directly under the world-children
     GRUP, XCLC 0,0), whose references are placed by POSITION and therefore fall
     into whichever grid square their XYZ lands in. Sanctuary's cell gets 8 that
     way: a folding chair, two patrol idle markers, four decals and a light box.

A bare count comparison hides which of the two is wrong, so this does not compare
counts. It compares SETS:

  * every REFR in the cell's own child group must be in the dump   (nothing lost)
  * every dump row that is not one of those must be a child of the worldspace's
     persistent cell                                               (nothing invented)

Usage:
  python cell_refs_check.py <Fallout4.esm> <worldEdid> <cx> <cy> <refdump>

Prints one line per finding and a final `RESULT ok|bad ...` line the gate reads.
Exit code 0 only when both set claims hold.
"""

import struct
import sys

from cell_census import Esm, Rec, decompress, fields


def cell_children(esm, world_edid, cx, cy):
    """(refs of the CELL at cx,cy, refs of the worldspace persistent cell)."""
    buf = esm.buf
    wrld = [None]

    def find(rec, path):
        if rec is None or rec.type != b'WRLD':
            return
        for t, p in fields(buf, rec):
            if t == b'EDID' and p.split(b'\0')[0].decode('latin-1') == world_edid:
                wrld[0] = rec.form
                return

    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), find)
    if wrld[0] is None:
        raise SystemExit('worldspace %s not found' % world_edid)

    grid = {}           # cell form -> (x,y)
    depth = {}          # cell form -> how deep under the world children it sits
    refs = {}           # cell form -> [refr form]

    def cb(rec, path):
        under = False
        child_of = None
        gtypes = []
        for label, gtype, goff in path:
            gtypes.append(gtype)
            if gtype == 1 and struct.unpack_from('<I', label, 0)[0] == wrld[0]:
                under = True
            if gtype in (6, 8, 9, 10):
                child_of = struct.unpack_from('<I', label, 0)[0]
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
                    grid[rec.form] = struct.unpack_from('<ii', p, 0)
            depth[rec.form] = len([g for g in gtypes if g in (4, 5)])
        elif rec.type == b'REFR' and child_of is not None:
            refs.setdefault(child_of, []).append(rec.form)

    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), cb)

    # The worldspace persistent cell is the one that sits directly under the
    # world-children group, i.e. under no exterior block/subblock at all.
    persistent_cell = None
    target = None
    for form, xy in grid.items():
        if depth.get(form, 9) == 0 and persistent_cell is None:
            persistent_cell = form
        elif xy == (cx, cy):
            target = form
    if target is None:
        raise SystemExit('no CELL at %d,%d in %s' % (cx, cy, world_edid))
    return target, persistent_cell, refs


def main():
    if len(sys.argv) < 6:
        raise SystemExit(__doc__)
    esm_path, world, cx, cy, dump_path = sys.argv[1:6]
    cx, cy = int(cx), int(cy)

    esm = Esm(esm_path)
    target, pcell, refs = cell_children(esm, world, cx, cy)
    own = set(refs.get(target, ()))
    persistent = set(refs.get(pcell, ())) if pcell is not None else set()
    print('cell 0x%08X at %d,%d: %d references in its own child group'
          % (target, cx, cy, len(own)))
    print('worldspace persistent cell 0x%08X: %d references in the whole world'
          % (pcell or 0, len(persistent)))

    dump = set()
    for line in open(dump_path):
        if line.startswith('#') or not line.strip():
            continue
        dump.add(int(line.split()[0], 16))
    print('the viewer\'s list: %d references' % len(dump))

    missing = sorted(own - dump)
    extra = sorted(dump - own)
    not_persistent = [f for f in extra if f not in persistent]

    print('lost by the viewer     : %d %s'
          % (len(missing), ['0x%08X' % f for f in missing[:12]]))
    print('extra, from the persistent cell: %d'
          % len([f for f in extra if f in persistent]))
    print('extra, from NOWHERE the plugin explains: %d %s'
          % (len(not_persistent), ['0x%08X' % f for f in not_persistent[:12]]))

    ok = not missing and not not_persistent and len(dump) > 0
    print('RESULT %s own %d dump %d persistent-in-cell %d'
          % ('ok' if ok else 'bad', len(own), len(dump),
             len([f for f in extra if f in persistent])))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
