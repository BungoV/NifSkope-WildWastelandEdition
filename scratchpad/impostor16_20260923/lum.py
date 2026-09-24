import sys, numpy as np, glob, re
from PIL import Image
d = sys.argv[1]
rs = []
for f in sorted(glob.glob(d + '/*_mesh.png')):
    r = []
    for k in ('mesh', 'card'):
        im = np.asarray(Image.open(f.replace('_mesh', '_' + k)).convert('RGB')).astype(float)
        bg = im[0, 0]; m = (im != bg).any(-1); r.append((im @ [.2126, .7152, .0722])[m].mean())
    rs.append(r[1] / r[0])
    if len(sys.argv) > 2: print(re.search(r'az\d+', f).group(), 'mesh %.1f card %.1f ratio %.3f' % (r[0], r[1], r[1] / r[0]))
rs = np.array(rs); print('%s: card/mesh luma ratio mean %.3f min %.3f max %.3f over %d views' % (d, rs.mean(), rs.min(), rs.max(), len(rs)))
