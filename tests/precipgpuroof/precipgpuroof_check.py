"""lane BUGS1102B (PRECIPROOF-GPU2): the GPU precipitation field's roof test against WwRoofMap::open, on the real driver.

    python -I tests/precipgpuroof/precipgpuroof_check.py <precipgpuroof_dump exe> [--red off|axes]

It runs res/shaders/cell_precip_gpu.vert as tests/precipgpu does (transform feedback, its 6 cases), once with pcRoofOn
false and once with the fixture roof map (precipgpuroof_dump `roof`) uploaded as R32F, as cellprecip.cpp uploads it, and
the uniforms from preciproof.h wwRoofCam. Gate R, every case: with the map on, a particle is culled exactly when it was
culled without the map OR WwRoofMap::open says it is under a roof (no open particle lost, no roofed one kept). Particles
within 0.1 units of a texel edge or of the roof's height, or on the box's wrap seam, are counted apart (float vs double).
The fixture must hold >= 200 roofed particles in view and >= 200 open ones.
Reds (must FAIL): --red off = pcRoofOn left false (the GPU field with no roof test, WW_CELL_PRECIP_RED=gpuroof),
--red axes = PC_ROOF_RED (the map read across its axes). Prints PRECIPGPUROOF-CHECK PASS|FAIL."""
import ctypes, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'precipgpu'))
import precipgpu_check as PG  # noqa: E402

S = PG.S
UNIT = 87  # src/gl/cellunits.h WwCellUnit::PrecipRoof
TOL = 0.1


def roof(exe, c):
    args = [exe] + [str(v) for v in (c[1], c[2], c[3], c[4], c[5], *c[6], *c[7], c[8], c[9], c[10], c[11], c[12], c[13])] + ['roof']
    env = dict(os.environ)
    env['PATH'] = r'C:\msys64\ucrt64\bin;' + env.get('PATH', '')
    out = subprocess.run(args, capture_output=True, text=True, env=env, check=True).stdout.split('\n')
    hd = out[0].split()
    assert hd[0] == 'map'
    nx, ny = int(hd[1]), int(hd[2])
    cam = (float(hd[3]), float(hd[4]), float(hd[5]))
    inv = float(hd[6])
    Z = np.array([[float(x) for x in l.split()[1:]] for l in out[1:1 + ny]], dtype=np.float32)
    assert Z.shape == (ny, nx), Z.shape
    O = np.array([[float(x) for x in l.split()[1:]] for l in out[1 + ny:] if l.startswith('o ')], dtype=np.float64)
    return nx, ny, cam, inv, Z, O


class Tex:
    def __init__(self):
        g = S.gl
        g.glGenTextures.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_uint)]
        g.glBindTexture.argtypes = [ctypes.c_uint, ctypes.c_uint]
        g.glTexParameteri.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_int]
        g.glPixelStorei.argtypes = [ctypes.c_uint, ctypes.c_int]
        g.glTexImage2D.argtypes = [ctypes.c_uint, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                   ctypes.c_uint, ctypes.c_uint, ctypes.c_void_p]
        self.active = S.fn('glActiveTexture', None, ctypes.c_uint)
        self.t = ctypes.c_uint()
        g.glGenTextures(1, ctypes.byref(self.t))

    def upload(self, Z):
        g = S.gl
        self.active(0x84C0 + UNIT)
        g.glBindTexture(0x0DE1, self.t)
        z = np.ascontiguousarray(Z, np.float32)
        g.glPixelStorei(0x0CF5, 4)
        g.glTexImage2D(0x0DE1, 0, 0x822E, Z.shape[1], Z.shape[0], 0, 0x1903, 0x1406, z.ctypes.data)  # R32F, RED, FLOAT
        for p, v in ((0x2801, 0x2600), (0x2800, 0x2600), (0x813D, 0), (0x2802, 0x812F), (0x2803, 0x812F)):
            g.glTexParameteri(0x0DE1, p, v)
        self.active(0x84C0)


def main():
    a = sys.argv[1:]
    exe = a[0]
    red = a[a.index('--red') + 1] if '--red' in a else None
    gl = PG.Gl(None, ['PC_ROOF_RED'] if red == 'axes' else [])
    u3f = S.fn('glUniform3f', None, ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float)
    tex = Tex()
    P = PG.proj()
    fails = 0
    tot_roofed = tot_open = tot_amb = 0
    for ci, c in enumerate(PG.CASES):
        g, ref = PG.dump(exe, c)
        g['t'] = c[5]
        nx, ny, rc, inv, Z, O = roof(exe, c)
        assert len(O) == len(ref)
        R = PG.view_rot(0.7 + 1.3 * ci, -0.15 + 0.07 * ci)
        gl.useProgram(gl.p)
        gl.u1i(gl.L('pcRoofOn'), 0)
        gl.u1i(gl.L('pcRoof'), UNIT)
        o0 = gl.run(g, R, P)
        tex.upload(Z)
        gl.useProgram(gl.p)
        gl.u1i(gl.L('pcRoofOn'), 0 if red == 'off' else 1)
        u3f(gl.L('pcRoofCam'), *rc)
        gl.u1f(gl.L('pcRoofInv'), inv)
        o1 = gl.run(g, R, P)
        L = g['L']
        seam = (np.abs(o1[:, 0:3] - ref[:, 2:5]) > 0.5 * L).any(1)
        amb = seam | (O[:, 2] < TOL) | (O[:, 3] < TOL)
        opn = O[:, 1] > 0.5
        c0, c1 = o0[:, 13] > 0.5, o1[:, 13] > 0.5
        want = c0 | ~opn
        lost = int((~amb & opn & c1 & ~c0).sum())
        kept = int((~amb & ~opn & ~c1 & ~c0).sum())
        other = int((~amb & (c1 != want)).sum()) - lost - kept
        roofed = int((~amb & ~opn & ~c0).sum())
        opened = int((~amb & opn & ~c0).sum())
        tot_roofed += roofed; tot_open += opened; tot_amb += int(amb.sum())
        ok = lost == 0 and kept == 0 and other == 0
        print('%-20s n %6d map %3d x %3d | in view: roofed %6d open %6d | lost-open %d kept-roofed %d other %d '
              'apart %d %s' % (c[0], len(ref), nx, ny, roofed, opened, lost, kept, other, int(amb.sum()), 'ok' if ok else 'FAIL'))
        fails += not ok
    if tot_roofed < 200 or tot_open < 200:
        print('fixture too thin: roofed %d open %d in view' % (tot_roofed, tot_open))
        fails += 1
    print('in view: %d roofed, %d open; %d counted apart (texel edge / roof height / seam)' % (tot_roofed, tot_open, tot_amb))
    print('PRECIPGPUROOF-CHECK', 'PASS' if fails == 0 else 'FAIL', '(%d gate fails, red %s)' % (fails, red))
    return 0 if fails == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
