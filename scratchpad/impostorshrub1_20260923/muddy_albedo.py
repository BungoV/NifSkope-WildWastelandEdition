"""IMPOSTORSHRUB1 muddy, stage 1 (albedo sheet), offline, no window:
the colour the MODEL's surface can have = its diffuse texels that survive the
alpha test x its vertex colours, against the colour the bake wrote into the
octahedral albedo sheet on FULLY covered texels (alpha 255, so no edge blend
enters). sRGB byte means and a Rec.709 luma on the bytes.

  python muddy_albedo.py NIF BLOCK DIFFUSE.dds BAKEDIR BASE [alpha-threshold]"""
import re, subprocess, sys
import numpy as np
from PIL import Image
NS = 'E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe'
nif, blk, dds, bake, base = sys.argv[1:6]
thr = int(sys.argv[6]) if len(sys.argv) > 6 else 128
L = np.array([0.2126, 0.7152, 0.0722])

def show(tag, rgb):
    m = rgb.mean(0)
    print('  %-44s n %7d  RGB %6.1f %6.1f %6.1f  luma %6.1f  sat %.3f' % (
        tag, len(rgb), m[0], m[1], m[2], (m * L).sum(), (m.max() - m.min()) / max(m.max(), 1e-6)))
    return m

d = np.asarray(Image.open(dds).convert('RGBA')).reshape(-1, 4).astype(float)
print('diffuse', dds, Image.open(dds).size)
show('diffuse, all texels', d[:, :3])
tex = d[d[:, 3] >= thr, :3]
mt = show('diffuse, alpha >= %d (what the alpha test keeps)' % thr, tex)

t = subprocess.run([NS, '-no-gui', 'dump', nif, '-b', blk, '--all', '-n', '1000000'],
                   capture_output=True, text=True).stdout
vc = np.array([[float(x) for x in m] for m in re.findall(
    r'Vertex Colors  <ByteColor4>  = R (\S+) G (\S+) B (\S+) A (\S+)', t)])
if len(vc):
    vc = vc[:, :3] if vc.max() > 1.001 else vc[:, :3] * 255
    mv = show('vertex colours (block %s)' % blk, vc)
    print('  vertex-colour factor (mean/255): %.3f %.3f %.3f' % tuple(mv / 255))
    show('diffuse(kept) x mean vertex colour', tex * (mv / 255))
else:
    print('  no vertex colours in block', blk)

a = np.asarray(Image.open('%s/%s_oct_albedo.png' % (bake, base)).convert('RGBA')).reshape(-1, 4).astype(float)
show('bake albedo sheet, alpha 255', a[a[:, 3] == 255, :3])
show('bake albedo sheet, alpha > 0', a[a[:, 3] > 0, :3])
