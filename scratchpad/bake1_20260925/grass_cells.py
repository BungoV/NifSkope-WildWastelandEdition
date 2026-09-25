"""G4's independent grass-cell pick (read only, no exe): which Commonwealth cells are GRASS cells on bungo's load order,
from the plugins themselves -- a cell's LAND quadrant textures (BTXT, ATXT) name LTEX records, and an LTEX whose last
override carries GNAM grass forms grows grass. Later plugins override LTEX (and LAND) by form id.
usage: grass_cells.py x0 y0 x1 y1 <plugin> [<plugin> ...]   (plugins in load order; Fallout4.esm first)
prints per cell: grid, quadrant base LTEX, share of quadrant/layer textures that carry grass."""
import sys, struct, collections
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/mountains_20260907')
import fo4esm

x0, y0, x1, y1 = (int(v) for v in sys.argv[1:5])
plugins = sys.argv[5:]
ltex_grass = {}            # ltex formid -> number of GNAM entries (last override wins)
land_tex = {}              # grid -> (base ltex ids, layer ltex ids), last override wins
WS = 0x0000003C

for path in plugins:
    buf = fo4esm.load(path)
    tes4 = fo4esm.read_record_header(buf, 0)
    masters = [s for s in fo4esm.subrecords(bytes(buf[24:24 + tes4[1]])) if s[0] == b'MAST']
    nm = len(masters)
    selfidx = None  # a form whose file index >= nm names this plugin; ids of Fallout4.esm records keep index 0
    grid = {}
    lands = []
    nltex = nland = 0

    def on_record(off, sig, dsize, flags, formid, tail, stack):
        if sig == b'LTEX':
            p = fo4esm.record_payload(buf, off, dsize, flags)
            g = sum(1 for s in fo4esm.subrecords(p) if s[0] == b'GNAM')
            if (formid >> 24) == 0:   # a Fallout4.esm LTEX or an override of one (index 0 = Fallout4.esm in every file here)
                ltex_grass[formid] = g
        elif sig == b'CELL':
            if not any(n.gtype == 1 and struct.unpack('<I', n.label)[0] == WS for n in stack):
                return   # another worldspace's cell at the same grid
            p = fo4esm.record_payload(buf, off, dsize, flags)
            for s in fo4esm.subrecords(p):
                if s[0] == b'XCLC':
                    grid[formid] = struct.unpack('<ii', s[1][:8])
        elif sig == b'LAND':
            cg = [n for n in stack if n.gtype in (6, 8, 9)]
            if not cg:
                return
            cell = struct.unpack('<I', cg[-1].label)[0]
            p = fo4esm.record_payload(buf, off, dsize, flags)
            base, layer = [], []
            for s in fo4esm.subrecords(p):
                if s[0] == b'BTXT':
                    base.append(struct.unpack('<I', s[1][:4])[0])
                elif s[0] == b'ATXT':
                    layer.append(struct.unpack('<I', s[1][:4])[0])
            lands.append((cell, base, layer))

    fo4esm.walk(buf, 24 + tes4[1], len(buf), [], on_record)
    got = 0
    for cell, base, layer in lands:
        if cell in grid:
            gx, gy = grid[cell]
            if x0 <= gx <= x1 and y0 <= gy <= y1:
                land_tex[(gx, gy)] = (base, layer)
                got += 1
    print('# %s: masters %d, LTEX total now %d, LAND in region from this file %d' % (path.split('/')[-1], nm, len(ltex_grass), got))

def grassy(fid):
    return ltex_grass.get(fid & 0x00FFFFFF if (fid >> 24) == 0 else fid, 0) > 0

rows = []
for (gx, gy), (base, layer) in sorted(land_tex.items()):
    allt = base + layer
    share = sum(1 for t in allt if grassy(t)) / float(len(allt)) if allt else 0.0
    bshare = sum(1 for t in base if grassy(t)) / float(len(base)) if base else 0.0
    rows.append((gx, gy, bshare, share, len(base), len(layer)))
for r in rows:
    print('cell %d %d base-grass %.2f all-grass %.2f (%d base, %d layers)' % r)
full = [r for r in rows if r[2] == 1.0 and r[3] >= 0.75]
print('# grass cells (all 4 base quadrants grass, >= 75% of every texture):', len(full), 'of', len(rows), [(r[0], r[1]) for r in full[:12]])
