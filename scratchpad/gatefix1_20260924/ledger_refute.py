"""GATEFIX1: prove each new/reworked ledger comparison FAILS on a broken record and
passes on a volatile-only change. Mutated copies go to a temp folder; the real
records in bt1/ are only read."""
import os, shutil, subprocess, sys, tempfile

L = 'E:/Projects/NifskopeWWE-gatefix1/tests/spells/lodgen_btofree_ledger.py'
B = 'E:/Projects/NifskopeWWE-gatefix1/scratchpad/gatefix1_20260924/bt1/'
R = 'FO4CSLOD/Commonwealth/Commonwealth.lodb'
T = tempfile.mkdtemp(prefix='gf1ref_')


def mutated(src, old, new, count=1):
    with open(src, 'rb') as fh:
        data = fh.read().decode('utf-8')
    assert data.count(old) >= 1, (src, old)
    data = data.replace(old, new, count)
    # the record's OWN folder is copied beside it, so the drop mode's on-disk
    # sizes of the rows in the record folder read what the real record has
    n = len(os.listdir(T))
    home = os.path.join(T, str(n), 'FO4CSLOD', 'Commonwealth')
    shutil.copytree(os.path.dirname(src), home)
    dst = os.path.join(home, 'Commonwealth.lodb')
    with open(dst, 'wb') as fh:
        fh.write(data.encode('utf-8'))
    return dst


def rc(mode, a, b, ra, rb):
    p = subprocess.run([sys.executable, L, mode, a, b, ra, rb], capture_output=True, text=True)
    return p.returncode


def rec_path(side):
    return B + side + '/' + R


cases = []
# drop mode: a census content byte, an out digest, and the end count
d = rec_path('drop')
cases.append(('drop: census content (threads) changed -> FAIL', 'drop', rec_path('rung_native'),
              mutated(d, 'threads 16', 'threads 15'), 1))
cases.append(('drop: one out digest changed -> FAIL', 'drop', rec_path('rung_native'),
              mutated(d, '1d77bf31d3b3bc03b9184745f6b3214964814e00', '1d77bf31d3b3bc03b9184745f6b3214964814e01', 1), 1))
cases.append(('drop: end count off by one -> FAIL', 'drop', rec_path('rung_native'),
              mutated(d, 'end\t7\t', 'end\t8\t'), 1))
cases.append(('drop: layout count off by one -> FAIL', 'drop', rec_path('rung_native'),
              mutated(d, ', 10 file(s)', ', 11 file(s)'), 1))
cases.append(('drop: stage-times value changed (volatile) -> PASS', 'drop', rec_path('rung_native'),
              mutated(d, 'stage times: landscape 0.0 s', 'stage times: landscape 9.9 s'), 0))
# same mode (leg b): content vs volatile
k = rec_path('keep')
cases.append(('same: census content (chunk jobs) changed -> FAIL', 'same', rec_path('rung_native'),
              mutated(k, 'chunk jobs 1', 'chunk jobs 2'), 1))
cases.append(('same: peak working set changed (volatile) -> PASS', 'same', rec_path('rung_native'),
              mutated(k, 'peak working set: 1.', 'peak working set: 7.'), 0))
cases.append(('same: a census row dropped -> FAIL', 'same', rec_path('rung_native'),
              mutated(k, '\ncensus\tstage times:', '\nxcensus\tstage times:'), 1))

fails = 0
for what, mode, a, b, want in cases:
    got = rc(mode, a, b, B + ('rung_native' if 'rung_native' in a else 'rung_stock'),
             B + ('drop' if mode == 'drop' else 'keep'))
    ok = (got != 0) == (want != 0)
    fails += 0 if ok else 1
    print('  %s %s (rc %d)' % ('ok  ' if ok else 'FAIL', what, got))
shutil.rmtree(T, ignore_errors=True)
print('%d refuter cases, %d failures' % (len(cases), fails))
sys.exit(1 if fails else 0)
