"""MERGE1: splice the lane texts into main's HANDOFF.md, WW_CHANGES.md and MISTAKES.md.
usage: python splice.py <main repo> <when>   (when = the `date`-read time for the new top block)
Bytes outside the inserted text are never rewritten (WW_CHANGES.md has mixed line endings); the inserted text takes
the line ending of the line it is inserted before."""
import sys, os
R, WHEN = sys.argv[1], sys.argv[2]
S = os.path.dirname(os.path.abspath(__file__))
BS = chr(92)


def draft(n):
    return open(os.path.join(S, n), encoding='utf-8').read().replace('\r\n', '\n').rstrip('\n') + '\n'


def insert_before(b, at, text):
    """insert `text` (LF lines) into bytes `b` at byte offset `at`, in the ending of the line starting at `at`."""
    eol = b.index(b'\n', at)
    nl = b'\r\n' if b[eol - 1:eol] == b'\r' else b'\n'
    return b[:at] + text.encode('utf-8').replace(b'\n', nl) + b[at:]


# --- WW_CHANGES and MISTAKES: new entry above the first "## " entry (newest on top)
for name, dr, mark in (('WW_CHANGES.md', 'splice_wwchanges.md', b'merged by MERGE1'),
                       ('MISTAKES.md', 'splice_mistakes.md', b'spliced by MERGE1')):
    p = os.path.join(R, name)
    b = open(p, 'rb').read()
    assert mark not in b, name
    at = b.index(b'\n## ') + 1
    nb = insert_before(b, at, draft(dr) + '\n')
    assert nb[:at] == b[:at] and nb.endswith(b[at:])
    open(p, 'wb').write(nb)
    print(name, 'inserted', len(nb) - len(b), 'bytes; rest unchanged')

# --- HANDOFF (all LF): new top block + lane lines; the 09-27 14:28 block becomes history; the two uncommitted
#     09-27 lines (19:43, 20:02) move from the file's end into that block as "Later the same day".
p = os.path.join(R, 'HANDOFF.md')
b = open(p, 'rb').read()
assert b'\r\n' not in b
t = b.decode('utf-8')
old_h = '## TOP BLOCK -- written 2026-09-27 14:28'
assert t.count(old_h) == 1
lines = [ln for ln in t.split('\n') if ln.startswith('2026-09-27 19:43 ') or ln.startswith('2026-09-27 20:02 ')]
assert len(lines) == 2, lines
for ln in lines:
    assert t.count('\n' + ln) == 1
    t = t.replace('\n' + ln, '', 1)
a = t.index(old_h)
e = t.index('\n## TOP BLOCK', a + 10) + 1
t = t[:e] + '### Later the same day (09-27)\n' + '\n'.join('- ' + ln for ln in lines) + '\n\n' + t[e:]
new_h = '## HISTORY (was the top block) -- written 2026-09-27 14:28'
t = t.replace(old_h, new_h, 1)
top = draft('splice_handoff_top.md').replace('@@WHEN@@', WHEN)
assert '@@' not in top, [x for x in top.split('\n') if '@@' in x]
a = t.index(new_h)
t = t[:a] + top + '\n' + draft('splice_handoff_lanes.md') + '\n' + t[a:]
open(p, 'wb').write(t.encode('utf-8'))
print('HANDOFF written')
