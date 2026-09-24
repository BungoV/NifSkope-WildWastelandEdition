"""IMPOSTORSHRUB1 job 1: every vanilla base that is a bush / shrub / sapling /
undergrowth, by record type and model path, with its LOD slot count and its
Commonwealth placement count. Reads Fallout4.esm through the repo's own reader
(tools/lod_emission_probe.py readPlugin). Output: bases.tsv"""
import os, re, sys
from collections import Counter
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tools')
import lod_emission_probe as P

ESM = P.DEFAULT_ESM
DATA = P.DEFAULT_DATA
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'bases.tsv')
PAT = re.compile(r'shrub|(?<!am)bush|sapling|undergrowth|(?<!tras)hedge', re.I)

worldOf, cellWorld, cellXY, baseOf, refs = P.readPlugin(ESM)
placed = Counter(b for b, w, c in refs if w == P.COMMONWEALTH)
rows = []
for form, (typ, edid, modl, mnam) in baseOf.items():
    if not modl:
        continue
    if not (PAT.search(modl) or PAT.search(edid)):
        continue
    rel = modl.replace(P.BS, '/')
    path = os.path.join(DATA, 'meshes', rel)
    rows.append((form, typ, edid, modl, len(mnam), placed.get(form, 0), os.path.isfile(path)))
rows.sort(key=lambda r: (r[3].lower(), r[0]))
with open(OUT, 'w') as f:
    f.write('form\ttype\tedid\tmodel\tmnamSlots\tplacedCW\tonDisk\n')
    for r in rows:
        f.write('%08x\t%s\t%s\t%s\t%d\t%d\t%d\n' % r)
models = sorted(set(r[3].lower() for r in rows))
print('bases', len(rows), 'distinct models', len(models),
      'types', dict(Counter(r[1] for r in rows)),
      'with MNAM', sum(1 for r in rows if r[4]),
      'placed in Commonwealth', sum(1 for r in rows if r[5]),
      'placements', sum(r[5] for r in rows),
      'model missing on disk', sum(1 for r in rows if not r[6]))
