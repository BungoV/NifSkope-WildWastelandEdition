"""Timeline of an epoch-prefixed lodgen bake log (bake_region.sh / printf %(%s)T prefix).
Usage: python timeline.py <bake.log> [gap_seconds]
Prints: phases (runs of one line class), the big gaps (silent stretches, i.e. where time went
without log output), and per-class first/last/count."""
import re, sys, collections

path = sys.argv[1]
GAP = int(sys.argv[2]) if len(sys.argv) > 2 else 20
rows = []
for raw in open(path, 'rb'):
    s = raw.decode('utf-8', 'replace').rstrip('\r\n')
    m = re.match(r'^(\d{10}) (.*)$', s)
    if not m:
        continue
    t = int(m.group(1)); body = m.group(2)
    cls = re.sub(r'[0-9a-fA-F]{5,}', 'H', body)
    cls = re.sub(r'-?\d+(\.\d+)?', 'N', cls)[:48]
    rows.append((t, cls, body))
if not rows:
    print('no rows'); sys.exit(1)
t0 = rows[0][0]
print('span %d s, %d lines' % (rows[-1][0] - t0, len(rows)))
print('\n== silent gaps >= %d s (time went to the stage BEFORE the next line) ==' % GAP)
for i in range(1, len(rows)):
    d = rows[i][0] - rows[i - 1][0]
    if d >= GAP:
        print('%6d +%5d s  before: %s\n               after : %s' % (rows[i - 1][0] - t0, d, rows[i - 1][2][:150], rows[i][2][:150]))
print('\n== line classes: first..last (count) ==')
first = collections.OrderedDict(); last = {}; cnt = collections.Counter()
for t, c, b in rows:
    if c not in first:
        first[c] = t
    last[c] = t; cnt[c] += 1
for c, f in first.items():
    if cnt[c] >= 3:
        print('%6d..%6d  %5d  %s' % (f - t0, last[c] - t0, cnt[c], c))
