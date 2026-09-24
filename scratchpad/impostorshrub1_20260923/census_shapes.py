"""IMPOSTORSHRUB1: per-model shape census (through the exe's own headless CLI)
and the join with a bake folder's sidecars + albedo coverage.

  python census_shapes.py BAKEROOT OUT.tsv

Per model: the geometry shapes (BSTriShape / BSSubIndexTriShape /
BSMeshLODTriShape), for a BSMeshLODTriShape its LOD0/1/2 sizes, the shapes
the bake's name rule hides, and from the bake: the oct line's halfW/halfH, the
sidecar's `ranges` / `hidden` lines, and COVERED TEXELS = albedo alpha > 0
over the whole octahedral sheet (a real count, not the sidecar's word)."""
import os, re, subprocess, sys
import numpy as np
from PIL import Image

REPO = 'E:/Projects/NifskopeWildWastelandEdition'
NS = os.environ.get('NS', REPO + '/release/NifSkope.before_impostorshrub1.exe')
DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'
S = os.path.dirname(os.path.abspath(__file__))
root, out = sys.argv[1], sys.argv[2]

def cli(*a):
    r = subprocess.run([NS, '-no-gui'] + list(a), capture_output=True, text=True)
    return r.stdout.replace('\r', '')

models = {}
for line in open(os.path.join(S, 'bases.tsv')).read().splitlines()[1:]:
    f = line.split('\t')
    models.setdefault(f[3].lower(), []).append(f)

rows = []
for model in sorted(models):
    rel = model.replace('\\', '/')
    path = DATA + '/meshes/' + rel
    b = os.path.splitext(os.path.basename(rel))[0]
    placed = sum(int(f[5]) for f in models[model])
    types = ','.join(sorted(set(f[1] for f in models[model])))
    if not os.path.isfile(path):
        rows.append([b, types, model, placed, 'MISSING', '', '', '', '', '', ''])
        continue
    shapes, lods, namehidden = [], [], []
    for ln in cli('list', path).splitlines():
        m = re.match(r"\[(\d+)\] (BSTriShape|BSSubIndexTriShape|BSMeshLODTriShape) '(.*)'", ln)
        if not m:
            continue
        blk, typ, name = int(m.group(1)), m.group(2), m.group(3)
        if 'EditorMarker' in name:
            continue
        shapes.append(name)
        us = name.rfind('_L')
        if us >= 0 and us == len(name) - 3 and name[-1].isdigit():
            namehidden.append(name)
        if typ == 'BSMeshLODTriShape':
            l = [cli('get', path, '-b', str(blk), '-f', 'LOD%d Size' % k).strip() for k in range(3)]
            lods.append('/'.join(l))
    bake = os.path.join(root, b)
    oct_, hidden, ranges, cov, tot = '', [], [], -1, -1
    txt = os.path.join(bake, b + '.txt')
    if os.path.isfile(txt):
        for ln in open(txt).read().splitlines():
            if ln.startswith('oct '):
                oct_ = ln
            elif ln.startswith('hidden '):
                hidden.append(ln[7:])
            elif ln.startswith('ranges '):
                ranges.append(ln.rsplit(' ', 1)[1])
    png = os.path.join(bake, b + '_oct_albedo.png')
    if os.path.isfile(png):
        a = np.asarray(Image.open(png).convert('RGBA'))[..., 3]
        cov, tot = int((a > 0).sum()), int(a.size)
    o = oct_.split()
    halfW = o[4] if len(o) > 5 else ''
    halfH = o[5] if len(o) > 5 else ''
    frame = (o[2] + 'x' + o[3]) if len(o) > 3 else ''
    rows.append([b, types, model, placed, len(shapes), ';'.join(lods) or '-',
                 len(namehidden), frame, halfW, halfH, cov])
with open(out, 'w') as f:
    f.write('model\ttypes\tpath\tplacedCW\tshapes\tLOD0/1/2 per BSMeshLODTriShape\tnameHidden\tframe\thalfW\thalfH\tcoveredTexels\n')
    for r in rows:
        f.write('\t'.join(str(x) for x in r) + '\n')
empty = [r for r in rows if r[-1] == 0]
print('models', len(rows), 'baked', sum(1 for r in rows if r[-1] != '' and r[-1] >= 0),
      'EMPTY', len(empty), [r[0] for r in empty])
