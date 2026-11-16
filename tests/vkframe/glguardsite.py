"""Lane VKFLIGHT4 (2026-11-16): site check of the Vulkan view's GL guard.
Every `void <route>OnGl( const char * why )` in src/features/vkframe (*.inc + vkpresent.cpp) adds to the guard on its
first line; overlayGl too; vkpresent.cpp includes vkglguard.inc and frame() says glGuardTake while the view runs.
Usage: python -I glguardsite.py <tree> [red]   (red = one hook dropped in memory: must FAIL)"""
import glob, os, re, sys

tree = sys.argv[1]
red = len(sys.argv) > 2 and sys.argv[2] == 'red'
vk = os.path.join(tree, 'src/features/vkframe')
files = sorted(glob.glob(os.path.join(vk, '*.inc'))) + [os.path.join(vk, 'vkpresent.cpp')]
defn = re.compile(r'^void (\w+)OnGl\( const char \* why \)\r?\n\{\r?\n(.*)$', re.M)
routes, missing = 0, []
dropped = False
for f in files:
    t = open(f, encoding='utf-8', errors='replace').read()
    for m in defn.finditer(t):
        routes += 1
        line = m.group(2)
        if red and not dropped:
            line, dropped = '', True
        if ('glGuard( "%s", 1 )' % m.group(1)) not in line:
            missing.append('%s:%s' % (os.path.basename(f), m.group(1)))
fails = 0
def check(ok, what):
    global fails
    print('  %s %s' % ('ok  ' if ok else 'FAIL', what))
    fails += 0 if ok else 1
check(routes >= 41, 'routes with an OnGl found: %d (41 on 11-16)' % routes)
check(not missing, 'every route OnGl adds to the guard on its first line (missing %d: %s)' % (len(missing), ', '.join(missing[:5])))
ov = open(os.path.join(vk, 'vkoverlay.inc'), encoding='utf-8').read()
check('glGuard( "overlay", 1 )' in ov, 'overlayGl adds to the guard')
pc = open(os.path.join(vk, 'vkpresent.cpp'), encoding='utf-8', errors='replace').read()
check('#include "features/vkframe/vkglguard.inc"' in pc, 'vkpresent.cpp includes vkglguard.inc')
check(re.search(r'glGuardTake\( &gN, &gM \);\s*\n\s*if \( winView > 0 \)\s*\n\s*qInfo\(\)', pc) is not None,
      'frame() takes the guard every window and says it while the view runs')
print('GLGUARDSITE %s (%d routes)' % ('FAILED' if fails else 'OK', routes))
sys.exit(1 if fails else 0)
