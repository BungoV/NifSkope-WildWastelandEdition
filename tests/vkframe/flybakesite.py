"""Lane VKCLOUD6 (2026-11-16): the fly-through box bake's app site (cloudfly.cpp runPass) must leave GL silent where
Vulkan drew, and only the red key or a refusal sends a slice to GL.

python -I tests/vkframe/flybakesite.py <tree>
Reads src/features/clouds/cloudfly.cpp. Checks (all on the base text):
  1 one route call, GL never after Vulkan: `wwFlyBakeRoute( wantVk, false, vkRun, glSlice,` count == 1;
  2 the Vulkan call `WwVkPresent::flyBakeRun(` count == 1, and it sits inside the `auto vkRun = [` lambda;
  3 the red key gates the route: `shareWanted( "WW_VKVIEW_CLOUDFLYBAKE_RED" )` count == 1 (the volumes are shared only
    under it), `bool wantVk = g.shared` count == 1, `g.shared = true` count == 1;
  4 every GL slice draw `quad( r );` sits inside the `auto glSlice = [` lambda (draws > 0, all inside), and glSlice is
    named exactly twice (its definition and the route call): GL draws a slice only when the route hands it back.
Reds (in memory, never on disk), each must FAIL:
  glwrites  the route's `false` (GL after Vulkan) replaced by `true`;
  unguarded a `quad( r );` added before the route call (a GL draw outside the refusal path);
  redkey    the red key dropped (`shareWanted( nullptr )`);
  nowant    `bool wantVk = g.shared` replaced by `bool wantVk = true` (Vulkan tried on GL's own volumes).
Last line `FLYBAKESITE OK` or `FLYBAKESITE FAILED`.
"""
import os
import sys

ROUTE = 'wwFlyBakeRoute( wantVk, false, vkRun, glSlice,'
RED_KEY = 'shareWanted( "WW_VKVIEW_CLOUDFLYBAKE_RED" )'


def lambda_span(lines, head):
    """(first, last) line indices of the lambda whose definition line holds `head`, or None (not exactly one)."""
    at = [i for i, ln in enumerate(lines) if head in ln]
    if len(at) != 1:
        return None
    depth = 0
    opened = False
    for j in range(at[0], len(lines)):
        depth += lines[j].count('{') - lines[j].count('}')
        if '{' in lines[j]:
            opened = True
        if opened and depth <= 0:
            return at[0], j
    return None


def check(text, label):
    lines = text.splitlines()
    routes = text.count(ROUTE)
    runs = text.count('WwVkPresent::flyBakeRun(')
    vk = lambda_span(lines, 'auto vkRun = [')
    runs_in = sum(1 for i, ln in enumerate(lines) if 'WwVkPresent::flyBakeRun(' in ln and vk and vk[0] <= i <= vk[1])
    reds = text.count(RED_KEY)
    wants = text.count('bool wantVk = g.shared')
    shares = text.count('g.shared = true')
    gl = lambda_span(lines, 'auto glSlice = [')
    draws = [i for i, ln in enumerate(lines) if 'quad( r );' in ln]
    draws_in = sum(1 for i in draws if gl and gl[0] <= i <= gl[1])
    names = sum(ln.count('glSlice') for ln in lines)
    ok = (routes == 1 and runs == 1 and runs_in == 1 and reds == 1 and wants == 1 and shares == 1
          and len(draws) > 0 and draws_in == len(draws) and names == 2)
    print('FLYBAKESITE %s: route calls %d, vulkan calls %d (in vkRun %d), red key %d, wantVk from shared %d, shared set %d, '
          'GL slice draws %d (in glSlice %d), glSlice named %d: %s'
          % (label, routes, runs, runs_in, reds, wants, shares, len(draws), draws_in, names, 'OK' if ok else 'FAIL'))
    return ok


def main():
    tree = sys.argv[1]
    path = os.path.join(tree, 'src', 'features', 'clouds', 'cloudfly.cpp')
    with open(path, 'r', encoding='utf-8', errors='replace', newline='') as fh:
        text = fh.read()
    base = check(text, 'base')
    reds = [
        ('glwrites', text.replace('wwFlyBakeRoute( wantVk, false,', 'wwFlyBakeRoute( wantVk, true,')),
        ('unguarded', text.replace('ok = wwFlyBakeRoute(', 'quad( r );\n\tok = wwFlyBakeRoute(')),
        ('redkey', text.replace(RED_KEY, 'shareWanted( nullptr )')),
        ('nowant', text.replace('bool wantVk = g.shared', 'bool wantVk = true')),
    ]
    fired = 0
    for name, t in reds:
        moved = t != text
        r = check(t, 'red ' + name) if moved else True
        bit = moved and not r
        fired += bit
        print('FLYBAKESITE red %s: %s' % (name, 'FAIL (fired)' if bit else ('PASS (dead)' if moved else 'TESTS NOTHING (no text replaced)')))
    ok = base and fired == len(reds)
    print('FLYBAKESITE reds fired %d of %d' % (fired, len(reds)))
    print('FLYBAKESITE %s' % ('OK' if ok else 'FAILED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
