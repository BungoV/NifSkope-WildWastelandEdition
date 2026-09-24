"""AMENDMENT 14:3x (after the first run, said so in the report): the .lodb is the bake RECORD --
its first lines carry the exe size, the UTC time, the output dirs and the TYPED switches, and its
census lines carry wall times and peak memory. It can never be byte-identical across two runs.
Compare it with those volatile lines dropped; every other line (plugins, chunk input digests,
out rows with each file's sha1) must match."""
import sys, os
L = sys.argv[1]
VOL = ('lodb\t', 'baked\t', 'switch\t', 'switches\t', 'census\tstage times', 'census\tbake census')
def body(v):
    t = open(os.path.join(L, v, 'obj', 'Commonwealth.lodb'), 'rb').read().decode().replace('\r', '')
    return [ln for ln in t.split('\n') if ln and not ln.startswith(VOL)]
fails = 0
for a, b, want in (('N0', 'R1', True), ('N1', 'R0', True), ('N0', 'R0', False)):
    x, y = body(a), body(b)
    d = [(p, q) for p, q in zip(x, y) if p != q]
    ok = (len(x) == len(y) and not d) == want and len(x) > 10
    print('LODB %s vs %s: %d stable lines, %d differ%s -> %s' % (a, b, len(x), len(d),
          (' e.g. ' + ' | '.join(p.split('\t')[0] + ' ' + p.split('\t')[-2][-40:] for p, _ in d[:4])) if d else '',
          'PASS' if ok else 'FAIL'))
    fails += not ok
print('RESULT %s' % ('PASS' if not fails else 'FAIL'))
