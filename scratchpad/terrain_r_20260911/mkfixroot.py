import sys, os, shutil

sys.path.insert(0, 'tests/spells')
from lodgen_cover_model import Esm

DATA = r'E:/Tools/Fallout 4/DataUnpacked/Data'
OUT = r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/terrain_r_20260911/out/fixroot'

e = Esm(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm')
e.walk(0x3C)
if os.path.isdir(OUT):
    shutil.rmtree(OUT)

forms = set()
for cy in (23, 24, 25, 26):
    for cx in (-21, -20, -19, -18):
        l = e.lands.get((cx, cy))
        if not l:
            continue
        for q in range(4):
            if l['base'][q]:
                forms.add(l['base'][q])
            for lay in l['layers'][q]:
                if lay['ltex']:
                    forms.add(lay['ltex'])

paths = set()
for f in forms:
    r = e.ltex.get(f)
    if not r:
        continue
    t = e.txst.get(r['tnam'])
    if not t:
        continue
    for k in ('tx00', 'tx01', 'tx07'):
        if t[k]:
            paths.add(t[k])
    if t['mnam']:
        paths.add(t['mnam'])

n = 0
tot = 0
missing = []
for p in sorted(paths):
    q = p.replace('\\', '/')
    low = q.lower()
    if low.endswith('.bgsm') or low.endswith('.bgem'):
        i = low.rfind('materials/')
        q = q[i:] if i >= 0 else 'materials/' + q
    elif not low.startswith('textures/'):
        q = 'textures/' + q
    src = os.path.join(DATA, q.replace('/', os.sep))
    if not os.path.isfile(src):
        missing.append(q)
        continue
    dst = os.path.join(OUT, q.replace('/', os.sep))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)
    n += 1
    tot += os.path.getsize(src)

print('forms %d  copied %d  bytes %d  missing %d' % (len(forms), n, tot, len(missing)))
for m in missing[:10]:
    print('  missing %s' % m)
