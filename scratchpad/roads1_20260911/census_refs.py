import collections
import json
import sys

SEP = chr(92)

d = json.load(open(sys.argv[1]))
b = d['bases']
fold = collections.Counter()
sigs = collections.Counter()
rows = collections.Counter()
missing = 0
for r in d['refs']:
    k = '%08X' % r['base']
    rec = b.get(k)
    if not rec:
        missing += 1
        continue
    p = rec['modl'].replace(SEP, '/').lower()
    parts = p.split('/')
    fold[(rec['sig'], '/'.join(parts[:2]))] += 1
    sigs[rec['sig']] += 1
    rows[(rec['sig'], p)] += 1
print('missing base records:', missing)
print('--- signatures ---')
for k, v in sigs.most_common():
    print('%6d %s' % (v, k))
print('--- top folders ---')
for k, v in fold.most_common(30):
    print('%6d %-6s %s' % (v, k[0], k[1]))
print('--- models whose path mentions road/street/sidewalk/curb/asphalt/pavement/decal ---')
for (s, p), v in rows.most_common():
    if any(w in p for w in ('road', 'street', 'sidewalk', 'curb', 'asphalt',
                            'pavement', 'decal', 'dirt', 'path')):
        print('%6d %-6s %s' % (v, s, p))
