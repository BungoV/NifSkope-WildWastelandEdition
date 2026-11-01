"""Rank a build's objects by compile time (lane STARTSPEED1).

usage: python -I tools/ww_build_times.py <ctime log> [top N]
The log is what tools/ww_ctime.sh wrote (WW_BUILD_TIMES=<log> bash tools/ww_build.sh):
one line per compile, "<start ms> <end ms> <rc> <object>".
Prints wall span, summed compile seconds, mean parallelism and the slowest objects.
"""
import sys


def main():
    path = sys.argv[1]
    top = int(sys.argv[2]) if len(sys.argv) > 2 else 25
    rows = []
    for ln in open(path, encoding='utf-8', errors='replace'):
        p = ln.split()
        if len(p) < 4:
            continue
        try:
            s, e, rc = int(p[0]), int(p[1]), int(p[2])
        except ValueError:
            continue
        rows.append((e - s, s, e, rc, p[3]))
    if not rows:
        print('no compiles logged')
        return 1
    t0 = min(r[1] for r in rows)
    t1 = max(r[2] for r in rows)
    tot = sum(r[0] for r in rows)
    span = max(1, t1 - t0)
    fails = sum(1 for r in rows if r[3] != 0)
    print('compiles %d  failed %d  wall %.1f s  summed %.1f s  mean parallel %.2f'
          % (len(rows), fails, span / 1000.0, tot / 1000.0, tot / span))
    rows.sort(reverse=True)
    acc = 0
    for i, r in enumerate(rows[:top]):
        acc += r[0]
        print('%3d %7.1f s  %5.1f %%  cum %5.1f %%  %s' % (i + 1, r[0] / 1000.0, 100.0 * r[0] / tot, 100.0 * acc / tot, r[4]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
