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
  5 (lane VKCLOUD9) the full build crosses once: the build loop hands runPass the queue (`p.nz, &q, nullptr, &qWhy )` once),
    vkRun queues (`queue->push_back( t );` once, inside vkRun), the one crossing `WwVkPresent::flyBakeRunN(` once, and the
    flush after the loop (`flyQueueRun( q, qWhy );` once).
  6 (lane VKCLOUD10) the density inputs: cloudvol.cpp calls `WwCloudInShare::ensureVol(` twice and no upload3d, cloudmap.cpp
    `WwCloudInShare::ensureMap(` and `WwCloudInShare::mapStale(` once and drops a shared map with `sharedTargetDrop( mapKey() )`;
    both files gate the share on `shareWanted( "WW_VKVIEW_CLOUDIN_RED" )`; cloudvol.cpp's and cloudfly.cpp's `vk` lines fold
    and name the inputs read in place (`WwCloudInShare::inPlaceNames( sum.inPlace )`).
Reds (in memory, never on disk), each must FAIL:
  glwrites  the route's `false` (GL after Vulkan) replaced by `true`;
  unguarded a `quad( r );` added before the route call (a GL draw outside the refusal path);
  redkey    the red key dropped (`shareWanted( nullptr )`);
  nowant    `bool wantVk = g.shared` replaced by `bool wantVk = true` (Vulkan tried on GL's own volumes);
  perpass   (lane VKCLOUD9) the build loop without the queue (a crossing a pass);
  noflush   (lane VKCLOUD9) the flush after the loop dropped (the queued passes never drawn).
  noensure / inkey / nostale / mapdelete / nosay / flynosay  (lane VKCLOUD10, check 6): a noise made by the old upload3d,
            the map's red key dropped, the stale-map rebuild dropped, the shared map deleted by GL, either line unnamed.
Last line `FLYBAKESITE OK` or `FLYBAKESITE FAILED`.
"""
import os
import sys

ROUTE = 'wwFlyBakeRoute( wantVk, false, vkRun, glSlice,'
RED_KEY = 'shareWanted( "WW_VKVIEW_CLOUDFLYBAKE_RED" )'
QUEUED = 'p.nz, &q, nullptr, &qWhy )'
PUSH = 'queue->push_back( t );'
FLUSH = 'flyQueueRun( q, qWhy );'


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
    # lane VKCLOUD9: one crossing a full build
    queued = text.count(QUEUED)
    pushes = text.count(PUSH)
    pushes_in = sum(1 for i, ln in enumerate(lines) if PUSH in ln and vk and vk[0] <= i <= vk[1])
    batch = text.count('WwVkPresent::flyBakeRunN(')
    flush = text.count(FLUSH)
    ok = (routes == 1 and runs == 1 and runs_in == 1 and reds == 1 and wants == 1 and shares == 1
          and len(draws) > 0 and draws_in == len(draws) and names == 2
          and queued == 1 and pushes == 1 and pushes_in == 1 and batch == 1 and flush == 1)
    print('FLYBAKESITE %s: route calls %d, vulkan calls %d (in vkRun %d), red key %d, wantVk from shared %d, shared set %d, '
          'GL slice draws %d (in glSlice %d), glSlice named %d; build queued %d, queue pushes %d (in vkRun %d), one-crossing calls %d, '
          'flush after the loop %d: %s'
          % (label, routes, runs, runs_in, reds, wants, shares, len(draws), draws_in, names, queued, pushes, pushes_in, batch, flush,
             'OK' if ok else 'FAIL'))
    return ok


IN_KEY = 'shareWanted( "WW_VKVIEW_CLOUDIN_RED" )'
SAY = 'WwCloudInShare::inPlaceNames( sum.inPlace )'
FOLD = 'sum.inPlace |= st.inPlace;'


def check_in(vol, cmap, fly, label):
    """lane VKCLOUD10, check 6: the clouds' density inputs are made through cloudinshare.h (shared while the view is on
    Vulkan), a shared map is dropped (never deleted) and rebuilt when stale, and both `vk` lines name the inputs read in place."""
    n = {
        'vol ensureVol': vol.count('WwCloudInShare::ensureVol('),
        'vol key': vol.count(IN_KEY),
        'vol upload3d': vol.count('upload3d'),
        'vol fold': vol.count(FOLD),
        'vol say': vol.count(SAY),
        'map ensureMap': cmap.count('WwCloudInShare::ensureMap('),
        'map key': cmap.count(IN_KEY),
        'map stale': cmap.count('WwCloudInShare::mapStale('),
        'map drop': cmap.count('sharedTargetDrop( mapKey() )'),
        'fly fold': fly.count(FOLD),
        'fly say': fly.count(SAY),
    }
    want = {'vol ensureVol': 2, 'vol upload3d': 0}
    ok = all(v == want.get(k, 1) for k, v in n.items())
    print('FLYBAKESITE %s inputs: %s: %s' % (label, ', '.join('%s %d' % kv for kv in n.items()), 'OK' if ok else 'FAIL'))
    return ok


def main():
    tree = sys.argv[1]
    path = os.path.join(tree, 'src', 'features', 'clouds', 'cloudfly.cpp')
    with open(path, 'r', encoding='utf-8', errors='replace', newline='') as fh:
        text = fh.read()
    src = {}
    for name in ('cloudvol.cpp', 'cloudmap.cpp'):
        with open(os.path.join(tree, 'src', 'features', 'clouds', name), 'r', encoding='utf-8', errors='replace', newline='') as fh:
            src[name] = fh.read()
    vol, cmap = src['cloudvol.cpp'], src['cloudmap.cpp']
    base_in = check_in(vol, cmap, text, 'base')
    reds_in = [
        ('noensure', (vol.replace('WwCloudInShare::ensureVol(', 'upload3d(', 1), cmap, text)),
        ('inkey', (vol, cmap.replace(IN_KEY, 'shareWanted( nullptr )'), text)),
        ('nostale', (vol, cmap.replace('WwCloudInShare::mapStale(', 'false && (', 1), text)),
        ('mapdelete', (vol, cmap.replace('sharedTargetDrop( mapKey() )', 'glDeleteTextures( 1, &s.tgt.tex )'), text)),
        ('nosay', (vol.replace(SAY, 'std::string( "?" )'), cmap, text)),
        ('flynosay', (vol, cmap, text.replace(SAY, 'std::string( "?" )'))),
    ]
    fired_in = 0
    for name, (v, m, t) in reds_in:
        moved = (v, m, t) != (vol, cmap, text)
        bit = moved and not check_in(v, m, t, 'red ' + name)
        fired_in += bit
        print('FLYBAKESITE red %s: %s' % (name, 'FAIL (fired)' if bit else ('PASS (dead)' if moved else 'TESTS NOTHING (no text replaced)')))
    base = check(text, 'base')
    reds = [
        ('glwrites', text.replace('wwFlyBakeRoute( wantVk, false,', 'wwFlyBakeRoute( wantVk, true,')),
        ('unguarded', text.replace('ok = wwFlyBakeRoute(', 'quad( r );\n\tok = wwFlyBakeRoute(')),
        ('redkey', text.replace(RED_KEY, 'shareWanted( nullptr )')),
        ('nowant', text.replace('bool wantVk = g.shared', 'bool wantVk = true')),
        ('perpass', text.replace(QUEUED, 'p.nz, nullptr, nullptr, &qWhy )')),
        ('noflush', text.replace(FLUSH, 'qWhy.clear();')),
    ]
    fired = 0
    for name, t in reds:
        moved = t != text
        r = check(t, 'red ' + name) if moved else True
        bit = moved and not r
        fired += bit
        print('FLYBAKESITE red %s: %s' % (name, 'FAIL (fired)' if bit else ('PASS (dead)' if moved else 'TESTS NOTHING (no text replaced)')))
    ok = base and fired == len(reds) and base_in and fired_in == len(reds_in)
    print('FLYBAKESITE reds fired %d of %d, inputs reds fired %d of %d' % (fired, len(reds), fired_in, len(reds_in)))
    print('FLYBAKESITE %s' % ('OK' if ok else 'FAILED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
