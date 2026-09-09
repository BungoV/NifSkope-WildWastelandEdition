"""Exact BSTriShape reader for FO4 terrain .BTR / object .BTO chunks.

Finds every BSTriShape-family geometry header by the PHANTOM4 signature
(`<IHI>` = numTriangles uint32, numVertices uint16, dataSize uint32, with
dataSize == numVertices*vertexSize + numTriangles*6) and reads the vertex
desc that sits in the 8 bytes before it.  Half-float positions are decoded
exactly, so gap measurements are not contaminated by printed precision.

Cross-checked against `nifskope-cli dump -b N` on Commonwealth.4.0.0.BTR:
numTriangles 4215, numVertices 11713, dataSize 165846, desc 52776558133763.
"""
import struct
import sys
import numpy as np


def find_shapes(data):
    shapes = []
    n = len(data)
    i = 0
    while i + 18 <= n:
        try:
            nt, nv, ds = struct.unpack_from('<IHI', data, i)
        except struct.error:
            break
        if 0 < nv <= 65535 and 0 < nt < 400000 and ds > 0:
            rem = ds - nt * 6
            if rem > 0 and rem % nv == 0:
                vsz = rem // nv
                if 8 <= vsz <= 64 and i >= 8:
                    desc = struct.unpack_from('<Q', data, i - 8)[0]
                    if (desc & 0xF) * 4 == vsz and i + 10 + ds <= n:
                        shapes.append({'off': i, 'numTris': nt, 'numVerts': nv,
                                       'dataSize': ds, 'vsize': vsz, 'desc': desc,
                                       'vertOff': i + 10, 'triOff': i + 10 + nv * vsz})
                        i += 10 + ds
                        continue
        i += 1
    return shapes


def read_shape(data, s):
    full = bool((s['desc'] >> 44) & 0x400)
    vo, vsz, nv = s['vertOff'], s['vsize'], s['numVerts']
    buf = np.frombuffer(data, dtype=np.uint8, count=nv * vsz, offset=vo).reshape(nv, vsz)
    if full:
        pos = buf[:, :12].copy().view(np.float32).astype(np.float64)
    else:
        pos = buf[:, :6].copy().view(np.float16).astype(np.float64)
    tri = np.frombuffer(data, dtype='<u2', count=s['numTris'] * 3,
                        offset=s['triOff']).reshape(s['numTris'], 3)
    return pos, tri


if __name__ == '__main__':
    data = open(sys.argv[1], 'rb').read()
    for s in find_shapes(data):
        pos, tri = read_shape(data, s)
        print(s['off'], s['numTris'], s['numVerts'], s['dataSize'], s['vsize'], s['desc'])
        print(pos[:4])
