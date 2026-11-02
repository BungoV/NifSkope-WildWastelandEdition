"""Compare two folders of program binaries (shader_link_bench.py --savebin): per program, same size and the count of
differing bytes. The NV binary differs between two links of new sources only in the run stamp (bytes <= 80) and an
8-byte source hash (skill ww-shader-link-limit 4); a nonce define per run moves that hash, so up to ~16 bytes may differ.

    python -I bincmp.py <dirA> <dirB> [--maxdiff 24]
Prints SAME/DIFF per program and a total line; exit 0 when every program is within --maxdiff bytes and the same size.
"""
import os, sys

def main():
    a, b = sys.argv[1], sys.argv[2]
    maxd = int(sys.argv[sys.argv.index('--maxdiff') + 1]) if '--maxdiff' in sys.argv else 24
    names = sorted(set(os.listdir(a)) | set(os.listdir(b)))
    bad = 0
    for n in names:
        pa, pb = os.path.join(a, n), os.path.join(b, n)
        if not (os.path.exists(pa) and os.path.exists(pb)):
            print('MISSING', n); bad += 1; continue
        x, y = open(pa, 'rb').read(), open(pb, 'rb').read()
        d = sum(1 for i in range(min(len(x), len(y))) if x[i] != y[i])
        ok = len(x) == len(y) and d <= maxd
        bad += 0 if ok else 1
        if not ok:
            print('DIFF %s size %d vs %d, %d bytes differ' % (n, len(x), len(y), d))
    print('BINCMP %d programs, %d outside (size or > %d bytes)' % (len(names), bad, maxd))
    return 0 if bad == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
