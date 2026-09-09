"""Go/no-go for recovering out-of-bounds materials by colour matching.

bungo's proposal: the out-of-bounds cells have no painted layers, but vanilla
shipped a real LOD diffuse for them. Where we DO have layers (the painted area)
we know both the colour and the blend that made it. So learn colour -> blend
there, then look up the out-of-bounds colours and populate those cells with the
matching materials.

The whole idea rests on one thing nobody has checked: **do the out-of-bounds
colours actually occur inside the painted area?** If the far terrain is painted
from textures that appear nowhere in the playable Commonwealth, there is no
match to find and the scheme cannot start.

This measures exactly that, and nothing else. It does NOT attempt the recovery.

Method: sample vanilla level-4 diffuse texels from tiles known to have layer
data and from tiles far outside, quantise the painted set into a 64x64x64
occupancy grid, then for every out-of-bounds texel find the distance to the
nearest occupied painted colour by expanding-shell search. Reports the
distribution of that distance, plus how concentrated the out-of-bounds palette
is (if the far terrain is two tiled textures, this is an afternoon's work; if it
is a hundred, it is a project).
"""
import os, random, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dds import DDS

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
LEVEL = 4
MIP = 3            # 64x64 per tile: 4096 texels, plenty and cheap
SAMPLE_TILES = 48  # per side
BIN = 2            # quantise to 6 bits a channel -> 64^3 grid


def tiles():
    pat = re.compile(r'^Commonwealth\.%d\.(-?\d+)\.(-?\d+)\.DDS$' % LEVEL, re.I)
    out = []
    for n in os.listdir(VAN):
        m = pat.match(n)
        if m:
            out.append((int(m.group(1)), int(m.group(2)), os.path.join(VAN, n)))
    return out


def texels(path):
    d = DDS(path)
    mip = min(MIP, d.mips - 1)
    w, h, px = d.decode(mip)
    return [(px[i * 4], px[i * 4 + 1], px[i * 4 + 2]) for i in range(w * h)]


def main():
    all_t = tiles()
    # PAINTED: the block our dim-32 bake produced real content for (cells -32..31).
    # Conservative on purpose - every tile here is one we can definitely pair a
    # colour with a real blend for.
    painted = [t for t in all_t if -32 <= t[0] < 32 and -32 <= t[1] < 32]
    # OUT: comfortably beyond the painted rectangle on both axes.
    out = [t for t in all_t if abs(t[0]) >= 80 or abs(t[1]) >= 80]
    random.seed(1234)
    random.shuffle(painted)
    random.shuffle(out)
    painted, out = painted[:SAMPLE_TILES], out[:SAMPLE_TILES]
    print('painted tiles sampled: %d   out-of-bounds tiles sampled: %d' % (len(painted), len(out)))

    grid = set()
    pcount = 0
    for x, y, p in painted:
        for c in texels(p):
            grid.add((c[0] >> BIN, c[1] >> BIN, c[2] >> BIN))
            pcount += 1
    print('painted texels: %d   distinct %d-bit colour bins: %d'
          % (pcount, 8 - BIN, len(grid)))

    ocols = {}
    ocount = 0
    for x, y, p in out:
        for c in texels(p):
            k = (c[0] >> BIN, c[1] >> BIN, c[2] >> BIN)
            ocols[k] = ocols.get(k, 0) + 1
            ocount += 1
    print('out-of-bounds texels: %d   distinct bins: %d' % (ocount, len(ocols)))

    # how concentrated is the far palette?
    top = sorted(ocols.items(), key=lambda kv: -kv[1])
    cum = 0
    for frac in (0.5, 0.8, 0.95):
        need = 0
        cum = 0
        for k, v in top:
            cum += v
            need += 1
            if cum >= frac * ocount:
                break
        print('  %.0f%% of out-of-bounds texels fall in %d bins' % (frac * 100, need))

    # nearest painted colour, by expanding shell in bin space
    def nearest(k, cap=24):
        if k in grid:
            return 0
        for r in range(1, cap + 1):
            for dx in range(-r, r + 1):
                for dy in range(-r, r + 1):
                    for dz in range(-r, r + 1):
                        if max(abs(dx), abs(dy), abs(dz)) != r:
                            continue
                        if (k[0] + dx, k[1] + dy, k[2] + dz) in grid:
                            return r
        return cap + 1

    cache, hist = {}, {}
    for k, v in ocols.items():
        if k not in cache:
            cache[k] = nearest(k)
        hist[cache[k]] = hist.get(cache[k], 0) + v

    print()
    print('distance from each out-of-bounds colour to the NEAREST painted colour')
    print('(Chebyshev in %d-bit bins; 1 bin = %d of 255 per channel)' % (8 - BIN, 1 << BIN))
    run = 0
    for r in sorted(hist):
        run += hist[r]
        print('  within %2d bin(s) (%3d/255): %6.2f%% cumulative'
              % (r, r * (1 << BIN), 100.0 * run / ocount))


if __name__ == '__main__':
    main()
