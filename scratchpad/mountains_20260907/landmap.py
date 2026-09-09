"""Map which Commonwealth cells carry texture data, and correlate that with the
brightness/saturation/flatness of the LOD tile vanilla shipped for them."""
import pickle, os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dds

HERE = os.path.dirname(os.path.abspath(__file__))
T = r'E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth'
cells = pickle.load(open(os.path.join(HERE, 'landinfo.pkl'), 'rb'))

LO, HI = -96, 95


def amap(pred, title):
    print()
    print(title + '   (x -96 left .. +95 right, y +95 top .. -96 bottom; 4 cells per char)')
    rows = []
    for by in range(HI, LO - 1, -4):
        line = ''
        for bx in range(LO, HI + 1, 4):
            n = 0
            for dy in range(4):
                for dx in range(4):
                    c = cells.get((bx + dx, by - dy))
                    if c and pred(c):
                        n += 1
            line += ' .:-=+*#@'[min(8, n * 8 // 16)] if n else ' '
        rows.append(line)
    for r in rows:
        print('   ' + r)


amap(lambda c: c['BTXT'] > 0, 'A. cells with a BTXT base texture')
amap(lambda c: c['ATXT'] > 0, 'B. cells with ATXT/VTXT alpha layers')
amap(lambda c: c['VCLR'] > 0, 'C. cells with VCLR hand-painted vertex colour')

# bounding box of the textured region
tx = [k for k, v in cells.items() if v['BTXT'] or v['ATXT']]
xs = [k[0] for k in tx]; ys = [k[1] for k in tx]
print()
print('cells with ANY texture assignment: %d of %d (%.1f%%)'
      % (len(tx), len(cells), 100.0 * len(tx) / len(cells)))
print('their bounding box: x %d..%d  y %d..%d' % (min(xs), max(xs), min(ys), max(ys)))
inside = set(tx)

# what base textures are used out there, if any
basec = collections.Counter()
for k, v in cells.items():
    for b in v['base']:
        basec[b] += 1
print()
print('distinct BTXT LTEX formids used: %d ; top 10 by cell count:' % len(basec))
for f, n in basec.most_common(10):
    print('    %08X  %d cells' % (f, n))

# now: vanilla LOD tile stats, inside vs outside the textured region
print()
print('=== vanilla shipped LOD-4 tiles: textured cells vs untextured cells ===')


def tilecells(bx, by):
    return [(bx + dx, by + dy) for dx in range(4) for dy in range(4)]


import random
random.seed(11)
tiles = [(x, y) for x in range(LO, HI, 4) for y in range(LO, HI, 4)]
IN, OUT = [], []
for (bx, by) in tiles:
    cc = tilecells(bx, by)
    ntex = sum(1 for c in cc if c in inside)
    (IN if ntex == 16 else OUT if ntex == 0 else None) is None or None
    if ntex == 16:
        IN.append((bx, by))
    elif ntex == 0:
        OUT.append((bx, by))
print('tiles fully inside the textured region: %d ; fully outside: %d ; mixed: %d'
      % (len(IN), len(OUT), len(tiles) - len(IN) - len(OUT)))

for name, group in (('TEXTURED (playable)', IN), ('UNTEXTURED (outer)', OUT)):
    samp = random.sample(group, min(40, len(group)))
    L = []; S = []; V = []; NR = []
    for (bx, by) in samp:
        p = os.path.join(T, 'Commonwealth.4.%d.%d.DDS' % (bx, by))
        pn = os.path.join(T, 'Commonwealth.4.%d.%d_msn.DDS' % (bx, by))
        if not os.path.exists(p):
            continue
        s = dds.stats(p, 3)          # mip 3 = 64x64
        L.append(s['lum']); S.append(s['meanSat']); V.append(s['lumStd'])
        d = dds.DDS(pn); w, h, px = d.decode(3)
        n = w * h
        mr = sum(px[i * 4] for i in range(n)) / n
        mb = sum(px[i * 4 + 2] for i in range(n)) / n
        sr = (sum((px[i * 4] - mr) ** 2 for i in range(n)) / n) ** 0.5
        sb = (sum((px[i * 4 + 2] - mb) ** 2 for i in range(n)) / n) ** 0.5
        NR.append((sr * sr + sb * sb) ** 0.5)

    def ms(v):
        m = sum(v) / len(v)
        sd = (sum((x - m) ** 2 for x in v) / len(v)) ** 0.5
        return m, sd
    print('%-22s n=%2d  lum %5.1f+-%4.1f   meanSat %.3f+-%.3f   lumStd %5.2f+-%4.2f   msn relief %5.1f+-%4.1f'
          % ((name, len(L)) + ms(L) + ms(S) + ms(V) + ms(NR)))
