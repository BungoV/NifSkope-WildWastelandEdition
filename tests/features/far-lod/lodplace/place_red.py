"""LODPLACE1: the place census's reds. Each writes a copy of a green --place dump with ONE fault class put in, the way
the bugs this census hunts would put it there, so place_census.py must FAIL on it:
  origin   every far object row one cell origin off (FIREFX1's defect: p.pos - origin, here x - 4096 * 15)
  flip     every far object row turned upside down (up z negated, its box mirrored about its origin's ground)
  under    every far object row and terrain vertex 6000 u lower (a z sign / offset applied twice)
python -I place_red.py <green dump> <out dump> origin|flip|under
"""
import sys

src, dst, kind = sys.argv[1], sys.argv[2], sys.argv[3]
assert kind in ('origin', 'flip', 'under')
out = []
n = 0
for line in open(src, encoding='utf-8', errors='replace'):
    f = line.split()
    if f and f[0] == 'I':
        x, y, zlo, zhi, upz = float(f[5]), float(f[6]), float(f[7]), float(f[8]), float(f[9])
        if kind == 'origin':
            x -= 4096.0 * 15
        elif kind == 'flip':
            upz = -upz if upz > 0.1 else upz
        else:
            zlo, zhi = zlo - 6000.0, zhi - 6000.0
        f[5:10] = ['%.1f' % x, '%.1f' % y, '%.1f' % zlo, '%.1f' % zhi, '%.4f' % upz]
        line = ' '.join(f) + '\n'
        n += 1
    elif f and f[0] == 'V' and kind == 'under':
        f[4] = '%.1f' % (float(f[4]) - 6000.0)
        line = ' '.join(f) + '\n'
    out.append(line)
open(dst, 'w', encoding='utf-8', newline='\n').write(''.join(out))
print('place red %s: %d far rows changed -> %s' % (kind, n, dst))
