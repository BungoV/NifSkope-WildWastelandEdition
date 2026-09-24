"""The bake record, before vs after, with DEFAULTS2's volatile lines dropped (amend_lodb.py's list)."""
import os
import sys

VOL = ('lodb\t', 'baked\t', 'switch\t', 'switches\t', 'census\tstage times', 'census\tbake census')


def body(p):
    t = open(p, 'rb').read().decode().replace('\r', '')
    return [ln for ln in t.split('\n') if ln and not ln.startswith(VOL)]


a, b = sys.argv[1], sys.argv[2]
for v in sys.argv[3:]:
    x = body(os.path.join(a, v, 'obj', 'Commonwealth.lodb'))
    y = body(os.path.join(b, v, 'obj', 'Commonwealth.lodb'))
    d = [p for p, q in zip(x, y) if p != q]
    print('LODB %s: %d vs %d stable lines, %d differ %s' % (v, len(x), len(y), len(d), [p[:60] for p in d[:3]]))
