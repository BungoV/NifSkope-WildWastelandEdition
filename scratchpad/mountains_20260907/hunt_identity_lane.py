"""IDENTITY-lane copy of the forensic hunt (separate filename so it cannot
collide with the MOUNTAINS lane's hunt.py). Read-only.

Finds generated terrain-LOD artefacts anywhere outside the known vanilla
unpack: <ws>.<level>.<x>.<y>[_msn].dds / .btr / .bto, and any *_msn.dds at all.
"""
import os, re, sys, time

LOD_OUT = re.compile(r'^(.+)\.(4|8|16|32|64|128)\.(-?\d+)\.(-?\d+)(_msn)?\.(dds|btr|bto)$', re.I)
MSN = re.compile(r'_msn\.dds$', re.I)
SKIP = ('$recycle.bin', 'system volume information', 'windows\\winsxs',
        'node_modules', '\\.git\\', 'appdata\\local\\temp\\claude')
VANILLA = os.path.normcase(r'e:\tools\fallout 4\dataunpacked')

def run(roots, budget):
    start = time.time()
    groups = {}
    msn = {}
    visited = 0
    truncated = []
    for root in roots:
        if not os.path.isdir(root):
            print('MISSING', root); continue
        for dirpath, dirnames, filenames in os.walk(root, topdown=True,
                                                    onerror=lambda e: None):
            if time.time() - start > budget:
                truncated.append(dirpath); break
            visited += 1
            low = os.path.normcase(dirpath)
            if any(s in low for s in SKIP):
                dirnames[:] = []
                continue
            if low.startswith(VANILLA):
                continue
            for f in filenames:
                m = LOD_OUT.match(f)
                if m:
                    k = (dirpath, m.group(6).lower())
                    groups[k] = groups.get(k, 0) + 1
                if MSN.search(f):
                    msn[dirpath] = msn.get(dirpath, 0) + 1
    print('visited %d dirs in %.0fs' % (visited, time.time() - start))
    print('--- generated-looking LOD chunk/texture dirs OUTSIDE the vanilla unpack ---')
    for (d, e), n in sorted(groups.items()):
        print('  %6d .%s  %s' % (n, e, d))
    if not groups:
        print('  (none)')
    print('--- any *_msn.dds OUTSIDE the vanilla unpack ---')
    for d, n in sorted(msn.items()):
        print('  %6d  %s' % (n, d))
    if not msn:
        print('  (none)')
    if truncated:
        print('--- BUDGET EXHAUSTED, not fully searched: ---')
        for t in truncated:
            print('  ', t)

if __name__ == '__main__':
    run(sys.argv[2:], float(sys.argv[1]))
