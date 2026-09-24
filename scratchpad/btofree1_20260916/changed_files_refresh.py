# Lane BTOFREE1, 2026-09-16 -- CHANGED_FILES.txt is re-measured rather than
# hand-edited: every "after" number below is read off disk now, so the table
# cannot drift from the tree while the lane is still patching.
import io
import os

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
OUT = ROOT + 'scratchpad/btofree1_20260916/CHANGED_FILES.txt'

# path, kind, bytes BEFORE, CR before, LF before  (the befores are the measurements
# this lane took before its first patch; a '-' means the file is new)
FILES = [
    ('--- source ---', None, None, None, None),
    ('src/lodgenchunkpass.h', 'M', 5990, 0, 142),
    ('src/lodgenchunkpass.cpp', 'M', 14058, 0, 409),
    ('src/nifcli.cpp', 'M', 357893, 0, 7683),
    ('src/lodgenmanager.cpp', 'M', 169509, 0, 3585),
    ('src/nifskope_ui.cpp', 'M', 1591829, 0, 33663),
    ('--- gates ---', None, None, None, None),
    ('tests/spells/lodgen_btofree.sh', 'A', None, None, None),
    ('tests/spells/lodgen_btofree_ledger.py', 'A', None, None, None),
    ('tests/spells/native_open.sh', 'M', 17125, 0, 348),
    ('tests/spells/native_open_authority.py', 'M', 10997, 0, 311),
    ('tests/spells/lodgen_native.sh', 'M', 15024, 0, 278),
    ('tests/spells/lodgen_byte_gate.sh', 'M', 8362, 0, 190),
    ('--- documents ---', None, None, None, None),
    ('docs/FO4CS_IMPROVED_LOD_PLAN.md', 'M', 81688, 0, 1322),
    ('.claude/skills/nifskope-ww-lodgen/SKILL.md', 'M', 38639, 0, 517),
    ('MISTAKES.md', 'M', 455055, 0, 7680),
]
OUTSIDE = [('E:/Projects/Claude/.claude/skills/nifskope-ww-lodgen/SKILL.md', 'M', 38639, 0, 517)]

TAIL = '''
--- NOT TOUCHED, and why --------------------------------------------------------------------------
- WW_CHANGES.md is MIXED (19020 CR / 29707 LF) and belongs to the director, who splices
  scratchpad/btofree1_20260916/WW_CHANGES_ENTRY.md. The entries at the head of that file are
  LF-only, so the entry file is LF-only and matches its neighbours; a splice near an older CRLF
  entry would need the CR back.
- tests/spells/lodgen_defaults.sh and tests/spells/lodgen_ladder.sh were READ and need nothing.
  defaults phases (b) and (c) bake WITHOUT --native, so their .BTO files are still on disk; phase
  (e) compares only files under the native directory; ladder's only chunk reference is a
  .BTO.manifest.txt, which is kept either way.
- git state: nothing committed, nothing stashed, no branch touched, no index entry.

--- FIXTURE RE-BAKED (data, not source; the old pair is preserved) ----------------------------------
M scratchpad/showcase1_20260912/out/look/native/Commonwealth.lodo
                                                         9657316 ->   225399755   (LODO v3, refused by name -> v4)
M scratchpad/showcase1_20260912/out/look/native/Commonwealth.lodi
                                                          128256 ->      134598   (v3 -> v5)
M scratchpad/showcase1_20260912/out/look/mesh_report.txt  435204 ->      911262
  the pre-re-bake pair is kept at scratchpad/btofree1_20260916/fixture_backup/

--- BUILT -------------------------------------------------------------------------------------------
  release/NifSkope.exe      22341632 B, 2026-09-16 15:53:30  ->  22356992 B, 2026-09-16 17:11:17
  release/NifSkope.before_btofree1.exe  is the rung copy of the 15:53:30 exe
  release/NifSkope_inuse_2000.exe  is the 15:53:30 exe, renamed aside because a NifSkope window
    (pid 2000, launched 16:53:38 with no arguments -- not this lane's) was holding it. It was NOT
    killed. That window had closed by 18:34, so the file is deletable, but deleting an exe is not
    this lane's call and it is left where it is.
'''


def counts(rel):
    p = rel if rel.startswith('E:') else ROOT + rel
    b = open(p, 'rb').read()
    return len(b), b.count(b'\r'), b.count(b'\n')


w = io.StringIO()
w.write('CHANGED_FILES -- lane BTOFREE1, 2026-09-16\n')
w.write("Every count below is a PYTHON BYTE COUNT (b.count(b'\\r') / b.count(b'\\n')), not grep, and\n")
w.write('every "after" was re-measured off disk when this file was written.\n')
w.write('Every file in this tree was LF-only before this lane and is LF-only after it; no file\'s CR\n')
w.write('count moved, and every patch script asserted that before it wrote.  A = added, M = modified.\n\n')
w.write('%-52s %12s    %12s   %10s   %12s\n' % ('', 'bytes before', 'bytes after', 'CR b -> a', 'LF b -> a'))

rows = [(p, k, n, c, l, False) for p, k, n, c, l in FILES] + \
       [('--- outside the repo (the same skill, the session\'s own copy) ---', None, None, None, None, False)] + \
       [(p, k, n, c, l, True) for p, k, n, c, l in OUTSIDE]

for p, k, n0, cr0, lf0, out in rows:
    if k is None:
        w.write(('%s' % p).ljust(104, '-') + '\n')
        continue
    n1, cr1, lf1 = counts(p)
    before = '-' if n0 is None else '{:,}'.format(n0)
    crb = '-' if cr0 is None else str(cr0)
    lfb = '-' if lf0 is None else '{:,}'.format(lf0)
    name = ('%s %s' % (k, p))
    if len(name) > 52:
        w.write('%s\n' % name)
        name = ''
    w.write('%-52s %12s -> %12s   %4s -> %3d   %12s -> %s\n'
            % (name, before, '{:,}'.format(n1), crb, cr1, lfb, '{:,}'.format(lf1)))

w.write(TAIL)
data = w.getvalue().encode('utf-8')
assert b'\r' not in data
open(OUT, 'wb').write(data)
print(w.getvalue())
print('CHANGED_FILES.txt rewritten, %d bytes, LF %d, CR %d' % (len(data), data.count(b'\n'), data.count(b'\r')))
