"""MERGE2: splice TERRLIVE1's lane text (+ MERGE2's own) into HANDOFF.md, WW_CHANGES.md and MISTAKES.md.
usage: python splice.py <repo> <when>   (when = the `date`-read time for the new top block)
Needs beside it: splice_handoff_top.md, splice_wwchanges.md, splice_mistakes.md, splice_gates.md, splice_exe.md,
splice_mistakes_merge2.md, splice_mainhead.txt.
Bytes outside the inserted text are never rewritten (WW_CHANGES.md has mixed line endings); inserted text takes the
line ending of the line it is inserted before. IDENT2's WW_CHANGES / MISTAKES text is already in the branch."""
import sys, os
R, WHEN = sys.argv[1], sys.argv[2]
S = os.path.dirname(os.path.abspath(__file__))


def draft(n):
    return open(os.path.join(S, n), encoding='utf-8').read().replace('\r\n', '\n').rstrip('\n') + '\n'


def insert_before(b, at, text):
    eol = b.index(b'\n', at)
    nl = b'\r\n' if b[eol - 1:eol] == b'\r' else b'\n'
    return b[:at] + text.encode('utf-8').replace(b'\n', nl) + b[at:]


def counts(b):
    c = b.count(b'\r\n')
    return c, b.count(b'\n') - c


# --- WW_CHANGES: TERRLIVE1 entry above the first "## " entry; IDENT2's heading says merged
p = os.path.join(R, 'WW_CHANGES.md')
b = open(p, 'rb').read()
assert b'merged by MERGE2' not in b
old = b'(lane IDENT2, 2026-09-29, branch ident2-20260929, not merged)'
new = b'(lane IDENT2, 2026-09-29, branch ident2-20260929; merged by MERGE2, 2026-09-29)'
assert b.count(old) == 1
c0 = counts(b)
b1 = b.replace(old, new, 1)
at = b1.index(b'\n## ') + 1
nb = insert_before(b1, at, draft('splice_wwchanges.md') + '\n')
assert nb[:at] == b1[:at] and nb.endswith(b1[at:])
c1 = counts(nb)
assert c1[0] >= c0[0] and c1[1] >= c0[1], (c0, c1)
open(p, 'wb').write(nb)
print('WW_CHANGES.md +%d bytes; CRLF %d -> %d, LF %d -> %d' % (len(nb) - len(b), c0[0], c1[0], c0[1], c1[1]))

# --- MISTAKES: new section above the first "## "
p = os.path.join(R, 'MISTAKES.md')
b = open(p, 'rb').read()
assert b'spliced by MERGE2' not in b
c0 = counts(b)
m = draft('splice_mistakes.md').replace('@@MERGE2_MISTAKES@@\n', draft('splice_mistakes_merge2.md'))
assert '@@' not in m
at = b.index(b'\n## ') + 1
nb = insert_before(b, at, m + '\n')
assert nb[:at] == b[:at] and nb.endswith(b[at:])
c1 = counts(nb)
open(p, 'wb').write(nb)
print('MISTAKES.md +%d bytes; CRLF %d -> %d, LF %d -> %d' % (len(nb) - len(b), c0[0], c1[0], c0[1], c1[1]))

# --- HANDOFF (all LF): new top block; MERGE1's block becomes history; its rulings are carried
p = os.path.join(R, 'HANDOFF.md')
b = open(p, 'rb').read()
assert b'\r\n' not in b
t = b.decode('utf-8')
old_h = '## TOP BLOCK -- written 2026-09-29 03:49'
assert t.count(old_h) == 1
a = t.index(old_h)
r0 = t.index("### bungo's rulings during MERGE1", a)
r0 = t.index('\n', r0) + 1
r1 = t.index('### What landed', r0)
rulings = t[r0:r1].rstrip('\n') + '\n'
t = t.replace(old_h, '## HISTORY (was the top block) -- written 2026-09-29 03:49', 1)
top = draft('splice_handoff_top.md')
for k, v in (('@@WHEN@@', WHEN), ('@@MAINHEAD@@', draft('splice_mainhead.txt').strip()),
             ('@@GATES@@\n', draft('splice_gates.md')), ('@@EXE@@\n', draft('splice_exe.md')),
             ('@@RULINGS@@\n', rulings)):
    assert top.count(k) == 1, k
    top = top.replace(k, v)
assert '@@' not in top, [x for x in top.split('\n') if '@@' in x]
a = t.index('## HISTORY (was the top block) -- written 2026-09-29 03:49')
t = t[:a] + top + '\n' + t[a:]
nb = t.encode('utf-8')
assert nb.endswith(b[a:][-100000:])
open(p, 'wb').write(nb)
print('HANDOFF.md +%d bytes' % (len(nb) - len(b)))
