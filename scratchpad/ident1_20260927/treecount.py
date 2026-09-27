"""IDENT1: does a file-wide u16 group id fit the whole Commonwealth?
Lower bound on file-wide groups = placements that can NEVER join (trees, placements with no drawn mesh),
because each is its own group under any rule. Prints one verdict block."""
import struct, sys
lodi, lodo = sys.argv[1], sys.argv[2]
I = open(lodi, 'rb').read()
O = open(lodo, 'rb').read()
ver, = struct.unpack_from('<I', I, 4)
cc, ic = struct.unpack_from('<II', I, 0x54)
offChunk, offCell, offInst, offCold = struct.unpack_from('<QQQQ', I, 0x68)
offGroup, = struct.unpack_from('<Q', I, 0x100)
gc, = struct.unpack_from('<I', I, 0x108)
baseCount, = struct.unpack_from('<I', O, 0x50)
offBases, = struct.unpack_from('<Q', O, 0x70)
tree = []; nomesh = []
for b in range(baseCount):
    o = offBases + 32 * b
    rep = struct.unpack_from('<4H', O, o + 8)
    fl, = struct.unpack_from('<H', O, o + 18)
    tree.append(bool(fl & 1))
    nomesh.append(all(r == 0xFFFF for r in rep))
nt = nm = 0
keys = set()
for i in range(ic):
    bid, = struct.unpack_from('<H', I, offInst + 24 * i + 0x0E)
    ref, part = struct.unpack_from('<Ih', I, offCold + 8 * i)
    if tree[bid] or nomesh[bid]:
        # a SCOL part joins its SCOL (clause i) whatever it is; anything else that cannot join is alone
        keys.add(('scol', ref) if part >= 0 else ('alone', i))
    if tree[bid]: nt += 1
    elif nomesh[bid]: nm += 1
print('never-join keys (trees/no-mesh; SCOL parts counted once per SCOL): %d' % len(keys))
# per-chunk groups and singletons from the table
g = struct.unpack_from('<%dH' % ic, I, offGroup)
sing = 0; groups = 0
for c in range(cc):
    first, cnt = struct.unpack_from('<II', I, offChunk + 32 * c)
    if cnt == 0: continue
    m = {}
    for k in range(first, first + cnt):
        m[g[k]] = m.get(g[k], 0) + 1
    groups += len(m); sing += sum(1 for v in m.values() if v == 1)
print('lodi v%d instances %d groupCount(header) %d groups(recount) %d singletons %d' % (ver, ic, gc, groups, sing))
print('trees %d  no-mesh non-tree %d  => never-join lower bound %d  (u16 id space 65536)' % (nt, nm, nt + nm))
print('non-singleton groups %d' % (groups - sing))
