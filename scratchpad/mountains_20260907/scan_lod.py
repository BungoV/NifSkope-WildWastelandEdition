"""Targeted scan: any generated terrain/object LOD under the FO4 mod stack,
the DynDOLOD tree, and the repo's scratch dirs. Cheaper than the whole-machine
walk and covers the places output would actually land."""
import os, re, sys

ROOTS = [
    r'E:\Projects\Fallout 4 Mods',
    r'E:\Tools\DynDOLOD',
    r'E:\Tools\xLODGen',
    r'E:\Projects\NifskopeWildWastelandEdition\tests',
    r'E:\Projects\NifskopeWildWastelandEdition\scratchpad',
    r'E:\Projects\NifskopeWildWastelandEdition\scratch_water',
    r'E:\Projects\NifskopeWildWastelandEdition\heightmaps',
    r'C:\Users\bungo\AppData\Local\Temp\TexGen_FO4',
    r'C:\Users\bungo\AppData\Local\Temp\DynDOLOD',
]
LOD = re.compile(r'^(\w+)\.(4|8|16|32|64|128)\.(-?\d+)\.(-?\d+)(_msn)?\.(dds|btr|bto)$', re.I)
TERRAINDIR = re.compile(r'(textures|meshes)[\\/]terrain[\\/]', re.I)

found = {}
terr = set()
for root in ROOTS:
    if not os.path.isdir(root):
        print('MISSING  ', root)
        continue
    n = 0
    for dp, dn, fn in os.walk(root, onerror=lambda e: None):
        if TERRAINDIR.search(dp + os.sep):
            terr.add(dp)
        for f in fn:
            if LOD.match(f):
                found.setdefault(dp, [0, set()])
                found[dp][0] += 1
                found[dp][1].add(os.path.splitext(f)[1].lower())
                n += 1
    print('SCANNED  %-52s  %d LOD-named files' % (root, n))

print()
print('=== directories with a textures/terrain or meshes/terrain path (%d) ===' % len(terr))
for d in sorted(terr):
    print('   ', d)
print()
print('=== directories holding LOD-named files (%d) ===' % len(found))
for d in sorted(found):
    n, e = found[d]
    print('    %6d  %-16s %s' % (n, ','.join(sorted(e)), d))
