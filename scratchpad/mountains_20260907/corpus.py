"""LENS 1 completion: sweep ALL 2304 level-4 diffuse tiles and their _msn, and
show whether the brightness/saturation boundary coincides with the boundary of
the ESM's textured region measured in section 2.

Step 1 validates the coarse mip against mip 0 before trusting it.
"""
import os, sys, pickle, random, math, io
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dds

HERE = os.path.dirname(os.path.abspath(__file__))
T = r'E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth'
MIP = 4                      # 32x32
LO, HI, STEP = -96, 92, 4

out = io.open(os.path.join(HERE, 'corpus_out.txt'), 'w', encoding='utf-8')


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    out.write(s + '\n')


# ---- step 1: is mip 4 honest? ----
names = [f for f in os.listdir(T) if f.startswith('Commonwealth.4.') and not f.endswith('_msn.DDS')]
random.seed(5)
p('=== mip %d validated against mip 0 on 20 random level-4 tiles ===' % MIP)
dl = []
ds = []
for n in random.sample(names, 20):
    a = dds.stats(os.path.join(T, n), 0)
    b = dds.stats(os.path.join(T, n), MIP)
    dl.append(b['lum'] - a['lum'])
    ds.append(b['meanSat'] - a['meanSat'])
p('   delta lum      mean %+.2f  max |%.2f|' % (sum(dl) / len(dl), max(abs(x) for x in dl)))
p('   delta meanSat  mean %+.4f max |%.4f|' % (sum(ds) / len(ds), max(abs(x) for x in ds)))
p('   -> tolerance quoted for every number below.')
p()

# ---- step 2: full sweep ----
cells = pickle.load(open(os.path.join(HERE, 'landinfo.pkl'), 'rb'))
textured = set(k for k, v in cells.items() if v['BTXT'] or v['ATXT'])

grid = {}
for x in range(LO, HI + 1, STEP):
    for y in range(LO, HI + 1, STEP):
        dp = os.path.join(T, 'Commonwealth.4.%d.%d.DDS' % (x, y))
        np_ = os.path.join(T, 'Commonwealth.4.%d.%d_msn.DDS' % (x, y))
        if not os.path.exists(dp):
            continue
        s = dds.stats(dp, MIP)
        d = dds.DDS(np_)
        w, h, px = d.decode(MIP)
        n = w * h
        mr = sum(px[i * 4] for i in range(n)) / n
        mb = sum(px[i * 4 + 2] for i in range(n)) / n
        sr = math.sqrt(sum((px[i * 4] - mr) ** 2 for i in range(n)) / n)
        sb = math.sqrt(sum((px[i * 4 + 2] - mb) ** 2 for i in range(n)) / n)
        ntex = sum(1 for dx in range(4) for dy in range(4)
                   if (x + dx, y + dy) in textured)
        grid[(x, y)] = dict(lum=s['lum'], sat=s['meanSat'], std=s['lumStd'],
                            relief=math.hypot(sr, sb), ntex=ntex)

p('=== full level-4 sweep: %d tiles decoded at mip %d ===' % (len(grid), MIP))
with open(os.path.join(HERE, 'level4.csv'), 'w') as f:
    f.write('x,y,lum,meanSat,lumStd,msnRelief,texturedCells\n')
    for (x, y), v in sorted(grid.items()):
        f.write('%d,%d,%.2f,%.4f,%.2f,%.2f,%d\n'
                % (x, y, v['lum'], v['sat'], v['std'], v['relief'], v['ntex']))
p('wrote level4.csv')


def amap(key, title, lo, hi):
    p()
    p(title + '   (x -96 left..+92 right, y +92 top..-96 bottom)')
    ch = ' .:-=+*#@'
    for y in range(HI, LO - 1, -STEP):
        line = ''
        for x in range(LO, HI + 1, STEP):
            v = grid.get((x, y))
            if v is None:
                line += ' '
                continue
            t = (v[key] - lo) / (hi - lo)
            line += ch[max(0, min(len(ch) - 1, int(t * (len(ch) - 1))))]
        p('   ' + line)


amap('lum', 'A. luminance   (space=missing, . = %d, @ = %d)' % (40, 110), 40, 110)
amap('sat', 'B. mean saturation   (. = 0.05, @ = 0.32)', 0.05, 0.32)
amap('relief', 'C. _msn relief = hypot(stdR, stdB)   (. = 0, @ = 60)', 0, 60)
amap('ntex', 'D. cells with an ESM texture assignment, 0..16', 0, 16)

IN = [v for v in grid.values() if v['ntex'] == 16]
OUT = [v for v in grid.values() if v['ntex'] == 0]
MIX = [v for v in grid.values() if 0 < v['ntex'] < 16]


def ms(v, k):
    m = sum(x[k] for x in v) / len(v)
    sd = math.sqrt(sum((x[k] - m) ** 2 for x in v) / len(v))
    return m, sd


p()
p('=== whole corpus, split by the ESM texture boundary ===')
p('%-28s %6s %14s %16s %14s %14s' % ('group', 'tiles', 'lum', 'meanSat', 'lumStd', 'msn relief'))
for nm, g in (('TEXTURED (all 16 cells)', IN), ('MIXED', MIX), ('UNTEXTURED (0 cells)', OUT)):
    p('%-28s %6d  %6.2f+-%-5.2f  %6.4f+-%-6.4f  %5.2f+-%-5.2f  %5.1f+-%-5.1f'
      % ((nm, len(g)) + ms(g, 'lum') + ms(g, 'sat') + ms(g, 'std') + ms(g, 'relief')))
out.close()
