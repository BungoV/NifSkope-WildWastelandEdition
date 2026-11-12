"""R9 red: optlevels.txt must apply when the tree is given the way tools/ww_spirv.sh gives it (relative, cwd = tree root).
Run from the tree root: python -I -X utf8 tests/hlsllive/red_levels.py   -> prints `LEVELS OK n` or `LEVELS FAIL ...` (exit 1).
It checks opt_of() for every table entry through a relative path, an absolute path and a backslash path."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stamps

TAB = stamps.load_table(os.path.join('res', 'shaders', 'hlsl', 'optlevels.txt'), 2)
if not TAB:
    print('LEVELS FAIL no optlevels.txt entries read (cwd must be the tree root)')
    sys.exit(1)
bad = []
n = 0
for name, v in sorted(TAB.items()):
    found = [os.path.join(dp, name) for dp, _, fs in os.walk(os.path.join('res', 'shaders', 'hlsl')) for f in fs if f == name]
    if not found:
        bad.append(name + ' (no file)')
        continue
    for form in (found[0], os.path.abspath(found[0]), found[0].replace('/', '\\')):
        n += 1
        if stamps.opt_of(form) != v[0]:
            bad.append('%s via %s gives %s, table %s' % (name, form, stamps.opt_of(form), v[0]))
if bad:
    print('LEVELS FAIL ' + '; '.join(bad[:6]))
    sys.exit(1)
print('LEVELS OK %d checks over %d entries' % (n, len(TAB)))
