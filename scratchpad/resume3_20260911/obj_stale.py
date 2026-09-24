"""RESUME3 gate R5: "a successful build is not a consistent one".

For every object in Makefile.Release, read its DEPENDENCY list (walking the
continuation lines, not `grep -A3`), intersect it with the files this lane
changed, and assert the object is newer than every changed file it depends on.
Prints the changed files nothing depends on as well, so a header that reached
nothing is visible rather than assumed.

    python obj_stale.py
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.chdir(ROOT)

changed = []
out = subprocess.run(['git', 'status', '--porcelain', '--', 'src', 'res', 'lib', 'NifSkope.pro'],
                     capture_output=True, text=True).stdout
for line in out.splitlines():
    f = line.split()[-1]
    if os.path.isfile(f):
        changed.append(f.replace('\\', '/'))
print('changed files under src/res/lib: %d' % len(changed))

# parse the object blocks
blocks = {}
cur = None
for raw in open('Makefile.Release', 'r', encoding='utf-8', errors='replace'):
    line = raw.rstrip('\n')
    if line.startswith('GeneratedFiles/.obj/') and ':' in line:
        cur = line.split(':', 1)[0].strip()
        blocks[cur] = [line.split(':', 1)[1]]
    elif cur is not None:
        if line.startswith('\t'):
            cur = None
        else:
            blocks[cur].append(line)
            if not line.endswith('\\'):
                cur = None
print('object blocks parsed: %d' % len(blocks))

deps = {}
for obj, lines in blocks.items():
    toks = ' '.join(lines).replace('\\', ' ').split()
    deps[obj] = set(t.replace('\\', '/') for t in toks)

exe = 'release/NifSkope.exe'
mexe = os.path.getmtime(exe)
stale = 0
reached = set()
for obj, d in sorted(deps.items()):
    if not os.path.exists(obj):
        print('  MISSING object %s' % obj)
        stale += 1
        continue
    mo = os.path.getmtime(obj)
    for c in changed:
        if c in d:
            reached.add(c)
            if mo < os.path.getmtime(c):
                print('  STALE %-46s older than %s' % (obj, c))
                stale += 1
    if mexe < mo:
        print('  EXE OLDER than %s' % obj)
        stale += 1

print()
print('objects stale against a changed dependency: %d' % stale)
print('changed files that reach at least one object: %d' % len(reached))
nr = [c for c in changed if c not in reached]
print('changed files that reach NO object (%d):' % len(nr))
for c in nr:
    print('   %s' % c)
print()
print('RESULT ' + ('PASS' if stale == 0 else 'RED'))
sys.exit(0 if stale == 0 else 1)
