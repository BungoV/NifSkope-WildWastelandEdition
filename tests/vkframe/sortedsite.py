"""Lane VKFLIGHT6 (2026-11-16): site check of the sorted pass on Vulkan.
1. vkBlDraw takes a blended rigid shape (GL's blend reaches the pipeline through objBlTargetState, as the fade band and the
   blended alpha-tested shapes already do): no GL_BLEND refusal left in its body.
2. Each twin entry (vkBlDraw, vkCellDraw, vkPfxDraw, runIssue's run) notes its draws (zoneVk) and its refusals (zoneWhy),
   so GL's draw after a refusal is counted by that reason.
3. The particles' GL draw counts itself (countGl on the call's line).
4. frame() builds the `vk sorted:` line (zoneLine) before the zones are reset and says it outside the vkShadeOn() blocks.
Usage: python -I sortedsite.py <tree> [red]   (red = the old GL_BLEND refusal put back in memory: must FAIL)"""
import os, re, sys

tree = sys.argv[1]
red = len(sys.argv) > 2 and sys.argv[2] == 'red'

def read(rel):
    return open(os.path.join(tree, rel), encoding='utf-8', errors='replace').read()

fails = 0
compared = 0
def check(ok, what):
    global fails, compared
    compared += 1
    print('  %s %s' % ('ok  ' if ok else 'FAIL', what))
    fails += 0 if ok else 1

def body(text, sig):
    m = re.search(re.escape(sig) + r'[^\n]*(?:\n[^\n{]*)*\n\{(.*?)\n\}', text, re.S)
    return m.group(1) if m else ''

bl = read('src/features/near/cellbindless.cpp')
if red:
    bl = bl.replace('\tstatic std::vector<std::uint32_t> cmds;\n\tcmds.clear();\n\tconst std::uintptr_t idxOff',
                    '\tif ( rigid && !fadeBand && gf->glIsEnabled( GL_BLEND ) )\n\t\treturn refuse( "a blended rigid shape" );\n'
                    '\tstatic std::vector<std::uint32_t> cmds;\n\tcmds.clear();\n\tconst std::uintptr_t idxOff', 1)
vb = body(bl, 'bool vkBlDraw(')
vc = body(bl, 'bool vkCellDraw(')
vp = body(bl, 'bool vkPfxDraw(')
ri = body(bl, 'bool runIssue(')
check(len(vb) > 1000 and len(vc) > 500 and len(vp) > 500 and len(ri) > 200,
      'twin bodies found (vkBlDraw %d, vkCellDraw %d, vkPfxDraw %d, runIssue %d chars)' % (len(vb), len(vc), len(vp), len(ri)))
blend = re.findall(r'glIsEnabled\(\s*GL_BLEND\s*\)', vb)
check(not blend, 'vkBlDraw refuses no blended shape (GL_BLEND reads in its body: %d)' % len(blend))
ref = re.search(r'auto refuse = \[&\]\( const std::string & w \) \{(.*?)\n\t\};', vb, re.S)
check(ref is not None and 'zoneWhy( w, true )' in ref.group(1), 'vkBlDraw refuse() notes its reason (zoneWhy named)')
for name, b, vk, weak, named in (('vkBlDraw', vb, 'zoneVk( 1 )', 3, 0), ('vkCellDraw', vc, 'zoneVk( 1 )', 2, 2),
                                 ('vkPfxDraw', vp, 'zoneVk( 1 )', 1, 2), ('runIssue', ri, 'zoneVk( run.rows )', 0, 0)):
    w = len(re.findall(r'zoneWhy\([^;]*,\s*false \)', b))
    nm = len(re.findall(r'zoneWhy\([^;]*,\s*true \)', b))
    check(vk in b, '%s notes its twin draws (%s)' % (name, vk))
    check(w >= weak and nm >= named, '%s notes its refusals (weak %d of %d, named %d of %d)' % (name, w, weak, nm, named))

gp = read('src/features/particles/glparticles.cpp').splitlines()
draws = [i for i, ln in enumerate(gp, 1) if re.search(r'\bglDraw(Arrays|Elements)\w*\s*\(', ln) and 'wwVolPartFxDump' not in ln]
uncounted = [i for i in draws if 'countGl(' not in gp[i - 1]]
check(len(draws) >= 1 and not uncounted, 'glparticles.cpp GL draws count themselves (%d sites, uncounted lines %s)' % (len(draws), uncounted))

z = read('src/features/near/cellglzone.h')
cg = body(z, 'inline void countGl(')
check('whyN[' in cg and 'why.clear()' in cg, 'countGl tallies GL draws by the refusal before them and clears it')
fm = re.search(r'\nvoid frame\(\)\s*\{(.*?)\n\}', bl, re.S)
fb = fm.group(1) if fm else ''
ln = fb.find('zoneLine( ZoneSorted, "sorted"')
rs = fb.find('zoneReset()')
say = fb.find('vkSortedLine')
first = fb.find('if ( vkShadeOn() )')
check(ln >= 0 and rs > ln, 'frame() builds the sorted line before zoneReset (line at %d, reset at %d)' % (ln, rs))
check(say >= 0 and (first < 0 or fb.find('vkSortedLine', rs) < first), 'frame() says `vk sorted:` outside the vkShadeOn() blocks')
print('SORTEDSITE %s (%d checks compared, %d failed)%s' % ('FAILED' if fails else 'OK', compared, fails, ' RED blend-refusal' if red else ''))
sys.exit(1 if fails else 0)
