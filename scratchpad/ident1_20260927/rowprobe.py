"""IDENT1: list the ResNCA row-house pieces near the audit point, so the three houses can be named by ref."""
import sys, os, collections
import groups

h, P, C = groups.load(sys.argv[1])
cx, cy, r = -5975.0, -15112.0, float(sys.argv[2]) if len(sys.argv) > 2 else 3000.0
q = [p for p in P if 'resnc' in p['name'].lower() and abs(p['x'] - cx) < r and abs(p['y'] - cy) < r]
print('pieces', len(q))
by = collections.defaultdict(list)
for p in q:
    by[(p['ref'], p['base'])].append(p)
for (ref, base), v in sorted(by.items(), key=lambda kv: (kv[1][0]['x'], kv[1][0]['y'])):
    xs = [p['lo'][0] for p in v] + [p['hi'][0] for p in v]
    ys = [p['lo'][1] for p in v] + [p['hi'][1] for p in v]
    nm = os.path.basename(v[0]['name'].replace(chr(92), '/').split('(')[-1].rstrip(')'))
    print('ref %08x base %08x parts %3d  x %7.0f..%7.0f  y %7.0f..%7.0f  %s' % (ref, base, len(v), min(xs), max(xs), min(ys), max(ys), nm[:48]))
