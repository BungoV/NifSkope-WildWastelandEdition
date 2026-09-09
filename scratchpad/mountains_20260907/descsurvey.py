"""Which vertex-descriptor flag combinations does VANILLA Fallout 4 actually
ship? If the stock engine renders a 32-byte object descriptor with
COLORS + UV_2 + EYEDATA somewhere in the shipped corpus, then the fatter
descriptor LODGEN writes is not, by itself, a stock-tolerance risk.

Read-only. Lane IDENTITY. Uses the same from-scratch NIF walker as nifpeek.py.
"""
import os, sys, struct, random, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nifpeek

FLAGS = [(0x0001, 'VERT'), (0x0002, 'UV'), (0x0004, 'UV2'), (0x0008, 'NRM'),
         (0x0010, 'TAN'), (0x0020, 'COL'), (0x0040, 'SKIN'), (0x0080, 'LAND'),
         (0x0100, 'EYE'), (0x0200, 'FULL')]

SHAPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape',
          'BSDynamicTriShape')

def descname(vf):
    return '+'.join(n for m, n in FLAGS if vf & m) or '(none)'

def main(root, limit, seed=1):
    files = []
    for dp, dn, fn in os.walk(root):
        for f in fn:
            if f.lower().endswith('.nif'):
                files.append(os.path.join(dp, f))
    print('%d .nif under %s' % (len(files), root))
    random.seed(seed)
    if len(files) > limit:
        files = random.sample(files, limit)
    combos = collections.Counter()
    strides = collections.Counter()
    examples = {}
    errs = 0
    for p in files:
        try:
            hdr, ver, user, bsver, strings, blocks, b, endoff = nifpeek.parse(p)
        except Exception:
            errs += 1
            continue
        for i, t, off, sz in blocks:
            if t not in SHAPES:
                continue
            try:
                o = off
                nameidx, o = nifpeek.u32(b, o)
                nex, o = nifpeek.u32(b, o); o += 4 * nex
                ctrl, o = nifpeek.u32(b, o)
                flags, o = nifpeek.u32(b, o)
                o += 12 + 36 + 4
                coll, o = nifpeek.u32(b, o)
                o += 16
                skin, o = nifpeek.u32(b, o)
                shader, o = nifpeek.u32(b, o)
                alpha, o = nifpeek.u32(b, o)
                desc, o = nifpeek.u64(b, o)
                nt, o = nifpeek.u32(b, o)
                nv, o = nifpeek.u16(b, o)
                ds, o = nifpeek.u32(b, o)
            except Exception:
                errs += 1
                continue
            stride = (desc & 0xF) * 4
            if ds != stride * nv + nt * 6:
                errs += 1          # layout mis-parse, do not trust this row
                continue
            vf = (desc >> 44) & 0x3FF
            key = descname(vf)
            combos[key] += 1
            strides[stride] += 1
            examples.setdefault(key, (p, '0x%X' % desc, stride))
    print('parsed shapes: %d, skipped/errors: %d' % (sum(combos.values()), errs))
    print()
    print('%-46s %8s %6s  example' % ('attribute set', 'shapes', 'stride'))
    for k, n in combos.most_common():
        p, d, st = examples[k]
        print('%-46s %8d %6d  %s %s' % (k, n, st, d, os.path.basename(p)))
    print()
    print('strides seen:', dict(sorted(strides.items())))

if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 4000)
