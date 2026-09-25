"""The PAINTED set over his LOAD ORDER (coordinator order 16:2x, north-east gate): for every Commonwealth cell, the
LAND record of the LAST plugin in the bake's list that carries one (a later LAND overrides the whole record), and
whether it is painted by the C++ definition (lodgenVtFillPainted: a quadrant with BTXT != 0 or any ATXT layer).
Plugins = BAKE1's esm_list.txt (the list the bake's --mo2-profile resolves). Writes land_lo.pkl:
  painted  set of (cx, cy)      winner  {(cx, cy): plugin}     esm  the Fallout4.esm-only painted set (w2_cells)
usage: land_lo.py"""
import sys, os, struct, pickle, collections, time
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/mountains_20260907')
import fo4esm
PLUGINS = open('E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/esm_list.txt').read().strip().split(',')
WS = 0x3C
winner, paint = {}, {}
t0 = time.time()
for path in PLUGINS:
    name = path.split('/')[-1]
    buf = fo4esm.load(path)
    grid = {}; lands = []
    def on_record(off, sig, dsize, flags, formid, tail, stack):
        if sig == b'CELL':
            if not any(n.gtype == 1 and struct.unpack('<I', n.label)[0] == WS for n in stack): return
            for s in fo4esm.subrecords(fo4esm.record_payload(buf, off, dsize, flags)):
                if s[0] == b'XCLC': grid[formid] = struct.unpack('<ii', s[1][:8])
        elif sig == b'LAND':
            if not any(n.gtype == 1 and struct.unpack('<I', n.label)[0] == WS for n in stack): return
            cg = [n for n in stack if n.gtype in (6, 8, 9)]
            if not cg: return
            p = False
            for s in fo4esm.subrecords(fo4esm.record_payload(buf, off, dsize, flags)):
                if s[0] == b'BTXT' and struct.unpack('<I', s[1][:4])[0] != 0: p = True
                elif s[0] == b'ATXT': p = True
            lands.append((struct.unpack('<I', cg[-1].label)[0], p))
    for g in fo4esm.top_level_groups(buf):
        if g.label == b'WRLD':
            fo4esm.walk(buf, g.offset + 24, g.offset + g.gsize, [g], on_record)
    n = 0
    for cell, p in lands:
        if cell in grid:
            winner[grid[cell]] = name; paint[grid[cell]] = p; n += 1
    print('%-40s LAND in Commonwealth %6d  (%.0f s)' % (name, n, time.time() - t0), flush=True)
painted = {c for c, p in paint.items() if p}
esm = pickle.load(open(HERE + '/w2_cells.pkl', 'rb'))['painted']
print('painted: load order %d, Fallout4.esm only %d | load order adds %d, removes %d' % (
    len(painted), len(esm), len(painted - esm), len(esm - painted)))
by = collections.Counter(winner[c] for c in painted - esm)
print('added cells by winning plugin:', dict(by))
for c in [(12, 30), (11, 30), (12, 31), (13, 31)]:
    print('cell', c, 'winner', winner.get(c), 'painted lo', c in painted, 'esm', c in esm)
pickle.dump({'painted': painted, 'winner': winner, 'esm': esm}, open(HERE + '/land_lo.pkl', 'wb'))
