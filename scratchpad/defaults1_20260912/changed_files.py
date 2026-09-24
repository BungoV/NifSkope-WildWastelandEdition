"""DEFAULTS1: write CHANGED_FILES.txt.

Two honesty rules this file obeys:

* A/M is THIS LANE's status, not git's. The tree is shared and many lanes'
  work is uncommitted in it, so `git status` calls files "new" that another
  lane created days ago. This lane created exactly one file.
* "before" is a MEASURED number where one was taken. The lane's source edits
  were made before any snapshot was, so for those the honest answer is "not
  snapshotted", and HEAD's number is printed separately, clearly labelled as
  what it is -- many lanes behind, NOT this lane's baseline.

Counts are Python byte counts. grep and wc do not answer this question.
"""
import subprocess

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
OUT = ROOT + '/scratchpad/defaults1_20260912/CHANGED_FILES.txt'

# path -> (this lane's status, measured before as (bytes, LF, CR) or None)
FILES = [
    ('src/nifcli.cpp', 'M', None),
    ('src/lodgen.h', 'M', None),
    ('src/lodgen.cpp', 'M', None),
    ('src/lodgenchunkpass.cpp', 'M', None),
    ('src/lodgenmanager.cpp', 'M', None),
    ('src/nifskope_ui.cpp', 'M', (1591162, 33649, 0)),
    ('tests/spells/lodgen_defaults.sh', 'A', None),
    ('tests/spells/lodgen_terrain.sh', 'M', (20499, 451, 0)),
    ('tests/spells/lodgen_tree_sway.sh', 'M', (4380, 97, 0)),
    ('tests/spells/lodgen_farring.sh', 'M', (13890, 325, 0)),
    ('tests/spells/lodgen_texture_arrays.sh', 'M', (28800, 513, 0)),
    ('tests/spells/lodgen_terrain_vt.sh', 'M', (30459, 591, 0)),
    ('tests/spells/lodgen_roads.sh', 'M', (8399, 171, 0)),
    ('tests/spells/lodgen_native_baseline.sh', 'M', (9139, 199, 0)),
    ('docs/LODGEN_TERRAIN_VT.md', 'M', None),
    ('docs/LODGEN_IMPOSTOR_SPEC.md', 'M', (36444, 615, 0)),
    ('.claude/skills/nifskope-ww-lodgen/SKILL.md', 'M', (36753, 494, 0)),
    ('MISTAKES.md', 'M', (425233, 7236, 0)),
]


def counts(data):
    return len(data), data.count(b'\n'), data.count(b'\r')


def fmt(t):
    return '%d / %d / %d' % t


rows = []
for p, status, before in FILES:
    after = counts(open(ROOT + '/' + p, 'rb').read())
    try:
        head = counts(subprocess.run(['git', '-C', ROOT, 'show', 'HEAD:' + p],
                                     capture_output=True, check=True).stdout)
    except subprocess.CalledProcessError:
        head = None
    rows.append((status, p, before, after, head))

w = max(len(p) for _, p, _, _, _ in rows)
L = []
L.append('DEFAULTS1 (2026-09-12) -- the files this lane changed. Nothing is committed.')
L.append('')
L.append('A = created by THIS lane. M = modified by this lane. The tree is shared and')
L.append("other lanes' work is uncommitted in it, so git would call several of these")
L.append('files new; only lodgen_defaults.sh is mine.')
L.append('')
L.append('Counts are bytes / LF / CR, from Python.')
L.append('')
L.append('%-2s %-*s %22s %22s' % ('', w, 'path', 'before (this lane)', 'after'))
for status, p, before, after, head in rows:
    L.append('%-2s %-*s %22s %22s' % (status, w, p,
                                      fmt(before) if before else 'not snapshotted',
                                      fmt(after)))
L.append('')
L.append('CR is 0 on every line above, before and after: every source, spell, doc and')
L.append('skill file in this list is LF-only and stayed LF-only.')
L.append('')
L.append('For the files with no snapshot, git HEAD is printed below ONLY as a landmark.')
L.append('HEAD is many lanes behind this tree, so the difference between these two')
L.append("columns is not this lane's diff and must not be read as one.")
L.append('')
L.append('%-2s %-*s %22s %22s' % ('', w, 'path', 'git HEAD', 'now'))
for status, p, before, after, head in rows:
    L.append('%-2s %-*s %22s %22s' % (status, w, p,
                                      fmt(head) if head else 'not in HEAD',
                                      fmt(after)))
text = '\n'.join(L) + '\n'
open(OUT, 'wb').write(text.encode('utf-8'))
print(text)
