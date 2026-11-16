"""Lane VKFAR17 (2026-11-16): the probe relight's app site must leave GL silent where Vulkan drew.

python -I tests/vkframe/reflrelightsite.py <tree>
Reads src/features/reflections/cellreflprobe.cpp. Checks:
  1 the Vulkan call exists: `vkDrawn = WwVkPresent::reflRelightRun(` (count == 1);
  2 every GL relight draw (`prog->uni1f( "rpLayer"` followed by a `->drawShape(` in the same block) sits inside the
    nearest enclosing `if ( !vkDrawn )` (count of guarded draws == count of draws, draws > 0);
  3 the GL red key WW_VKVIEW_REFLRELIGHT_RED gates the route (`shareWanted( "WW_VKVIEW_REFLRELIGHT_RED" )`, count == 1).
Red (in memory, never on disk): `!vkDrawn` replaced by `true` must FAIL check 2 (GL left writing over Vulkan's tiles).
Last line `REFLRELIGHTSITE OK` or `REFLRELIGHTSITE FAILED`.
"""
import os
import re
import sys


def guarded_draws(text):
    """(draws, guarded): GL relight draws and how many sit under an enclosing `if ( !vkDrawn )`."""
    lines = text.splitlines()
    draws = guarded = 0
    for i, ln in enumerate(lines):
        if 'prog->uni1f( "rpLayer"' not in ln:
            continue
        # the draw must follow within the same block (before the closing brace at this depth)
        depth = 0
        drew = False
        for ln2 in lines[i + 1:i + 12]:
            depth += ln2.count('{') - ln2.count('}')
            if '->drawShape(' in ln2:
                drew = True
                break
            if depth < 0:
                break
        if not drew:
            continue
        draws += 1
        # walk back to the brace that opens this line's block and read its line
        depth = 0
        for j in range(i - 1, -1, -1):
            depth += lines[j].count('}') - lines[j].count('{')
            if depth < 0:
                if re.search(r'\bif\s*\(\s*!\s*vkDrawn\s*\)', lines[j]):
                    guarded += 1
                break
    return draws, guarded


def check(text, label):
    runs = text.count('vkDrawn = WwVkPresent::reflRelightRun(')
    reds = text.count('shareWanted( "WW_VKVIEW_REFLRELIGHT_RED" )')
    draws, guarded = guarded_draws(text)
    ok = runs == 1 and reds == 1 and draws > 0 and guarded == draws
    print('REFLRELIGHTSITE %s: vulkan calls %d, red key gates %d, GL relight draws %d, under !vkDrawn %d: %s'
          % (label, runs, reds, draws, guarded, 'OK' if ok else 'FAIL'))
    return ok


def main():
    tree = sys.argv[1]
    path = os.path.join(tree, 'src', 'features', 'reflections', 'cellreflprobe.cpp')
    with open(path, 'r', encoding='utf-8', errors='replace', newline='') as fh:
        text = fh.read()
    base = check(text, 'base')
    red = check(text.replace('!vkDrawn', 'true'), 'red gl-left-writing')
    print('REFLRELIGHTSITE red gl-left-writing: %s' % ('FAIL (fired)' if not red else 'PASS (dead)'))
    ok = base and not red
    print('REFLRELIGHTSITE %s' % ('OK' if ok else 'FAILED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
