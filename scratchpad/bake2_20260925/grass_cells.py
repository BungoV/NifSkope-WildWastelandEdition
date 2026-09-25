"""G4's independent grass-cell pick for ANY worldspace (BAKE2; BAKE1's grass_cells.py knew only Fallout4.esm LTEX and
worldspace 3C). Read only, no exe. A cell's LAND quadrant textures (BTXT, ATXT) name LTEX records; an LTEX whose last
override carries GNAM grass forms grows grass. Every form id (LTEX, the WRLD group label, CELL, BTXT/ATXT refs) is
resolved from the file's own master list to the LOAD-ORDER id (full plugins by position; light plugins 0xFE, never
LTEX owners here), so DLC and mod overrides join the right record.
usage: grass_cells.py <wrld load-order id hex> x0 y0 x1 y1 <plugin> [<plugin> ...]   (all plugins, in load order)"""
import sys, struct, os
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/mountains_20260907')
import fo4esm

WS = int(sys.argv[1], 16)
x0, y0, x1, y1 = (int(v) for v in sys.argv[2:6])
plugins = sys.argv[6:]
names = [os.path.basename(p).lower() for p in plugins]

def is_light(buf, path):
    fl = fo4esm.read_record_header(buf, 0)
    flags = struct.unpack_from('<I', buf, 8)[0]
    return path.lower().endswith('.esl') or bool(flags & 0x200)

gindex = {}          # plugin name -> load-order top byte (full plugins only)
n_full = 0
bufs = []
for p in plugins:
    b = fo4esm.load(p); bufs.append(b)
    if not is_light(b, p):
        gindex[os.path.basename(p).lower()] = n_full; n_full += 1

ltex_grass = {}; land_tex = {}
for path, buf in zip(plugins, bufs):
    me = os.path.basename(path).lower()
    if me not in gindex:
        continue
    tes4 = fo4esm.read_record_header(buf, 0)
    masters = [s[1].split(b'\0')[0].decode('latin1').lower() for s in fo4esm.subrecords(bytes(buf[24:24 + tes4[1]])) if s[0] == b'MAST']
    nm = len(masters)
    def g(fid):
        i = fid >> 24
        owner = masters[i] if i < nm else me
        if owner not in gindex:
            return None
        return (gindex[owner] << 24) | (fid & 0xFFFFFF)
    grid = {}; lands = []
    def on_record(off, sig, dsize, flags, formid, tail, stack):
        if sig == b'LTEX':
            p = fo4esm.record_payload(buf, off, dsize, flags)
            ltex_grass[g(formid)] = sum(1 for s in fo4esm.subrecords(p) if s[0] == b'GNAM')
        elif sig == b'CELL':
            if not any(n.gtype == 1 and g(struct.unpack('<I', n.label)[0]) == WS for n in stack):
                return
            p = fo4esm.record_payload(buf, off, dsize, flags)
            for s in fo4esm.subrecords(p):
                if s[0] == b'XCLC':
                    grid[g(formid)] = struct.unpack('<ii', s[1][:8])
        elif sig == b'LAND':
            if not any(n.gtype == 1 and g(struct.unpack('<I', n.label)[0]) == WS for n in stack):
                return
            cg = [n for n in stack if n.gtype in (6, 8, 9)]
            if not cg:
                return
            cell = g(struct.unpack('<I', cg[-1].label)[0])
            p = fo4esm.record_payload(buf, off, dsize, flags)
            base, layer = [], []
            for s in fo4esm.subrecords(p):
                if s[0] == b'BTXT': base.append(g(struct.unpack('<I', s[1][:4])[0]))
                elif s[0] == b'ATXT': layer.append(g(struct.unpack('<I', s[1][:4])[0]))
            lands.append((cell, base, layer))
    fo4esm.walk(buf, 24 + tes4[1], len(buf), [], on_record)
    got = 0
    for cell, base, layer in lands:
        if cell in grid:
            gx, gy = grid[cell]
            if x0 <= gx <= x1 and y0 <= gy <= y1:
                land_tex[(gx, gy)] = (base, layer); got += 1
    if got or grid:
        print('# %s: masters %d, LTEX known %d, WS cells %d, LAND in region from this file %d' % (me, nm, len(ltex_grass), len(grid), got))

grassy = lambda fid: ltex_grass.get(fid, 0) > 0
rows = []
for (gx, gy), (base, layer) in sorted(land_tex.items()):
    allt = base + layer
    share = sum(1 for t in allt if grassy(t)) / float(len(allt)) if allt else 0.0
    bshare = sum(1 for t in base if grassy(t)) / float(len(base)) if base else 0.0
    rows.append((gx, gy, bshare, share, len(base), len(layer)))
for r in rows:
    print('cell %d %d base-grass %.2f all-grass %.2f (%d base, %d layers)' % r)
full = sorted([r for r in rows if r[2] == 1.0 and r[3] >= 0.75], key=lambda r: -r[3])
none = [r for r in rows if r[3] == 0.0 and r[4] + r[5] > 0]
print('# grass cells (all 4 base quadrants grass, >= 75%% of every texture): %d of %d %s' % (len(full), len(rows), [(r[0], r[1]) for r in full[:12]]))
print('# no-grass painted cells: %d %s' % (len(none), [(r[0], r[1]) for r in none[:12]]))
