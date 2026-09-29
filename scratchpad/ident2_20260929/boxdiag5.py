import sys, numpy as np
exec(open('boxdiag.py').read().split("for bi in")[0])
for bi in map(int, sys.argv[3:]):
    o = T['occluders'][bi]
    R = np.array(ND.unpack_rotation(o['r0'], o['r1'], o['r2'])).reshape(3, 3)
    half = np.array([o['hx'], o['hy'], o['hz']]); c = np.array([o['x'], o['y'], o['z']])
    tr, nm = box_tris(o)
    for lab, h in (('file box', half), ('emitter box (/0.999)', half / 0.999), ('emitter probe box (/0.999 + 0.5)', half / 0.999 + 0.5)):
        print('box %d %-34s out %.4f' % (bi, lab, 1 - G.inside_share(tr, c, R, h, 'xXyYz', 9)))
    yaw = np.degrees(np.arctan2(R[1, 0], R[0, 0]))
    print('   R yaw %.4f deg, R[2] %s' % (yaw, np.round(R[2], 6)))
