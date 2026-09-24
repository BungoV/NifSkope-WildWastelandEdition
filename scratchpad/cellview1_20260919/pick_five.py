"""Pick five named references out of the INDEPENDENT Python walk.

Gate row 4 of tests/spells/cell_open.sh needs five references by form id with
the world position the plugin gives them. They must come from HERE -- a reader
that shares no code with src/esmdata.cpp -- and never out of a dump the viewer
produced, because a reference chosen from our own output only proves the code
agrees with itself.

Five DIFFERENT record types where the cell has them, so a row that only ever
reads STAT fails at least four of them.

  python pick_five.py <Fallout4.esm> <worldEdid> <cx> <cy>
"""
import struct
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))), 'tests', 'spells'))
import cell_census as cc

esm_path, world, cx, cy = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
esm = cc.Esm(esm_path)
bases, scols = cc.index_bases(esm)
wform, cellxy, persistent = cc.cells_of_world(esm, world)

xy2form = {xy: f for f, xy in cellxy.items()}
cf = xy2form.get((cx, cy))
if cf is None:
    raise SystemExit('no cell %d,%d in %s' % (cx, cy, world))

rows = []
for cellform, rec in persistent:
    if cellform != cf:
        continue
    base = None
    pos = rot = None
    scale = 1.0
    for t, p in cc.refr_fields(esm, rec):
        if t == b'NAME' and len(p) >= 4:
            base = struct.unpack_from('<I', p, 0)[0]
        elif t == b'DATA' and len(p) >= 24:
            pos = struct.unpack_from('<fff', p, 0)
            rot = struct.unpack_from('<fff', p, 12)
        elif t == b'XSCL' and len(p) >= 4:
            scale = struct.unpack_from('<f', p, 0)[0]
    if base is None or pos is None:
        continue
    b = bases.get(base)
    if not b:
        continue
    btype, modl, edid = b
    if btype != b'SCOL' and not modl:
        continue
    rows.append((btype.decode(), rec.form, base, edid, modl, pos, rot, scale))

bytype = {}
for r in rows:
    bytype.setdefault(r[0], []).append(r)

print('# cell %d,%d of %s: %d drawable refs, types %s'
      % (cx, cy, world, len(rows),
         ' '.join('%s=%d' % (k, len(v)) for k, v in sorted(bytype.items()))))
print('# type  refForm  baseForm  x y z  rx ry rz  scale  edid  model')
picked = []
for t in sorted(bytype):
    # the LOWEST form id of each type: a stable, arbitrary choice that does not
    # depend on walk order, so a re-run picks the same five.
    picked.append(sorted(bytype[t], key=lambda r: r[1])[0])
for r in sorted(picked, key=lambda r: r[1])[:8]:
    t, form, base, edid, modl, pos, rot, scale = r
    print('%-5s 0x%08X 0x%08X  %.2f %.2f %.2f  %.4f %.4f %.4f  %.3f  %s  %s'
          % (t, form, base, pos[0], pos[1], pos[2], rot[0], rot[1], rot[2],
             scale, edid or '-', modl or '(SCOL)'))
