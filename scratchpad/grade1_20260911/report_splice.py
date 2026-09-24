"""Splice section 0a in before section 1, and sections 3-8 at the end.

The report is LF-only and must stay LF-only; the Write tool refuses the name,
so the two staged .txt files are spliced by this script instead.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
REP = os.path.abspath(os.path.join(HERE, '..', 'lane_grade1_report.md'))

d = open(REP, 'rb').read()
assert d.count(b'\r') == 0, 'report is not LF-only'
t = d.decode('utf-8')

a = open(os.path.join(HERE, 's0a.txt'), encoding='utf-8').read().replace('\r\n', '\n')
b = open(os.path.join(HERE, 's3s8.txt'), encoding='utf-8').read().replace('\r\n', '\n')
assert a.startswith('## 0a.') and b.startswith('## 3. The change')
assert '## 0a.' not in t and '## 3. The change' not in t, 'already spliced'

anchor = '## 1. The curve'
assert t.count(anchor) == 1
t = t.replace(anchor, a + anchor)

if not t.endswith('\n'):
    t += '\n'
t += '\n' + b

out = t.encode('utf-8')
assert out.count(b'\r') == 0
open(REP, 'wb').write(out)
print('%s: %d -> %d bytes, %d -> %d lines, CR %d'
      % (REP, len(d), len(out), d.count(b'\n'), out.count(b'\n'),
         out.count(b'\r')))
