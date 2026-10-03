"""Lane SPEED1 (2026-10-02): the independent checker of tests/spells/cell_speed_bake.sh.

No NifSkope code. For one cell, the run that loaded everything (<out>/<cell>.all) against the run that
did not load what the probe soup leaves out (<out>/<cell>.lean):

  SAME     the bake's sector files (bake/*), probes.tsv, soup.psp and souprefs.tsv: the same file names and
           the same bytes (sha256) in both runs, at least one sector file, none of them empty.
  SKIPPED  the lean run says how many placements it did not load; that is at least the sum of the soup's
           own "refs left out by type" row, and that row reads the same in both runs.
  TIME     wall seconds and peak working set of each run, as the operating system counted them
           (cell_speed_run.py's rows in results.tsv). Reported, not gated.

usage: cell_speed_bake_check.py <out_dir> <cell>
"""
import hashlib
import os
import re
import sys


def sums(run):
    out = {}
    names = ['probes.tsv', 'soup.psp', 'souprefs.tsv']
    bake = os.path.join(run, 'bake')
    if os.path.isdir(bake):
        names += ['bake/' + n for n in sorted(os.listdir(bake))]
    for n in names:
        p = os.path.join(run, n)
        if os.path.isfile(p):
            with open(p, 'rb') as f:
                b = f.read()
            if n == 'probes.tsv':   # its "# probe time ms" header line is a stopwatch, not the bake
                b = re.sub(rb'(?m)^# [^\n]*time ms[^\n]*\n', b'', b)
            out[n] = (hashlib.sha256(b).hexdigest(), len(b))
    return out


def notes(run):
    try:
        with open(os.path.join(run, 'shot.notes'), encoding='utf-8', errors='replace') as f:
            return f.read()
    except OSError:
        return ''


def main():
    out, cell = sys.argv[1], sys.argv[2]
    a, b = os.path.join(out, cell + '.all'), os.path.join(out, cell + '.lean')
    sa, sb = sums(a), sums(b)
    sectors = [n for n in sa if n.startswith('bake/') and n.lower().endswith('.tbk')]
    differ = sorted(n for n in set(sa) | set(sb) if sa.get(n) != sb.get(n))
    empty = [n for n in sa if sa[n][1] == 0]
    whole = all(n in sa for n in ('probes.tsv', 'soup.psp', 'souprefs.tsv'))
    ok = bool(sectors) and whole and not differ and not empty
    print('SAME %s  %d files (%d sector files, the probes, the soup, its reference list), %d bytes; %s'
          % ('PASS' if ok else 'FAIL', len(sa), len(sectors), sum(v[1] for v in sa.values()),
             'the same bytes in both runs' if not differ else
             '%d differ or are missing: %s' % (len(differ), ', '.join(differ[:6]))))

    na, nb = notes(a), notes(b)
    row = re.compile(r'refs left out by type(.*)')
    ra, rb = row.search(na), row.search(nb)
    left = sum(int(x) for x in re.findall(r' (\d+)', ra.group(1))) if ra else -1
    m = re.search(r'headless bake: (\d+) placements the soup leaves out were not loaded', nb)
    skipped = int(m.group(1)) if m else -1
    said_all = 'headless bake:' in na
    # The row may only GROW in the lean run, and only where the all-loaded run lost a placement to a model
    # that did not load (it is counted there before the type is asked): fewer failed models in the lean run.
    def types(r):
        return {k: int(v) for k, v in re.findall(r' (\S+) (\d+)', r.group(1))} if r else {}
    ta, tb = types(ra), types(rb)
    left_b = sum(tb.values())
    grown = {k: tb[k] - ta.get(k, 0) for k in tb if tb[k] != ta.get(k, 0)}
    shrunk = [k for k in ta if tb.get(k, 0) < ta[k]]
    fail = re.compile(r'failed to load (\d+)')
    fa, fb = fail.search(na), fail.search(nb)
    fewer_failed = bool(fa and fb) and int(fb.group(1)) < int(fa.group(1))
    row_ok = bool(ra and rb) and not shrunk and (not grown or fewer_failed)
    ok = row_ok and skipped == left_b and skipped >= left and skipped > 0 and not said_all
    print('SKIPPED %s  the lean run did not load %d placements (its own row sums to %d); the soup leaves out %d by '
          'type (%s); the row in the lean run: %s; the all-loaded run skipped none: %s'
          % ('PASS' if ok else 'FAIL', skipped, left_b, left, ra.group(1).strip() if ra else 'no row',
             ('the same' if not grown and not shrunk else
              'grew by %s where models failed to load (%s -> %s failed)' % (
                  ' '.join('%s +%d' % kv for kv in sorted(grown.items())) or 'nothing',
                  fa.group(1) if fa else '?', fb.group(1) if fb else '?') + ('' if row_ok else ', NOT ALLOWED'))
             if ra and rb else 'NO ROW', 'yes' if not said_all else 'NO'))

    rows = {}
    try:
        with open(os.path.join(out, 'results.tsv')) as f:
            for line in f:
                c = line.rstrip('\n').split('\t')
                if len(c) >= 6 and c[0].startswith(cell + '.'):
                    rows[c[0][len(cell) + 1:]] = c
    except OSError:
        pass
    if 'all' in rows and 'lean' in rows:
        s0, s1 = float(rows['all'][2]), float(rows['lean'][2])
        m0, m1 = float(rows['all'][5]), float(rows['lean'][5])
        print('TIME  all loaded %.1f s, lean %.1f s (%.0f%% less); peak %.0f MB -> %.0f MB (%.0f%% less)'
              % (s0, s1, 100 * (1 - s1 / s0) if s0 else 0, m0, m1, 100 * (1 - m1 / m0) if m0 else 0))
    else:
        print('TIME  a run is missing from results.tsv')
    return 0


if __name__ == '__main__':
    sys.exit(main())
