"""Lane VKFLIGHT5 (2026-11-16): site check of the Vulkan view's opaque wiring.
1. Every GL draw call of the opaque / sorted / obscurance / editor draw functions counts itself (WwBl::glDrawn on the
   same line as the call), the sites found by objcensus's own scanner (never a hand list of lines).
2. vkShadeOn follows the view (WwBl::shadeWanted); nothing else in src reads WW_VKOBJ_SHADE by itself.
3. The pass brackets set the zones; WwBl::frame() hands the zones to the guard and says `vk opaque:` outside the
   vkShadeOn() blocks (so "the twins are off" is said too).
Usage: python -I opaquesite.py <tree> [red]   (red = one hook dropped in memory: must FAIL)"""
import os, re, sys

tree = sys.argv[1]
red = len(sys.argv) > 2 and sys.argv[2] == 'red'
sys.path.insert(0, os.path.join(tree, 'tests/vkframe/objcensus'))
import objcensus  # noqa: E402

# file -> functions whose raw GL draws must count (None = every function); exempt = gates-only keys
FILES = {
    'src/gl/glshape.cpp': None,
    'src/gl/glnode.cpp': ('Node::drawVertexSelection', 'Node::drawTriangleSelection'),
    'src/gl/bsshape.cpp': ('BSShape::drawShapes', 'BSShape::drawSelection'),
    'src/features/near/cellgpucull.cpp': ('draw',),
    'src/features/near/cellqueue.cpp': ('poolDraw',),
    'src/features/near/cellbindless.cpp': ('runIssue', 'drawElements', 'multiDrawElements'),
}
EXEMPT = {'src/gl/bsshape.cpp|BSShape::drawShapes|glDrawElements|2'}  # WW_CELL_VOLPART fx dump (gates only)
WRAPPERS = ('WwBl::drawElements', 'WwBl::multiDrawElements', 'GpuDriven::poolDraw', 'drawShape')

def read(rel):
    return open(os.path.join(tree, rel), encoding='utf-8', errors='replace').read()

fails = 0
def check(ok, what):
    global fails
    print('  %s %s' % ('ok  ' if ok else 'FAIL', what))
    fails += 0 if ok else 1

sites, missing, dropped = 0, [], False
for rel, funcs in FILES.items():
    text = read(rel)
    lines = text.splitlines()
    if red and not dropped and rel == 'src/gl/glshape.cpp':
        for i, ln in enumerate(lines):
            if 'WwBl::glDrawn( 1 ), ' in ln:
                lines[i] = ln.replace('WwBl::glDrawn( 1 ), ', '', 1)
                dropped = True
                break
        text = '\n'.join(lines)
    found, _ = objcensus.scan_text(rel, text)
    for key, n, func, tok in found:
        if tok in WRAPPERS or key in EXEMPT or (funcs is not None and func not in funcs):
            continue
        sites += 1
        if 'glDrawn(' not in lines[n - 1]:
            missing.append('%s:%d %s' % (rel, n, tok))
check(sites >= 25, 'GL draw calls of the opaque / sorted / obscurance / editor functions: %d (25 on 11-16)' % sites)
check(not missing, 'each counts itself on its own line (missing %d: %s)' % (len(missing), '; '.join(missing[:4])))

bl = read('src/features/near/cellbindless.cpp')
check('#include "features/near/cellglzone.h"' in read('src/features/near/cellbindless.h'), 'cellbindless.h includes cellglzone.h')
m = re.search(r'bool vkShadeOn\(\)\s*\{(.*?)\n\}', bl, re.S)
check(m is not None and 'shadeWanted(' in m.group(1) and '"WW_VKVIEW"' in m.group(1),
      'vkShadeOn follows the view: WwBl::shadeWanted( WW_VKOBJ_SHADE, WW_VKVIEW )')
readers = []
for dp, dn, fn in os.walk(os.path.join(tree, 'src')):
    for f in fn:
        if f.endswith(('.cpp', '.h', '.inc', '.hpp')):
            p = os.path.join(dp, f)
            for i, ln in enumerate(open(p, encoding='utf-8', errors='replace'), 1):
                if re.search(r'wwEnv\(\s*"WW_VKOBJ_SHADE"', ln):
                    readers.append('%s:%d' % (os.path.relpath(p, tree).replace('\\', '/'), i))
inShade = [r for r in readers if r.startswith('src/features/near/cellbindless.cpp')]
check(len(readers) == 1 and len(inShade) == 1, 'WW_VKOBJ_SHADE read once, in vkShadeOn (readers: %s)' % ', '.join(readers))

gs, gv = read('src/gl/glscene.cpp'), read('src/glview.cpp')
for zone, src, name in (('ZoneOpaque', gs, 'glscene.cpp'), ('ZoneSorted', gs, 'glscene.cpp'),
                        ('ZoneObscurance', gv, 'glview.cpp'), ('ZoneOverlays', gv, 'glview.cpp')):
    check(('WwBl::glZoneSet( WwBl::%s )' % zone) in src, '%s sets %s' % (name, zone))
fm = re.search(r'\nvoid frame\(\)\s*\{(.*?)\n\}', bl, re.S)
body = fm.group(1) if fm else ''
op = body.find('"vk opaque:')
first = body.find('if ( vkShadeOn() )')
check('WwVkPresent::glGuard( glZoneName(' in body, 'frame() hands the zones to the guard')
check(op >= 0 and (first < 0 or op < first), 'frame() says `vk opaque:` before (outside) the vkShadeOn() blocks')
print('OPAQUESITE %s (%d sites)' % ('FAILED' if fails else 'OK', sites))
sys.exit(1 if fails else 0)
