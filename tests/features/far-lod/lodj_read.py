"""Read a `.lodj` -- the per-chunk native cache the FO4CS bake writes beside
`<ws>.lodo` and `<ws>.lodi` (lane INCR1, 2026-09-17).

WHY THERE IS A FILE AT ALL. `--incremental` hands the chunk pass only the dirty
chunks, but the `.lodo`/`.lodi` pair is AGGREGATED from every chunk's placements,
so a skipped chunk would simply be missing from it. The cache is the skipped
chunk's contribution, kept from the bake that last built it: every placement it
emitted, in emission order, and the exact lighting sums it accumulated.

WHY EVERY NUMBER IS HEX. The pair the replay rebuilds has to be BIT-identical to
the one a full bake writes, and a decimal round trip of a float is not. So
positions, rotations and scale are `%08x` IEEE-754 bit patterns and the lighting
sums are `%016x` doubles. This reader hands them back as floats; use `raw()` when
what matters is the bytes rather than the value.

THE FORMAT -- UTF-8, LF, `key<TAB>fields`, like every other text file this tree
writes:

    lodj<TAB>1<TAB><ws><TAB><dim><TAB><cx><TAB><cy>
    placements<TAB><n>
    p<TAB>base<TAB>ref<TAB>part<TAB>pos*3<TAB>rot*9<TAB>scale<TAB>slot
      <TAB>isTree<TAB>mirrorU<TAB>treeHash<TAB>hasAlpha<TAB>emits
      <TAB>objectIndex<TAB>model
    lit<TAB><n>
    l<TAB>objectIndex<TAB>aoSum<TAB>skySum<TAB>groundSum<TAB>verts
    pao<TAB><n>
    a<TAB>objectIndex<TAB>paoSum<TAB>rays
    end<TAB><placements><TAB><lit><TAB><pao>

The model path is the LAST field and is whatever is left of the line, because a
path may hold anything a path may hold.

USAGE
    python lodj_read.py <file.lodj> [--verbose]
    from lodj_read import read; c = read(path)
"""
import struct
import sys


def _f32(h):
    return struct.unpack('<f', struct.pack('<I', int(h, 16)))[0]


def _f64(h):
    return struct.unpack('<d', struct.pack('<Q', int(h, 16)))[0]


class Cache(object):
    """One chunk's contribution to the region's `.lodo`/`.lodi` pair."""

    def __init__(self):
        self.path = ''
        self.version = 0
        self.worldspace = ''
        self.dim = 0
        self.cx = 0
        self.cy = 0
        self.placements = []    # list of dict
        self.lit = []           # list of dict
        self.pao = []           # list of dict
        self.end = None         # (placements, lit, pao) as the file promises
        self.raw = []           # every line, unparsed

    def agrees(self):
        """Does the end line match what the file actually carries?"""
        return self.end == (len(self.placements), len(self.lit), len(self.pao))


def read(path):
    c = Cache()
    c.path = path
    with open(path, 'rb') as fh:
        data = fh.read()
    if data.count(b'\r') != 0:
        raise ValueError('%s is not LF-only (%d CR bytes)'
                         % (path, data.count(b'\r')))
    lines = [l for l in data.decode('utf-8').split('\n') if l != '']
    c.raw = list(lines)
    if not lines:
        raise ValueError('%s is empty' % path)
    h = lines[0].split('\t')
    if h[0] != 'lodj':
        raise ValueError('%s does not start with a lodj header line' % path)
    if len(h) < 6:
        raise ValueError('%s has a short header line (%d fields)' % (path, len(h)))
    c.version = int(h[1])
    c.worldspace = h[2]
    c.dim, c.cx, c.cy = int(h[3]), int(h[4]), int(h[5])
    for line in lines[1:]:
        f = line.split('\t')
        k = f[0]
        if k == 'p':
            if len(f) < 25:
                raise ValueError('%s: a placement row of %d fields' % (path, len(f)))
            c.placements.append({
                'baseForm': int(f[1], 16),
                'refForm': int(f[2], 16),
                'scolPart': int(f[3]),
                'pos': [_f32(x) for x in f[4:7]],
                'rot': [_f32(x) for x in f[7:16]],
                'scale': _f32(f[16]),
                'slot': int(f[17]),
                'isTree': f[18] == '1',
                'mirrorU': f[19] == '1',
                'treeHash': int(f[20], 16),
                'hasAlpha': f[21] == '1',
                'emits': f[22] == '1',
                'objectIndex': int(f[23]),
                'model': '\t'.join(f[24:]),
                'raw': line,
            })
        elif k == 'l':
            c.lit.append({'objectIndex': int(f[1]), 'ao': _f64(f[2]),
                          'sky': _f64(f[3]), 'ground': _f64(f[4]),
                          'verts': int(f[5]), 'raw': line})
        elif k == 'a':
            c.pao.append({'objectIndex': int(f[1]), 'ao': _f64(f[2]),
                          'rays': int(f[3]), 'raw': line})
        elif k == 'end':
            c.end = (int(f[1]), int(f[2]), int(f[3]))
    return c


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    c = read(argv[1])
    print('lodj v%d  %s  dim %d  chunk (%d,%d)'
          % (c.version, c.worldspace, c.dim, c.cx, c.cy))
    print('  placements %d, lit rows %d, placement-ao rows %d, end agrees %s'
          % (len(c.placements), len(c.lit), len(c.pao), c.agrees()))
    trees = sum(1 for p in c.placements if p['isTree'])
    models = len(set(p['model'] for p in c.placements))
    print('  %d tree(s), %d distinct model path(s)' % (trees, models))
    if '--verbose' in argv:
        for p in c.placements[:10]:
            print('    %08x/%08x part %d  %s' % (p['baseForm'], p['refForm'],
                                                 p['scolPart'], p['model']))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
