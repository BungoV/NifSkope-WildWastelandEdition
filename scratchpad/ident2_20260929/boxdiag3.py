import sys, collections, numpy as np
exec(open('boxdiag.py').read().split("for bi in")[0])
for bi in (438,):
    o = T['occluders'][bi]
    R = np.array(ND.unpack_rotation(o['r0'], o['r1'], o['r2'])).reshape(3, 3)
    half = np.array([o['hx'], o['hy'], o['hz']]); c = np.array([o['x'], o['y'], o['z']])
    tr, nm = box_tris(o)
    loc = np.einsum('ij,tvi->tvj', R, tr - c)
    zs = loc[:, :, 2].ravel()
    inxy = (np.abs(loc[:, :, 0]) < half[0] + 60).ravel() & (np.abs(loc[:, :, 1]) < half[1] + 60).ravel()
    zz = np.sort(np.unique(np.round(zs[inxy], 2)))
    print('half', half, 'vertex z (box frame) near the top face +-40:', zz[(zz > half[2] - 40) & (zz < half[2] + 40)])
    # triangles that span the top plane height and are near-vertical (walls)
    zmin = loc[:, :, 2].min(1); zmax = loc[:, :, 2].max(1)
    walls = (zmin < half[2]) & (zmax > half[2] - 30)
    print('triangles reaching within 30 u of the top plane:', walls.sum(), 'of which reach above it:', ((zmin < half[2]) & (zmax >= half[2])).sum())
    print('highest wall tops (box frame z) among tris near the footprint:', np.sort(np.unique(np.round(zmax[inxy.reshape(-1,3).all(1)], 2)))[-12:])
