import numpy as np
from PIL import Image

R = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/work/'
a = np.asarray(Image.open(R + 'lodl_lit.png').convert('RGB'), dtype=np.float64)
b = np.asarray(Image.open(R + 'btr_ctl2.png').convert('RGB'), dtype=np.float64)
print('lodl', a.shape, 'btr', b.shape)
h = min(a.shape[0], b.shape[0])
w = min(a.shape[1], b.shape[1])
a = a[:h, :w].mean(axis=2)
b = b[:h, :w].mean(axis=2)


def ncc(x, y):
    x = x - x.mean()
    y = y - y.mean()
    d = np.sqrt((x * x).sum() * (y * y).sum())
    return float((x * y).sum() / d) if d else 0.0


print('mean luma: lodl %.1f  btr %.1f' % (a.mean(), b.mean()))
print('ncc as is        %+.4f' % ncc(a, b))
print('ncc btr flipped Y %+.4f' % ncc(a, b[::-1, :]))
print('ncc btr flipped X %+.4f' % ncc(a, b[:, ::-1]))
print('ncc btr rot 180   %+.4f' % ncc(a, b[::-1, ::-1]))
# a coarse downsample kills the resolution gap and shows the low-frequency match
k = 16
hh, ww = (h // k) * k, (w // k) * k
A = a[:hh, :ww].reshape(hh // k, k, ww // k, k).mean(axis=(1, 3))
B = b[:hh, :ww].reshape(hh // k, k, ww // k, k).mean(axis=(1, 3))
print('16x downsampled:')
print('  ncc as is        %+.4f' % ncc(A, B))
print('  ncc btr flipped Y %+.4f' % ncc(A, B[::-1, :]))
print('  ncc btr flipped X %+.4f' % ncc(A, B[:, ::-1]))
print('  ncc btr rot 180   %+.4f' % ncc(A, B[::-1, ::-1]))
