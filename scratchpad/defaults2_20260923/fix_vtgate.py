"""DEFAULTS2 re-rung of tests/spells/lodgen_terrain_vt.sh: the OLD-look arm (V9a-3) is the way back
to the bake before the rulings, so it now spells --blend-edges off as well. V9a-1/-2 (the shipped
default) are left at their bar, unchanged."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_terrain_vt.sh'
CHECK = '--check' in sys.argv
old = 'OLDLAND="--land-hex 0 --land-warp 0 --land-mip-bias 0 --land-guide off"\n'
new = ('# --blend-edges off joined the way back on 2026-09-23 (lane DEFAULTS2): the edge\n'
       '# blend became the default that day (bungo, "Yes, default on"), so the OLD look is\n'
       '# the four land switches AND the hard quadrant lines. The shipped-default pair\n'
       '# (V9a-1/-2) is NOT re-rung: with the blend on, the two colour writers disagree\n'
       '# (DEFAULTS2 measured 2,790 of 262,144 texels on chunk 4.-20.24, all within 4 px\n'
       '# of a quadrant line and 8 px of the chunk edge, max 25 levels) and it stays red.\n'
       'OLDLAND="--land-hex 0 --land-warp 0 --land-mip-bias 0 --land-guide off --blend-edges off"\n')
b = open(P, 'rb').read(); s = b.decode('utf-8')
assert s.count(old) == 1, s.count(old)
ns = s.replace(old, new).encode('utf-8')
assert ns.count(b'\r') == b.count(b'\r')
if not CHECK:
    open(P, 'wb').write(ns)
print('checked' if CHECK else 'patched', P)
