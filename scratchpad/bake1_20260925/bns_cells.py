import sys, collections, struct
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/mountains_20260907')
import fo4esm
buf = fo4esm.load(sys.argv[1])
tes4 = fo4esm.read_record_header(buf, 0)
grid = {}
per = collections.Counter()
def on_record(off, sig, dsize, flags, formid, tail, stack):
    if sig == b'CELL':
        p = fo4esm.record_payload(buf, off, dsize, flags)
        for s in fo4esm.subrecords(p):
            if s[0] == b'XCLC':
                grid[formid] = struct.unpack('<ii', s[1][:8])
    elif sig == b'REFR':
        cg = [n for n in stack if n.gtype in (8, 9, 10)]
        if cg:
            per[struct.unpack('<I', cg[-1].label)[0]] += 1
fo4esm.walk(buf, 24 + tes4[1], len(buf), [], on_record)
print('cells with refs', len(per), 'cells with grid', sum(1 for c in per if c in grid))
for c, n in per.most_common(15):
    print('%08X' % c, grid.get(c), n)
# 4x4-cell block density (dim 4 chunks aligned to 4)
blk = collections.Counter()
for c, n in per.items():
    if c in grid:
        blk[(grid[c][0] // 4 * 4, grid[c][1] // 4 * 4)] += n
print('densest dim-4 chunks', blk.most_common(8))
