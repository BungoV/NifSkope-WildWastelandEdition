"""TERRLIVE1 rework: classify every .lodl cell (no land / land without LTEX / land with LTEX) and write a map.
usage: python cells.py <file.lodl> <out.npz>"""
import struct, sys
import numpy as np
p, out = sys.argv[1], sys.argv[2]
f = open(p, 'rb')
h = f.read(0xA0)
minX, minY, maxX, maxY = struct.unpack_from('<4i', h, 0x08)
nLtex = struct.unpack_from('<I', h, 0x30)[0]
oQuad, oCell = struct.unpack_from('<2Q', h, 0x60)
cx, cy = maxX - minX + 1, maxY - minY + 1
f.seek(oQuad); q = np.frombuffer(f.read(cx * 2 * cy * 2 * 12), '<u2').reshape(cy * 2, cx * 2, 6)
f.seek(oCell); c = np.frombuffer(f.read(cx * cy * 16), np.uint8).reshape(cy, cx, 16)
flags = c[:, :, 14].astype(np.uint16) | (c[:, :, 15].astype(np.uint16) << 8)
land = (flags & 2) != 0
realq = (q < nLtex).any(axis=2)                     # a quadrant with any real LTEX slot (base or layer)
baseq = q[:, :, 5] < nLtex
real = realq.reshape(cy, 2, cx, 2).any(axis=(1, 3))
base = baseq.reshape(cy, 2, cx, 2).any(axis=(1, 3))
cls = np.where(~land, 0, np.where(real, 2, 1)).astype(np.uint8)
print(f'cells {cx}x{cy} from ({minX},{minY}); ltex {nLtex}; no land {int((cls==0).sum())}, land without LTEX '
      f'{int((cls==1).sum())}, land with LTEX {int((cls==2).sum())} (with a real base {int(base.sum())}); '
      f'quadrants with LTEX {int(realq.sum())} of {realq.size}')
np.savez(out, cls=cls, realq=realq, minX=minX, minY=minY)
