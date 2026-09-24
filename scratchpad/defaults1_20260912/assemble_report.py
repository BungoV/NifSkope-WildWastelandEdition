"""DEFAULTS1: put the report's sections in the brief's order (0..7).

The sections were written as they were measured, so they are on disk out of
order. This splits the file on its own `## ` headings, keeps the preamble, and
reassembles 0,1,2,3,4,5,6,7 -- pulling 1, 5, 6, 7 out of sec/ if they are not
already in the file. LF only; CR asserted 0.
"""
import os
import re

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/defaults1_20260912'
REP = ROOT + '/lane_defaults1_report.md'
SEC = ROOT + '/sec'

text = open(REP, 'rb').read().decode('utf-8')
parts = re.split(r'(?m)^(?=## \d\. )', text)
pre, blocks = parts[0], parts[1:]
have = {}
for b in blocks:
    n = int(re.match(r'## (\d)\.', b).group(1))
    have[n] = b.rstrip('\n')

for n in (1, 5, 6, 7):
    p = '%s/s%d.md' % (SEC, n)
    if os.path.exists(p):
        have[n] = open(p, 'rb').read().decode('utf-8').rstrip('\n')

missing = [n for n in range(8) if n not in have]
assert not missing, 'missing sections: %s' % missing

out = pre.rstrip('\n') + '\n\n' + '\n\n---\n\n'.join(have[n] for n in range(8)) + '\n'
data = out.encode('utf-8')
assert data.count(b'\r') == 0
open(REP, 'wb').write(data)
print('%d bytes  LF %d  CR %d  sections %s' % (len(data), data.count(b'\n'), data.count(b'\r'),
                                               ' '.join(str(n) for n in range(8))))
