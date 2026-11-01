"""lane PERFFIX3 D (2026-11-01): the GPU precipitation field against its CPU twin, on the real driver, no NifSkope.

    python -I tests/precipgpu/precipgpu_check.py <dump exe> [--red field|cull] [-v]

It runs res/shaders/cell_precip_gpu.vert (with PC_TEST: it also writes the camera-relative position) in a hidden 4.1
core context with transform feedback and the rasterizer off, the uniforms taken from precipmath.h gpuRun through the
dump exe (never re-typed), and compares every particle with precipmath.h particleAt (the dump's `p` lines).
Gates (each case: rain / snow / hail, near and far tier, t = 0 .. 3600 s, a camera far from the origin, wind):
  F field     |position - CPU| (modulo the box side: a particle ON the wrap seam may sit at either face) <= 0.05 units,
              velocity <= 1e-3 relative, half size <= 1e-5 relative, angle (mod 2 pi) <= 0.01 rad, atlas cell exact,
              inside-the-near-box flag exact, for every particle
  C cull      every particle whose drawn quad (cell_precip_gpu.geom's corners, the CPU positions) overlaps the view
              is kept; also prints the kept share (the rest the GPU never lights)
Reds (must FAIL): --red field = PC_FIELD_RED (the variant pick dropped: gate F), --red cull = PC_CULL_RED (the quad's
extent ignored: gate C). Prints PRECIPGPU-CHECK PASS|FAIL."""
import ctypes, math, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
FP = ctypes.POINTER(ctypes.c_float)
IP = ctypes.POINTER(ctypes.c_int)

# type ids: precipmath.h Type (Rain 0, Hail 2, SnowFine 3, SnowBigClump 6)
CASES = [
    # name, type, size, rate, seed, t, cam, wind, far, drawScale, spin, sway, windShare, weight
    ('rain_near_t0', 0, 2.0, 4.0, 11, 0.0, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 0, 1.0, 0.0, 0.0, 1.0, 1.0),
    ('rain_far_t37', 0, 2.0, 4.0, 11, 37.25, (-60500.0, 70000.0, 6300.0), (120.0, -40.0), 1, 1.0, 0.0, 0.0, 1.0, 1.0),
    ('snow_near_t3600', 3, 5.5, 1.0, 97, 3600.5, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 0, 3.0, 0.5, 6.0, 1.0, 1.0),
    ('snow_far_t600_w06', 3, 5.5, 1.0, 97, 600.125, (12345.6, -98765.4, 7210.0), (-55.0, 210.0), 1, 3.0, 0.5, 6.0, 1.0, 0.6),
    ('hail_near_t95', 2, 10.0, 10.0, 5, 95.5, (0.0, 0.0, 450.0), (0.0, 0.0), 0, 1.0, 2.0, 0.0, 1.0, 1.0),
    ('bigclump_near_t12', 6, 20.0, 1.0, 3, 12.0, (-4096.0, 81920.0, 1300.0), (300.0, 50.0), 0, 1.5, 0.4, 8.0, 0.7, 1.0),
]
W, H, FOV = 1920, 1026, 55.0
MINPX, SHUTTER = 0.75, 1.0 / 120.0
VARY = [b'vsRel', b'vsView', b'vsVel', b'vsHalf', b'vsAngle', b'vsCell', b'vsCull', b'vsColor']
NF = 3 + 3 + 3 + 1 + 1 + 2 + 1 + 4


def dump(exe, c):
    args = [exe] + [str(v) for v in (c[1], c[2], c[3], c[4], c[5], *c[6], *c[7], c[8], c[9], c[10], c[11], c[12], c[13])]
    env = dict(os.environ)
    env['PATH'] = r'C:\msys64\ucrt64\bin;' + env.get('PATH', '')
    out = subprocess.run(args, capture_output=True, text=True, env=env, check=True).stdout.split('\n')
    run = out[0].split()
    assert run[0] == 'run'
    v = run[1:]
    g = {'seedS': int(v[0]), 'L': float(v[1]), 'inner': float(v[2]), 'thin': float(v[3]),
         'offXY': (float(v[4]), float(v[5])), 'offZ': [float(x) for x in v[6:22]], 'nCand': int(v[22]), 'best': int(v[23]),
         'cand': [int(x) for x in v[24:40]], 'fallU': [float(x) for x in v[40:56]], 'halfU': [float(x) for x in v[56:72]],
         'wind': (float(v[72]), float(v[73])), 'sway': float(v[74]), 'spin': float(v[75]), 'count': int(v[76]),
         'cols': int(v[77]), 'cells': int(v[78])}
    P = np.array([[float(x) for x in l.split()[1:]] for l in out[1:] if l.startswith('p ')], dtype=np.float64)
    return g, P


def view_rot(yaw, pitch):
    """world -> view rotation (rows): the camera looks along d (yaw about z, pitch up), up = +z"""
    d = np.array([math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), math.sin(pitch)])
    r = np.cross(d, [0, 0, 1.0]); r /= np.linalg.norm(r)
    u = np.cross(r, d)
    return np.array([r, u, -d])


def proj():
    f = 1.0 / math.tan(math.radians(FOV) / 2)
    n, fa = 1.0, 200000.0
    return np.array([[f * H / W, 0, 0, 0], [0, f, 0, 0], [0, 0, (n + fa) / (n - fa), 2 * n * fa / (n - fa)], [0, 0, -1, 0]])


def visible(rel, vel, half, angle, R, P):
    """the CPU reference of the cull: does the drawn quad (cell_precip_gpu.geom) overlap the view?"""
    v = rel @ R.T
    vv = vel @ R.T
    depth = -v[:, 2]
    ok = depth >= 1.0
    focal = P[1, 1] * 0.5 * H
    k = focal / np.maximum(depth, 1e-6)
    hd = np.maximum(half, MINPX / k)
    tr = vv[:, :2] * SHUTTER
    tl = np.linalg.norm(tr, axis=1)
    st = tl > 0.2 * hd
    ax = np.where(st[:, None], tr / np.maximum(tl, 1e-30)[:, None], np.stack([np.cos(angle), np.sin(angle)], 1))
    ay = np.stack([-ax[:, 1], ax[:, 0]], 1)
    hx = np.where(st, hd + 0.5 * tl, hd)
    hy = hd
    corners = []
    for qx, qy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        p = v.copy()
        p[:, :2] += ax * (qx * hx)[:, None] + ay * (qy * hy)[:, None]
        c = np.c_[p, np.ones(len(p))] @ P.T
        corners.append(c[:, :2] / c[:, 3:4])
    Q = np.stack(corners, 1)  # n x 4 x 2 (ndc), a parallelogram
    # separating axes: the view's x and y, the quad's two edge normals
    sep = (Q[:, :, 0].min(1) > 1) | (Q[:, :, 0].max(1) < -1) | (Q[:, :, 1].min(1) > 1) | (Q[:, :, 1].max(1) < -1)
    box = np.array([[-1, -1], [1, -1], [1, 1], [-1, 1]], dtype=np.float64)
    for e in (Q[:, 1] - Q[:, 0], Q[:, 3] - Q[:, 0]):
        nrm = np.stack([-e[:, 1], e[:, 0]], 1)
        pq = np.einsum('nkd,nd->nk', Q, nrm)
        pb = box @ nrm.T  # 4 x n
        sep |= (pq.min(1) > pb.max(0)) | (pq.max(1) < pb.min(0))
    return ok & ~sep


class Gl:
    def __init__(self, red):
        S.context()
        f = S.fn
        self.createShader = f('glCreateShader', GLuint, GLenum)
        self.shaderSource = f('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)
        self.compileShader = f('glCompileShader', None, GLuint)
        self.getShaderiv = f('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
        self.getShaderInfoLog = f('glGetShaderInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
        self.createProgram = f('glCreateProgram', GLuint)
        self.attach = f('glAttachShader', None, GLuint, GLuint)
        self.link = f('glLinkProgram', None, GLuint)
        self.getProgramiv = f('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
        self.getProgramInfoLog = f('glGetProgramInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
        self.tfVaryings = f('glTransformFeedbackVaryings', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), GLenum)
        self.useProgram = f('glUseProgram', None, GLuint)
        self.loc = f('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
        self.u1ui = f('glUniform1ui', None, GLint, GLuint)
        self.u1i = f('glUniform1i', None, GLint, GLint)
        self.u1f = f('glUniform1f', None, GLint, ctypes.c_float)
        self.u2f = f('glUniform2f', None, GLint, ctypes.c_float, ctypes.c_float)
        self.u4f = f('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
        self.u1fv = f('glUniform1fv', None, GLint, ctypes.c_int, FP)
        self.u1iv = f('glUniform1iv', None, GLint, ctypes.c_int, IP)
        self.um4 = f('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, FP)
        self.genVA = f('glGenVertexArrays', None, ctypes.c_int, ctypes.POINTER(GLuint))
        self.bindVA = f('glBindVertexArray', None, GLuint)
        self.genBuf = f('glGenBuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
        self.bindBuf = f('glBindBuffer', None, GLenum, GLuint)
        self.bindBufBase = f('glBindBufferBase', None, GLenum, GLuint, GLuint)
        self.bufData = f('glBufferData', None, GLenum, ctypes.c_ssize_t, ctypes.c_void_p, GLenum)
        self.getBufSub = f('glGetBufferSubData', None, GLenum, ctypes.c_ssize_t, ctypes.c_ssize_t, ctypes.c_void_p)
        self.beginTF = f('glBeginTransformFeedback', None, GLenum)
        self.endTF = f('glEndTransformFeedback', None)
        S.gl.glEnable.argtypes = [GLenum]
        S.gl.glDrawArrays.argtypes = [GLenum, GLint, ctypes.c_int]
        S.gl.glGetError.restype = GLenum
        defines = ['PC_TEST'] + (['PC_FIELD_RED'] if red == 'field' else []) + (['PC_CULL_RED'] if red == 'cull' else [])
        src = S.inject(S.expand(os.path.join(TREE, 'res', 'shaders', 'cell_precip_gpu.vert')), defines)
        sh = self.createShader(0x8B31)
        b = ctypes.c_char_p(src.encode('latin-1'))
        self.shaderSource(sh, 1, ctypes.byref(b), None)
        self.compileShader(sh)
        ok = GLint(); self.getShaderiv(sh, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            log = ctypes.create_string_buffer(1 << 16); self.getShaderInfoLog(sh, 1 << 16, None, log)
            raise SystemExit('COMPILE FAIL ' + log.value.decode(errors='replace')[:1500])
        self.p = self.createProgram()
        self.attach(self.p, sh)
        names = (ctypes.c_char_p * len(VARY))(*VARY)
        self.tfVaryings(self.p, len(VARY), names, 0x8C8C)  # GL_INTERLEAVED_ATTRIBS
        self.link(self.p)
        self.getProgramiv(self.p, 0x8B82, ctypes.byref(ok))
        if not ok.value:
            log = ctypes.create_string_buffer(1 << 16); self.getProgramInfoLog(self.p, 1 << 16, None, log)
            raise SystemExit('LINK FAIL ' + log.value.decode(errors='replace')[:1500])
        va = GLuint(); self.genVA(1, ctypes.byref(va)); self.bindVA(va)
        self.buf = GLuint(); self.genBuf(1, ctypes.byref(self.buf))

    def L(self, n):
        return self.loc(self.p, n.encode())

    def run(self, g, R, P):
        self.useProgram(self.p)
        self.u1ui(self.L('pcSeedS'), g['seedS'])
        self.u1f(self.L('pcL'), g['L']); self.u1f(self.L('pcInner'), g['inner']); self.u1f(self.L('pcThin'), g['thin'])
        self.u2f(self.L('pcOffXY'), *g['offXY'])
        keep = {}
        for name in ('offZ', 'fallU', 'halfU'):
            keep[name] = np.ascontiguousarray(g[name], np.float32)
            self.u1fv(self.L('pc' + name[0].upper() + name[1:]), 16, keep[name].ctypes.data_as(FP))
        keep['cand'] = np.ascontiguousarray(g['cand'], np.int32)
        self.u1iv(self.L('pcCand'), 16, keep['cand'].ctypes.data_as(IP))
        self.u1i(self.L('pcNCand'), g['nCand']); self.u1i(self.L('pcBest'), g['best'])
        self.u2f(self.L('pcWind'), *g['wind'])
        self.u1f(self.L('pcSway'), g['sway']); self.u1f(self.L('pcSpin'), g['spin']); self.u1f(self.L('pcT'), g['t'])
        self.u1i(self.L('pcCols'), g['cols']); self.u1i(self.L('pcCells'), g['cells'])
        rows = max(1, g['cells'] // max(1, g['cols']))
        self.u2f(self.L('pcCellUv'), 1.0 / g['cols'], 1.0 / rows)
        for k in range(3):
            self.u4f(self.loc(self.p, ('pcRow[%d]' % k).encode()), *[float(x) for x in R[k]], 0.0)
        keep['P'] = np.ascontiguousarray(P, np.float32)
        self.um4(self.L('pcProj'), 1, 1, keep['P'].ctypes.data_as(FP))
        self.u1f(self.L('pcFocal'), float(P[1, 1]) * 0.5 * H)
        self.u1f(self.L('pcMinPx'), MINPX); self.u1f(self.L('pcShutter'), SHUTTER)
        n = g['count']
        nbytes = n * NF * 4
        self.bindBuf(0x8C8E, self.buf)  # GL_TRANSFORM_FEEDBACK_BUFFER
        self.bufData(0x8C8E, nbytes, None, 0x88E5)
        self.bindBufBase(0x8C8E, 0, self.buf)
        S.gl.glEnable(0x8C89)  # GL_RASTERIZER_DISCARD
        self.beginTF(0x0000)
        S.gl.glDrawArrays(0x0000, 0, n)
        self.endTF()
        out = np.empty(n * NF, np.float32)
        self.getBufSub(0x8C8E, 0, nbytes, out.ctypes.data)
        err = S.gl.glGetError()
        if err:
            raise SystemExit('GL error 0x%x' % err)
        return out.reshape(n, NF).astype(np.float64)


def main():
    a = sys.argv[1:]
    exe = a[0]
    red = a[a.index('--red') + 1] if '--red' in a else None
    verbose = '-v' in a
    gl = Gl(red)
    P = proj()
    fails = 0
    kept_all = vis_all = tot_all = 0
    for ci, c in enumerate(CASES):
        g, ref = dump(exe, c)
        g['t'] = c[5]
        R = view_rot(0.7 + 1.3 * ci, -0.15 + 0.07 * ci)
        o = gl.run(g, R, P)
        L = g['L']
        relG, velG, halfG, angG = o[:, 0:3], o[:, 6:9], o[:, 9], o[:, 10]
        cellG, cullG = o[:, 11:13], o[:, 13]
        inside = ref[:, 1] > 0.5
        relC, velC, halfC, angC, varC = ref[:, 2:5], ref[:, 5:8], ref[:, 8], ref[:, 9], ref[:, 10].astype(int)
        d = np.abs(relG - relC)
        d = np.minimum(d, np.abs(L - d))
        seam = int(((np.abs(relG - relC) > 0.5 * L).any(1)).sum())
        dv = np.abs(velG - velC @ R.T).max(1) / np.maximum(np.abs(velC).max(1), 1.0)	# vsVel is in view space
        dh = np.abs(halfG - halfC) / np.maximum(halfC, 1e-9)
        da = np.abs((angG - angC + math.pi) % (2 * math.pi) - math.pi)
        cell = varC % g['cells']
        cellC = np.stack([(cell % g['cols']) / g['cols'], (cell // g['cols']) / max(1, g['cells'] // g['cols'])], 1)
        dc = np.abs(cellG - cellC).max(1)
        # the GPU's inside test: culled with keep false only; a kept, visible particle must not be inside
        okF = (d.max() <= 0.05 and dv.max() <= 1e-3 and dh.max() <= 1e-5 and da.max() <= 0.01 and dc.max() <= 1e-6)
        vis = visible(relC, velC, halfC * 1.0, angC, R, P) & ~inside
        lost = int((vis & (cullG > 0.5)).sum())
        keptInside = int((inside & (cullG < 0.5)).sum())
        okC = lost == 0 and keptInside == 0
        kept = int((cullG < 0.5).sum())
        kept_all += kept; vis_all += int(vis.sum()); tot_all += len(ref)
        print('%-20s n %6d L %7.1f | F pos %.4f vel %.2e half %.1e ang %.4f cell %.1e seam %d %s | C visible %6d kept %6d '
              'lost %d inside-kept %d %s' % (c[0], len(ref), L, d.max(), dv.max(), dh.max(), da.max(), dc.max(), seam,
                                            'ok' if okF else 'FAIL', int(vis.sum()), kept, lost, keptInside,
                                            'ok' if okC else 'FAIL'))
        if verbose and not okF:
            i = int(np.argmax(d.max(1)))
            print('   worst', i, relG[i], relC[i], 'var', varC[i])
        fails += (not okF) + (not okC)
    print('kept share %.3f (visible %.3f) of %d particles: the rest is never expanded nor lit' %
          (kept_all / tot_all, vis_all / tot_all, tot_all))
    print('PRECIPGPU-CHECK', 'PASS' if fails == 0 else 'FAIL', '(%d gate fails, red %s)' % (fails, red))
    return 0 if fails == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
