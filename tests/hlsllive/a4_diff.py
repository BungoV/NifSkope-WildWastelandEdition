"""A4 differential gate (HLSLBULK1 R5): render every program from its GLSL and from its SPIR-V in the hidden 4.1 core context with
identical uniforms, textures, UBO data and vertices, and compare the readbacks.
  python -I -X utf8 tests/hlsllive/a4_diff.py <tree> <spv dir> [--only substr] [--plant] [--apply] [--seeds N]
Verdict per program: EXACT (every byte equal) | BOUND (equal NaN mask, max |diff|/max(1,|ref|) <= 1e-4: the GLSL compiler and the
DXC path contract multiply-adds differently) | FAIL (anything else) | NOCOV (the GLSL render wrote no pixel over all seeds: nothing
was proven) | SKIP (a program did not build here). --plant adds 0.37 to one float uniform in the SPIR-V run only: the gate must
turn at least one program FAIL (the red). --apply rewrites <spv dir>/spirv/programs.txt: FAIL programs become `FAIL a4 ...` (the loader
then keeps GLSL for them); the old file is kept as programs_prea4.txt. Writes <spv dir>/spirv/a4.txt.
HLSLFIX2 shards: `--shard i/N` renders only the programs whose crc32(name) % N == i (one hidden context per process) and writes
a4.shard<i>of<N>.txt + a4cache.shard<i>of<N>.txt (no a4.txt, no --apply); `--merge N` (no GL) joins the N parts in the serial
order into a4.txt / a4cache.txt and then does --apply, so the files equal a serial run's byte for byte (tests/hlsllive/a4_shards.sh)."""
import ctypes, json, os, sys, zlib
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from glctx import *
import remap, spirv_probe
import a2_iface
import stamps

c_f, c_i, c_u, c_p, c_c = ctypes.c_float, ctypes.c_int, ctypes.c_uint, ctypes.c_void_p, ctypes.c_char_p
SZ = 32
GLF = {}


def g(name, res, *args):
    return fn(name, res, *args)


FLOAT_T = {0x1406: 1, 0x8B50: 2, 0x8B51: 3, 0x8B52: 4}
INT_T = {0x1404: 1, 0x8B53: 2, 0x8B54: 3, 0x8B55: 4}
UINT_T = {0x1405: 1, 0x8DC6: 2, 0x8DC7: 3, 0x8DC8: 4}
BOOL_T = {0x8B56: 1, 0x8B57: 2, 0x8B58: 3, 0x8B59: 4}
MAT_T = {0x8B5A: (2, 2), 0x8B5B: (3, 3), 0x8B5C: (4, 4), 0x8B65: (2, 3), 0x8B66: (2, 4), 0x8B67: (3, 2), 0x8B68: (3, 4), 0x8B69: (4, 2), 0x8B6A: (4, 3)}
# sampler type -> (texture target, internal format, format, type)
SAMP = {0x8B5E: (0x0DE1, 0x8814, 0x1908, 0x1406), 0x8B5F: (0x806F, 0x8814, 0x1908, 0x1406), 0x8B60: (0x8513, 0x8814, 0x1908, 0x1406),
        0x8B5D: (0x0DE0, 0x8814, 0x1908, 0x1406), 0x8DC1: (0x8C1A, 0x8814, 0x1908, 0x1406), 0x8DC0: (0x8C18, 0x8814, 0x1908, 0x1406),
        0x8B62: (0x0DE1, 0x8CAC, 0x1902, 0x1406), 0x8DC4: (0x8C1A, 0x8CAC, 0x1902, 0x1406), 0x8DC5: (0x8513, 0x8CAC, 0x1902, 0x1406),
        0x8B61: (0x0DE0, 0x8CAC, 0x1902, 0x1406), 0x8DC3: (0x8C18, 0x8CAC, 0x1902, 0x1406),
        0x8DCA: (0x0DE1, 0x8D70, 0x8D99, 0x1405), 0x8DD2: (0x0DE1, 0x8D70, 0x8D99, 0x1405),
        0x8DCB: (0x806F, 0x8D70, 0x8D99, 0x1405), 0x8DD3: (0x806F, 0x8D70, 0x8D99, 0x1405),
        0x8B63: (0x84F5, 0x8814, 0x1908, 0x1406)}
SAMP[0x8DCA] = (0x0DE1, 0x8D82, 0x8D99, 0x1404)
# HLSLFIX2: buffer textures (cell_givol, cell_ddgiprobe, impostor_inst, the light lists): before, they were left unbound (read 0)
SAMP.update({0x8DC2: (0x8C2A, 0x8814, 0x1908, 0x1406), 0x8DD0: (0x8C2A, 0x8D82, 0x8D99, 0x1404), 0x8DD8: (0x8C2A, 0x8D70, 0x8D99, 0x1405),
             0x9108: (0x9100, 0x8814, 0x1908, 0x1406), 0x9109: (0x9100, 0x8D82, 0x8D99, 0x1404), 0x910A: (0x9100, 0x8D70, 0x8D99, 0x1405)})
SAMP[0x8DCB] = (0x806F, 0x8D82, 0x8D99, 0x1404)
SAMP[0x8DD2] = (0x0DE1, 0x8D70, 0x8D99, 0x1405)
SAMP[0x8DD3] = (0x806F, 0x8D70, 0x8D99, 0x1405)
DIM = {0x0DE0: (16, 1, 1), 0x0DE1: (16, 16, 1), 0x806F: (8, 8, 8), 0x8513: (16, 16, 6), 0x8C1A: (16, 16, 4), 0x8C18: (16, 1, 4), 0x84F5: (16, 16, 1), 0x8C2A: (256, 1, 1), 0x9100: (32, 32, 1)}


def rng_for(name, seed):
    return np.random.default_rng(zlib.crc32(('%s|%d' % (name, seed)).encode()))


def cstr_buf(n=512):
    return ctypes.create_string_buffer(n)


def attrib_cols(attribs):
    '''R6: a matrix attribute takes one location per column (the queue's per-draw modelViewMatrix at 8-11): one vector attribute each,
    named `<name>#<column>`; the harness feeds every column, not only the first'''
    out = []
    for name, loc, ty in attribs:
        if ty in MAT_T:
            c, rr = MAT_T[ty]
            out += [('%s#%d' % (name, k), loc + k, {2: 0x8B50, 3: 0x8B51, 4: 0x8B52}[rr]) for k in range(c)]
        else:
            out.append((name, loc, ty))
    return out


class Prog:
    def __init__(self, p):
        self.p = p
        gp = g('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
        v = GLint()
        self.unis, self.samps, self.attribs, self.blocks = [], [], [], []
        gp(p, 0x8B86, ctypes.byref(v))
        gau, gul = g('glGetActiveUniform', None, GLuint, GLuint, GLsizei, ctypes.POINTER(GLsizei), ctypes.POINTER(GLint), ctypes.POINTER(GLenum), c_c), g('glGetUniformLocation', GLint, GLuint, c_c)
        for i in range(v.value):
            ln, sz, ty, nm = GLsizei(), GLint(), GLenum(), cstr_buf()
            gau(p, i, 512, ctypes.byref(ln), ctypes.byref(sz), ctypes.byref(ty), nm)
            name = nm.value.decode()
            loc = gul(p, nm.value)
            if loc < 0:
                continue
            (self.samps if ty.value in SAMP or 'sampler' in str(ty.value) else self.unis).append((name, loc, ty.value, sz.value))
        self.samps = [s for s in self.samps if s[2] in SAMP]
        self.unis = [u for u in self.unis if u[2] not in SAMP]
        gp(p, 0x8B89, ctypes.byref(v))
        gaa, gal = g('glGetActiveAttrib', None, GLuint, GLuint, GLsizei, ctypes.POINTER(GLsizei), ctypes.POINTER(GLint), ctypes.POINTER(GLenum), c_c), g('glGetAttribLocation', GLint, GLuint, c_c)
        for i in range(v.value):
            ln, sz, ty, nm = GLsizei(), GLint(), GLenum(), cstr_buf()
            gaa(p, i, 512, ctypes.byref(ln), ctypes.byref(sz), ctypes.byref(ty), nm)
            loc = gal(p, nm.value)
            if loc >= 0:
                self.attribs.append((nm.value.decode(), loc, ty.value))
        gp(p, 0x8A36, ctypes.byref(v))
        gb, gbn, gbi = g('glGetActiveUniformBlockiv', None, GLuint, GLuint, GLenum, ctypes.POINTER(GLint)), g('glGetActiveUniformBlockName', None, GLuint, GLuint, GLsizei, ctypes.POINTER(GLsizei), c_c), None
        for i in range(v.value):
            ln, nm, sz, bd = GLsizei(), cstr_buf(), GLint(), GLint()
            gbn(p, i, 512, ctypes.byref(ln), nm)
            gb(p, i, 0x8A40, ctypes.byref(sz))      # DATA_SIZE
            gb(p, i, 0x8A3F, ctypes.byref(bd))      # BINDING
            self.blocks.append([nm.value.decode(), i, sz.value, bd.value])
        # R6: the colour attachments the fragment stage writes (index 0 outputs): a draw buffer no output feeds, or the second
        # output of a dual-source pair, holds an undefined value that differs between two drivers' compiles of the same shader
        self.outs = None
        try:
            gpi = g('glGetProgramInterfaceiv', None, GLuint, GLenum, GLenum, ctypes.POINTER(GLint))
            gpr = g('glGetProgramResourceiv', None, GLuint, GLenum, GLuint, GLsizei, c_p, GLsizei, c_p, c_p)
            gpi(p, 0x92E4, 0x92F5, ctypes.byref(v))
            outs = set()
            for i in range(v.value):
                props = (GLenum * 3)(0x930E, 0x930F, 0x92FB)   # LOCATION, LOCATION_INDEX, ARRAY_SIZE
                res = (GLint * 3)()
                gpr(p, 0x92E4, i, 3, ctypes.cast(props, c_p), 3, None, ctypes.cast(res, c_p))
                if res[1] <= 0 and res[0] >= 0:
                    outs.update(range(res[0], res[0] + max(res[2], 1)))
            self.outs = outs
        except Exception:
            self.outs = None
        # HLSLFIX3: shader storage blocks (name, binding, data size); the bindless twins read their per-draw rows from binding 12
        self.ssbos = []
        try:
            gpi(p, 0x92E6, 0x92F5, ctypes.byref(v))
            for i in range(v.value):
                nm = cstr_buf()
                g('glGetProgramResourceName', None, GLuint, GLenum, GLuint, GLsizei, c_p, c_c)(p, 0x92E6, i, 512, None, nm)
                props = (GLenum * 2)(0x9302, 0x9303)   # BUFFER_BINDING, BUFFER_DATA_SIZE
                res = (GLint * 2)()
                gpr(p, 0x92E6, i, 2, ctypes.cast(props, c_p), 2, None, ctypes.cast(res, c_p))
                self.ssbos.append((nm.value.decode(), res[0], res[1]))
        except Exception:
            self.ssbos = []
        self.members = {}
        for bname, bi, _sz, _bd in self.blocks:
            cnt = GLint()
            g('glGetActiveUniformBlockiv', None, GLuint, GLuint, GLenum, ctypes.POINTER(GLint))(p, bi, 0x8A42, ctypes.byref(cnt))
            idx = (GLuint * max(cnt.value, 1))()
            g('glGetActiveUniformBlockiv', None, GLuint, GLuint, GLenum, c_p)(p, bi, 0x8A43, ctypes.cast(idx, c_p))
            info = {}
            for tok, key in ((0x8A37, 'type'), (0x8A38, 'size'), (0x8A3B, 'off'), (0x8A3C, 'astride'), (0x8A3D, 'mstride'), (0x8A3E, 'rowmajor')):
                arr = (GLint * max(cnt.value, 1))()
                g('glGetActiveUniformsiv', None, GLuint, GLsizei, c_p, GLenum, c_p)(p, cnt.value, ctypes.cast(idx, c_p), tok, ctypes.cast(arr, c_p))
                info[key] = list(arr)
            self.members[bname] = [tuple(info[k][j] for k in ('type', 'size', 'off', 'astride', 'mstride', 'rowmajor')) for j in range(cnt.value)]

    def has(self, tok):
        v = GLint(); g('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))(self.p, tok, ctypes.byref(v)); return v.value


def int_uniform(ty, name):
    return ty in INT_T or ty in UINT_T or ty in BOOL_T


MODE = ''   # R6 coverage retry: '' random ints and bools | 'zero' all 0 | 'one' all 1 (a state the random draw never reaches)
# HLSLFIX2: res/shaders/hlsl/a4fixtures.txt, `<program>\t<name=value,...>` (the A4_FORCE form): uniforms pinned in BOTH runs for a
# program that random inputs never cover (a range test on a few ints, a depth-only output). <program> is the .prog name; a row
# for the base name covers its define variants too. The row is part of the program's cache key.
FIXTURE = ''


def fixtures(tree):
    out = {}
    p = os.path.join(tree, 'res/shaders/hlsl/a4fixtures.txt')
    if os.path.exists(p):
        for ln in open(p, encoding='utf-8').read().splitlines():
            if ln.strip() and not ln.startswith('#') and '\t' in ln:
                k, v = ln.split('\t', 1)
                out[k.strip()] = v.strip()
    return out


def set_uniform(prog, name, loc, ty, size, seed, plant):
    r = rng_for(name, seed)
    force = dict(kv.split('=') for kv in FIXTURE.split(',') if kv)   # HLSLFIX2: the program's a4fixtures.txt row, same form as A4_FORCE
    force.update(kv.split('=') for kv in os.environ.get('A4_FORCE', '').split(',') if kv)   # debug: A4_FORCE=useFalloff=0,pfxGame=1
    if name.endswith('[0]') and name[:-3] in force:   # an array is named `x[0]` by GL; a force names it `x`
        force[name] = force[name[:-3]]
    if name in force and ty in MAT_T and len(force[name].split(':')) == MAT_T[ty][0] * MAT_T[ty][1] * size:
        # a matrix (array), column-major, every component ':'-joined
        c, rr = MAT_T[ty]
        v = np.array([float(x) for x in force[name].split(':')], dtype=np.float32)
        nm = 'glUniformMatrix%d' % c if c == rr else 'glUniformMatrix%dx%d' % (c, rr)
        g(nm + 'fv', None, GLint, GLsizei, ctypes.c_ubyte, c_p)(loc, size, 0, v.ctypes.data)
        return True
    if name in force and ty in FLOAT_T and (size == 1 or len(force[name].split(':')) == FLOAT_T[ty] * size) and (':' in force[name] or FLOAT_T[ty] == 1):
        # A4_FORCE=falloffParams=0.1:0.5:0.7:0.8; HLSLFIX2: a scalar float, and a whole array (every component, ':'-joined)
        v = np.array([float(x) for x in force[name].split(':')], dtype=np.float32)
        g('glUniform%dfv' % FLOAT_T[ty], None, GLint, GLsizei, c_p)(loc, max(len(v) // FLOAT_T[ty], 1), v.ctypes.data)
        return True
    if name in force and ty in (INT_T.keys() | BOOL_T.keys()) and len(force[name].split(':')) == (INT_T.get(ty) or BOOL_T[ty]) * size:
        # an int / bool (HLSLFIX2: and an ivec, ':'-joined)
        n = INT_T.get(ty) or BOOL_T[ty]
        v = np.array([int(x) for x in force[name].split(':')], dtype=np.int32)
        g('glUniform%div' % n, None, GLint, GLsizei, c_p)(loc, size, v.ctypes.data)
        return True
    if ty in FLOAT_T:
        n = FLOAT_T[ty]
        a = (r.random(n * size) * 0.8 + 0.1).astype(np.float32)
        if 'falloff' in name.lower() and n >= 2:   # smoothstep(edge0, edge1, x) is undefined for edge0 >= edge1: feed ascending edges
            a = np.sort(a.reshape(size, n), axis=1).ravel()
        if plant:
            a += np.float32(0.37)
        f = g('glUniform%dfv' % n, None, GLint, GLsizei, c_p)
        f(loc, size, a.ctypes.data)
    elif ty in INT_T or ty in BOOL_T:
        n = INT_T.get(ty) or BOOL_T[ty]
        a = r.integers(0, 2 if ty in BOOL_T else 3, n * size).astype(np.int32)   # a bool is 0 or 1 as the app sets it
        if MODE and ty in INT_T | BOOL_T and ty != 0x8B55:
            a[:] = 1 if MODE == 'one' else 0
        if ty == 0x8B55:
            a[:] = np.tile(np.array([0, 0, SZ, SZ], dtype=np.int32), size)
        g('glUniform%div' % n, None, GLint, GLsizei, c_p)(loc, size, a.ctypes.data)
    elif ty in UINT_T:
        n = UINT_T[ty]
        a = r.integers(0, 3, n * size).astype(np.uint32)
        g('glUniform%duiv' % n, None, GLint, GLsizei, c_p)(loc, size, a.ctypes.data)
    elif ty in MAT_T:
        c, rr = MAT_T[ty]
        a = (r.random(c * rr * size) * 0.8 + 0.1).astype(np.float32)
        if c == rr:   # identity plus a small seeded skew: random dense matrices throw every vertex off screen
            a = np.tile(np.eye(c, dtype=np.float32).ravel(), size) + (a - 0.5) * np.float32(0.05)
        nm = 'glUniformMatrix%d' % c if c == rr else 'glUniformMatrix%dx%d' % (c, rr)
        g(nm + 'fv', None, GLint, GLsizei, ctypes.c_ubyte, c_p)(loc, size, 0, a.ctypes.data)
    else:
        return False
    return True


def make_texture(ty, seed):
    tgt, ifmt, fmt, dt = SAMP[ty]
    t = GLuint()
    g('glGenTextures', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(t))
    g('glBindTexture', None, GLenum, GLuint)(tgt, t)
    w, h, d = DIM[tgt]
    r = np.random.default_rng(1000 + seed + ty)
    nc = 1 if fmt == 0x1902 else 4
    if tgt == 0x8C2A:   # HLSLFIX2: a buffer texture (samplerBuffer): RGBA32F / RGBA32I / RGBA32UI texels in a buffer object
        data = ((r.random(w * 4) * 0.9 + 0.05).astype(np.float32) if dt == 0x1406 else r.integers(0, 3, w * 4).astype(np.int32 if dt == 0x1404 else np.uint32))
        b = GLuint(); g('glGenBuffers', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(b))
        g('glBindBuffer', None, GLenum, GLuint)(0x8C2A, b.value)
        g('glBufferData', None, GLenum, ctypes.c_ssize_t, c_p, GLenum)(0x8C2A, data.nbytes, data.ctypes.data, 0x88E4)
        g('glTexBuffer', None, GLenum, GLenum, GLuint)(0x8C2A, ifmt, b.value)
        return t.value, tgt
    if tgt == 0x9100:   # HLSLFIX2: a multisample texture (sampler2DMS; before: unbound, so it shared unit 0 with a sampler2D and
        # the draw was refused: cell_underwater_ms NOCOV). 4 samples, cleared to one seeded value (no upload path for MS texels)
        g('glTexImage2DMultisample', None, GLenum, GLsizei, GLenum, GLsizei, GLsizei, ctypes.c_ubyte)(tgt, 4, ifmt, w, h, 1)
        prev = GLint(); gl.glGetIntegerv(0x8CA6, ctypes.byref(prev))
        f = GLuint(); g('glGenFramebuffers', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(f))
        g('glBindFramebuffer', None, GLenum, GLuint)(0x8D40, f.value)
        g('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)(0x8D40, 0x8CE0, tgt, t.value, 0)
        g('glDrawBuffers', None, GLsizei, c_p)(1, ctypes.cast((GLenum * 1)(0x8CE0), c_p))
        v = float(r.random() * 0.9 + 0.05)
        if dt == 0x1406:
            g('glClearBufferfv', None, GLenum, GLint, c_p)(0x1800, 0, ctypes.cast((c_f * 4)(v, v, v, v), c_p))
        else:
            k = int(v * 3)
            g('glClearBuffer%sv' % ('i' if dt == 0x1404 else 'ui'), None, GLenum, GLint, c_p)(0x1800, 0, ctypes.cast((ctypes.c_int * 4)(k, k, k, k), c_p))
        g('glBindFramebuffer', None, GLenum, GLuint)(0x8D40, prev.value)
        return t.value, tgt
    if dt == 0x1406:
        data = (r.random(w * h * d * nc) * 0.9 + 0.05).astype(np.float32)
    elif dt == 0x1404:
        data = r.integers(0, 3, w * h * d * nc).astype(np.int32)
    else:
        data = r.integers(0, 3, w * h * d * nc).astype(np.uint32)
    tex_img = g('glTexImage2D', None, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, c_p)
    tex_img3 = g('glTexImage3D', None, GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, c_p)
    tex_img1 = g('glTexImage1D', None, GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, c_p)
    if tgt == 0x8513:
        for f in range(6):
            tex_img(0x8515 + f, 0, ifmt, w, h, 0, fmt, dt, data.ctypes.data + f * w * h * nc * 4)
    elif tgt in (0x806F, 0x8C1A):
        tex_img3(tgt, 0, ifmt, w, h, d, 0, fmt, dt, data.ctypes.data)
    elif tgt in (0x0DE0,):
        tex_img1(tgt, 0, ifmt, w, 0, fmt, dt, data.ctypes.data)
    elif tgt == 0x8C18:
        tex_img(tgt, 0, ifmt, w, d, 0, fmt, dt, data.ctypes.data)
    else:
        tex_img(tgt, 0, ifmt, w, h, 0, fmt, dt, data.ctypes.data)
    tp = g('glTexParameteri', None, GLenum, GLenum, GLint)
    filt = 0x2600 if dt != 0x1406 or fmt == 0x1902 else 0x2601   # float colour textures filter; the rest nearest
    if ifmt == 0x8814:
        filt = 0x2600   # RGBA32F: linear filtering of 32F is implementation business, keep nearest
    tp(tgt, 0x2801, filt); tp(tgt, 0x2800, filt)
    tp(tgt, 0x2802, 0x812F); tp(tgt, 0x2803, 0x812F)
    if tgt in (0x806F, 0x8513):
        tp(tgt, 0x8072, 0x812F)
    if fmt == 0x1902:
        tp(tgt, 0x884C, 0x884E); tp(tgt, 0x884D, 0x0203)   # compare ref to texture, LEQUAL
    return t.value, tgt


class Rig:
    def __init__(self):
        gv = g('glGenVertexArrays', None, GLsizei, ctypes.POINTER(GLuint))
        self.vao = GLuint(); gv(1, ctypes.byref(self.vao))
        self.fbo = GLuint(); g('glGenFramebuffers', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(self.fbo))
        g('glBindFramebuffer', None, GLenum, GLuint)(0x8D40, self.fbo.value)
        self.cols = []
        for i in range(4):
            t = GLuint(); g('glGenTextures', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(t))
            g('glBindTexture', None, GLenum, GLuint)(0x0DE1, t.value)
            g('glTexImage2D', None, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, c_p)(0x0DE1, 0, 0x8814, SZ, SZ, 0, 0x1908, 0x1406, None)
            g('glTexParameteri', None, GLenum, GLenum, GLint)(0x0DE1, 0x2801, 0x2600)
            g('glTexParameteri', None, GLenum, GLenum, GLint)(0x0DE1, 0x2800, 0x2600)
            g('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)(0x8D40, 0x8CE0 + i, 0x0DE1, t.value, 0)
            self.cols.append(t.value)
        d = GLuint(); g('glGenRenderbuffers', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(d))
        g('glBindRenderbuffer', None, GLenum, GLuint)(0x8D41, d.value)
        g('glRenderbufferStorage', None, GLenum, GLenum, GLsizei, GLsizei)(0x8D41, 0x81A6, SZ, SZ)
        g('glFramebufferRenderbuffer', None, GLenum, GLenum, GLenum, GLuint)(0x8D40, 0x8D00, 0x8D41, d.value)
        bufs = (GLenum * 4)(0x8CE0, 0x8CE1, 0x8CE2, 0x8CE3)
        g('glDrawBuffers', None, GLsizei, c_p)(4, ctypes.cast(bufs, c_p))
        self.ubo = {}
        self.tex = {}
        self.rows = {}

    def ubo_for(self, name, size, seed, members=()):
        # HLSLFIX2: the member layout is in the key; without it the first program to use a block name decided the int lanes for
        # every later one, so a verdict hung on render order (cell_volair AIRZONE+BOUNCE rows: EXACT serial, FAIL sharded)
        members = tuple(sorted(members))
        k = (name, size, seed, MODE, members)
        if k not in self.ubo:
            b = GLuint(); g('glGenBuffers', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(b))
            a = (rng_for(name, seed).random(max(size, 16) // 4 + 4) * 0.8 + 0.1).astype(np.float32)
            iv = a.view(np.int32)
            for ty, sz, off, ast, mst, rowm in members:
                if off < 0:
                    continue
                for e in range(max(sz, 1)):
                    base = (off + e * max(ast, 0)) // 4
                    if ty in INT_T or ty in UINT_T or ty in BOOL_T:
                        n = INT_T.get(ty) or UINT_T.get(ty) or BOOL_T[ty]
                        for q in range(n):
                            iv[base + q] = (1 if MODE == 'one' else 0) if MODE else int(rng_for('%s%d%d' % (name, off, q), seed).integers(0, 2 if ty in BOOL_T else 3))
                        if ty == 0x8B55:   # an ivec4 is a viewport rectangle often: x y w h
                            iv[base:base + 4] = (0, 0, SZ, SZ)
                    elif ty in MAT_T and MAT_T[ty][0] == MAT_T[ty][1] and mst > 0:   # a square matrix: identity plus a small skew
                        c = MAT_T[ty][0]
                        for col in range(c):
                            for row in range(c):
                                a[base + col * (mst // 4) + row] = (1.0 if col == row else 0.0) + (a[base + col * (mst // 4) + row] - 0.5) * 0.05
            g('glBindBuffer', None, GLenum, GLuint)(0x8A11, b.value)
            g('glBufferData', None, GLenum, ctypes.c_ssize_t, c_p, GLenum)(0x8A11, a.nbytes, a.ctypes.data, 0x88E4)
            self.ubo[k] = b.value
        return self.ubo[k]

    def rows_for(self, seed):
        """HLSLFIX3: the bindless row buffer (ww_bindless.glsl WwDraw, std430 176 B: mat4 mv, vec4 nm[3], uvec2 tex[8]). ROWS rows
        cover wwDrawBase 0..2 plus the base instance 0 of glDrawArrays. Each row's eight maps are their own RGBA32F textures (a slot
        or row mix-up reads different texels), made resident as GL_ARB_bindless_texture handles; mv and nm are the identity plus a
        small skew, like the matrix uniforms."""
        if seed not in self.rows:
            ROWS = 4
            gh = g('glGetTextureHandleARB', ctypes.c_uint64, GLuint)
            mk = g('glMakeTextureHandleResidentARB', None, ctypes.c_uint64)
            a = np.zeros(ROWS * 44, dtype=np.uint32)
            f = a.view(np.float32)
            r = rng_for('wwDraws', seed)
            for row in range(ROWS):
                base = row * 44
                sk = (r.random(16) - 0.5) * 0.05
                for c in range(4):
                    for k in range(4):
                        f[base + c * 4 + k] = (1.0 if c == k else 0.0) + sk[c * 4 + k]
                sk = (r.random(12) - 0.5) * 0.05
                for c in range(3):
                    for k in range(4):
                        f[base + 16 + c * 4 + k] = ((1.0 if c == k else 0.0) + sk[c * 4 + k]) if k < 3 else 0.0
                for slot in range(8):
                    g('glActiveTexture', None, GLenum)(0x84C0 + 79)
                    t, _tgt = make_texture(0x8B5E, seed * 64 + row * 8 + slot + 7)
                    h = gh(t)
                    mk(h)
                    a[base + 28 + slot * 2] = h & 0xFFFFFFFF
                    a[base + 28 + slot * 2 + 1] = h >> 32
            b = GLuint(); g('glGenBuffers', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(b))
            g('glBindBuffer', None, GLenum, GLuint)(0x90D2, b.value)
            g('glBufferData', None, GLenum, ctypes.c_ssize_t, c_p, GLenum)(0x90D2, a.nbytes, a.ctypes.data, 0x88E4)
            self.rows[seed] = b.value
        return self.rows[seed]

    def tex_for(self, ty, seed):
        if (ty, seed) not in self.tex:
            # HLSLFIX2: made on a spare unit. make_texture binds the new texture on the ACTIVE unit, which was the previous
            # sampler's: on the first use of a (type, seed) that sampler read the wrong texture in the GLSL run only, so a
            # verdict hung on which program came first (cell_hdr: FAIL 0.869 / 0.412 / 0.131 shifting with the order)
            g('glActiveTexture', None, GLenum)(0x84C0 + 79)   # the last unit GL 4.x guarantees; no program binds 80 samplers
            self.tex[(ty, seed)] = make_texture(ty, seed)
        return self.tex[(ty, seed)]

    def run(self, pr, ref, seed, plant, prim, nverts, patch_n):
        """render program pr; ref = the GLSL Prog (its attribute / uniform list decides the inputs, so both runs get the same)"""
        up = g('glUseProgram', None, GLuint)
        up(pr.p)
        g('glBindFramebuffer', None, GLenum, GLuint)(0x8D40, self.fbo.value)
        clr = (c_f * 4)(0.25, 0.25, 0.25, 0.25)
        for i in range(4):
            g('glClearBufferfv', None, GLenum, GLint, c_p)(0x1800, i, ctypes.cast(clr, c_p))
        g('glViewport', None, GLint, GLint, GLsizei, GLsizei)(0, 0, SZ, SZ)
        for cap in (0x0B44, 0x0BE2, 0x0C11):   # CULL_FACE, BLEND, SCISSOR_TEST
            g('glDisable', None, GLenum)(cap)
        # R6: the depth buffer is an output too (a depth-only program, gl_FragDepth): test ALWAYS, write on, cleared to 1.0
        g('glEnable', None, GLenum)(0x0B71)
        g('glDepthFunc', None, GLenum)(0x0207)
        g('glDepthMask', None, ctypes.c_ubyte)(1)
        dclr = (c_f * 1)(1.0)
        g('glClearBufferfv', None, GLenum, GLint, c_p)(0x1801, 0, ctypes.cast(dclr, c_p))
        g('glBindVertexArray', None, GLuint)(self.vao.value)
        # vertices (attribute by location from the GLSL program; the SPIR-V one carries the same locations)
        bl = []
        for name, loc, ty in attrib_cols(ref.attribs):
            n = FLOAT_T.get(ty) or INT_T.get(ty) or UINT_T.get(ty) or 4
            isint = ty in INT_T or ty in UINT_T
            r = rng_for('attr' + name, seed)
            if isint:
                a = r.integers(0, 3, nverts * n).astype(np.int32)
            else:
                a = (r.random(nverts * n) * 1.8 - 0.9).astype(np.float32)
                if '#' in name:   # a column of a matrix attribute: the identity's column plus a small skew
                    a = (r.random(nverts * n) * 0.05 - 0.025).astype(np.float32)
                    a[int(name.rsplit('#', 1)[1]) % n::n] += 1.0
                elif n == 4:
                    a[3::4] = 1.0
            b = GLuint(); g('glGenBuffers', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(b))
            g('glBindBuffer', None, GLenum, GLuint)(0x8892, b.value)
            g('glBufferData', None, GLenum, ctypes.c_ssize_t, c_p, GLenum)(0x8892, a.nbytes, a.ctypes.data, 0x88E4)
            g('glEnableVertexAttribArray', None, GLuint)(loc)
            if isint:
                g('glVertexAttribIPointer', None, GLuint, GLint, GLenum, GLsizei, c_p)(loc, n, 0x1404, 0, None)
            else:
                g('glVertexAttribPointer', None, GLuint, GLint, GLenum, ctypes.c_ubyte, GLsizei, c_p)(loc, n, 0x1406, 0, 0, None)
            bl.append((loc, b.value))
        # uniforms
        planted = False
        self.uerr = []
        while g('glGetError', GLenum)():
            pass
        for name, loc, ty, size in sorted(ref.unis):
            lp = g('glGetUniformLocation', GLint, GLuint, c_c)(pr.p, name.encode())
            if lp < 0:
                continue
            do_plant = plant and not planted and ty in FLOAT_T
            if do_plant:
                planted = True
            set_uniform(pr, name, lp, ty, size, seed, do_plant)
            if g('glGetError', GLenum)():
                self.uerr.append(name)
        unit = 0
        for name, loc, ty, size in sorted(ref.samps):
            lp = g('glGetUniformLocation', GLint, GLuint, c_c)(pr.p, name.encode())
            if lp < 0:
                continue
            units = []
            for k in range(size):
                t, tgt = self.tex_for(ty, seed)
                g('glActiveTexture', None, GLenum)(0x84C0 + unit)
                g('glBindTexture', None, GLenum, GLuint)(tgt, t)
                units.append(unit); unit += 1
            arr = np.array(units, dtype=np.int32)
            g('glUniform1iv', None, GLint, GLsizei, c_p)(lp, size, arr.ctypes.data)
        # blocks: one buffer per block name, bound at the program's own binding
        bu = g('glBindBufferBase', None, GLenum, GLuint, GLuint)
        for name, idx, size, bind in pr.blocks:
            ref_sz = next((s for n2, _i, s, _b in ref.blocks if n2 == name), size)
            bu(0x8A11, bind, self.ubo_for(name, ref_sz, seed, ref.members.get(name, ())))
        for name, bind, size in ref.ssbos:   # HLSLFIX3: at the GLSL program's binding, so a SPIR-V one elsewhere reads nothing
            if bind == 12:
                bu(0x90D2, bind, self.rows_for(seed))
        if patch_n:
            g('glPatchParameteri', None, GLenum, GLint)(0x8E72, patch_n)
        # transform: the SPIR-V program has no glGetError cleanup trouble; clear errors before the draw
        while g('glGetError', GLenum)():
            pass
        g('glDrawArrays', None, GLenum, GLint, GLsizei)(prim, 0, nverts)
        g('glFinish', None)()
        err = g('glGetError', GLenum)()
        outs = []
        for i in range(4):
            g('glReadBuffer', None, GLenum)(0x8CE0 + i)
            buf = np.zeros(SZ * SZ * 4, dtype=np.float32)
            g('glReadPixels', None, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, c_p)(0, 0, SZ, SZ, 0x1908, 0x1406, buf.ctypes.data)
            if ref.outs is not None and i not in ref.outs:
                buf[:] = 0.25
            outs.append(buf)
        dbuf = np.zeros(SZ * SZ, dtype=np.float32)
        g('glReadPixels', None, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, c_p)(0, 0, SZ, SZ, 0x1902, 0x1406, dbuf.ctypes.data)
        d4 = np.full(SZ * SZ * 4, 0.25, dtype=np.float32)   # a texel the draw left at the cleared 1.0 reads as the colour clear (not covered)
        d4[0::4] = np.where(dbuf == 1.0, 0.25, dbuf)
        outs.append(d4)
        for loc, b in bl:
            g('glDisableVertexAttribArray', None, GLuint)(loc)
            g('glDeleteBuffers', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(GLuint(b)))
        up(0)
        return np.concatenate(outs), err


def compare(a, b):
    """-> (verdict, detail)"""
    ia, ib = a.view(np.uint32), b.view(np.uint32)
    if np.array_equal(ia, ib):
        return 'EXACT', ''
    na, nb = np.isnan(a), np.isnan(b)
    if not np.array_equal(na, nb):
        return 'FAIL', 'NaN mask differs (%d vs %d)' % (na.sum(), nb.sum())
    m = ~na & np.isfinite(a) & np.isfinite(b)
    if not np.array_equal(np.isfinite(a) | na, np.isfinite(b) | nb):
        return 'FAIL', 'inf mask differs'
    d = np.abs(a[m].astype(np.float64) - b[m].astype(np.float64)) / np.maximum(1.0, np.abs(a[m].astype(np.float64)))
    mx = float(d.max()) if d.size else 0.0
    nd = int((ia != ib).sum())
    if mx <= 1e-4:
        return 'BOUND', 'max rel %.3g over %d of %d values' % (mx, nd, a.size)
    return 'FAIL', 'max rel %.3g over %d of %d values' % (mx, nd, a.size)


def summary(rows, plant):
    cnt = {}
    for _n, grp, v, _d in rows:
        cnt.setdefault(grp, {}).setdefault(v, 0)
        cnt[grp][v] += 1
    print('--- per group')
    for grp in sorted(cnt):
        print('%-22s %s' % (grp, ' '.join('%s=%d' % kv for kv in sorted(cnt[grp].items()))))
    allv = {}
    for r in rows:
        allv[r[2]] = allv.get(r[2], 0) + 1
    print('A4 %d programs: %s%s' % (len(rows), ' '.join('%s=%d' % kv for kv in sorted(allv.items())), '  (planted fault run)' if plant else ''))


def cache_text(cache):
    return ''.join('%s\t%s\t%s\t%s\t%s\n' % (n, v[0], v[1], v[2], v[3].replace('\t', ' ').replace('\n', ' ')) for n, v in sorted(cache.items()))


def finish(spvdir, rows, plant, apply_):
    out = os.path.join(spvdir, 'spirv', 'a4_plant.txt' if plant else 'a4.txt')
    open(out, 'w', newline='\n').write(''.join('%s\t%s\t%s\t%s\n' % r for r in rows))
    if apply_ and not plant:
        old = os.path.join(spvdir, 'spirv', 'programs.txt')
        os.replace(old, os.path.join(spvdir, 'spirv', 'programs_prea4.txt'))
        bad = {r[0]: (r[3] if r[2] == 'FAIL' else 'NOCOV unproven: the GLSL render covered no pixel') for r in rows if r[2] in ('FAIL', 'NOCOV')}
        lines = []
        for l in open(os.path.join(spvdir, 'spirv', 'programs_prea4.txt')).read().splitlines():
            n = l.split('\t')[0]
            lines.append('%s\tFAIL a4 %s' % (n, bad[n]) if n in bad else l)
        open(old, 'w', newline='\n').write('\n'.join(lines) + '\n')
        print('applied: %d programs moved to GLSL' % len(bad))


def merge(spvdir, n, plant, apply_):
    """HLSLFIX2: join the --shard parts (index-ordered rows, cache lines) into the serial run's files; every part must exist"""
    tag = 'a4_plant' if plant else 'a4'
    rows, cache = [], {}
    for i in range(n):
        pf = os.path.join(spvdir, 'spirv', '%s.shard%dof%d.txt' % (tag, i, n))
        for ln in open(pf, encoding='utf-8').read().splitlines():
            p, js = ln.split('\t', 1)
            rows.append((int(p), tuple(json.loads(js))))
        if not plant:
            cache.update(stamps.load_table(os.path.join(spvdir, 'spirv', 'a4cache.shard%dof%d.txt' % (i, n)), 5))
    rows = [r for _i, r in sorted(rows)]
    for name, group, v, d in rows:
        print('%-6s %-22s %-34s %s' % (v, group, name, d))
    summary(rows, plant)
    print('A4 merged %d shards' % n)
    if not plant:
        open(os.path.join(spvdir, 'spirv', 'a4cache.txt'), 'w', newline='\n').write(cache_text(cache))
    finish(spvdir, rows, plant, apply_)


def main():
    a = sys.argv[1:]
    tree, spvdir = a[0], a[1]
    only = a[a.index('--only') + 1] if '--only' in a else None
    plant, apply_ = '--plant' in a, '--apply' in a
    seeds = int(a[a.index('--seeds') + 1]) if '--seeds' in a else 3
    if '--merge' in a:
        return merge(spvdir, int(a[a.index('--merge') + 1]), plant, apply_)
    shard = tuple(int(x) for x in a[a.index('--shard') + 1].split('/')) if '--shard' in a else None
    glctx.context() if False else context()
    units = GLint(0); gl.glGetIntegerv(0x8872, ctypes.byref(units))
    units = min(max(units.value, 1), 32) - 2
    okp = dict(l.rstrip('\n').split('\t', 1) for l in open(os.path.join(spvdir, 'spirv', 'programs.txt')) if '\t' in l)
    rig = Rig()
    rows, tot = [], {}
    byn = remap._byname(tree)
    # R7: a verdict is cached against the program's SPIR-V bytes, the GLSL sources it is compared with (and their includes) and the
    # harness files; a program with no change since the last run is not rendered again (--fresh ignores the cache)
    cachef = os.path.join(spvdir, 'spirv', 'a4cache.txt')
    # the repo's seed (res/shaders/hlsl/a4cache.txt, verdicts of the machine that built it) first, this build folder's cache over it
    cache = {}
    if '--fresh' not in a and not plant:
        cache = stamps.load_table(os.path.join(tree, 'res/shaders/hlsl/a4cache.txt'), 5)
        cache.update(stamps.load_table(cachef, 5))
    tk = stamps.tools_key(['spvgl.py', 'build_spv.py'])
    # HLSLFIX2: the harness files' CODE (stamps.code_key: comments and docstrings dropped), not their bytes; a2_iface.py and
    # remap.py are read too (items, glsl_program, the source lookup); a code edit anywhere in them still re-renders every program
    atool = stamps.sha(stamps.code_key(['a4_diff.py', 'a2_iface.py', 'glctx.py', 'spirv_probe.py', 'remap.py', 'stamps.py']),
                       units, seeds, os.environ.get('A4_BASEONLY', ''))
    gfind = lambda n: os.path.join(tree, byn[n]) if n in byn else None
    newcache = dict(cache)
    fix = fixtures(tree)
    ridx = []   # each row's position in the serial order (a2_iface.items), for --merge
    for pos, (pf, st, vdefs, name) in enumerate(a2_iface.items(tree)):
        if shard and zlib.crc32(name.encode('utf-8')) % shard[1] != shard[0]:
            continue
        del ridx[len(rows):]
        ridx.append(pos)   # the row this program may append is rows[len(rows)]
        txt = open(os.path.join(tree, pf), encoding='utf-8', errors='replace').read()
        if '.geom' not in txt:
            st = [x for x in st if not x[1].endswith('_points_gs.hlsl')]
        if only and only not in name or any(e not in a2_iface.STG for e, _p, _s in st):
            continue
        if okp.get(name, '')[:2] != 'OK':
            continue
        if os.environ.get('A4_BASEONLY') and '@' in name:   # debug: the base programs only (a diagnosis loop, not a verdict)
            continue
        group = pf.replace('\\', '/').split('/')[3] if pf.replace('\\', '/').startswith('res/shaders/features/') else 'core'
        specs = [(a2_iface.STG[e], os.path.join(spvdir, 'spirv', os.path.basename(pth).rsplit('.', 1)[0] + '.spv')) for e, pth, _s in st]
        shaders = [w for ln in txt.splitlines() if ln.startswith('shaders') for w in ln.split()[1:]]
        global FIXTURE
        FIXTURE = fix.get(name, fix.get(name.split('@')[0], ''))
        ckey = stamps.sha(name, atool, ','.join(vdefs), *([FIXTURE] if FIXTURE else []), *([stamps.stage_key(pth_, [], tk) for _e, pth_, _s in st]
                                                         + [stamps.source_key(os.path.join(tree, byn.get(s_, 'res/shaders/' + s_)), gfind) for s_ in shaders]))
        hit = cache.get(name)
        if hit and hit[0] == ckey:
            rows.append((name, group, hit[2], hit[3]))
            print('%-6s %-22s %-34s %s' % (hit[2], group, name, ('(cached) ' + hit[3]).strip()), flush=True)
            continue
        pg = a2_iface.glsl_program(tree, [byn.get(s, 'res/shaders/' + s) for s in shaders], units, vdefs)
        ps, log = spirv_probe.build(specs)
        if pg is None or ps is None:
            rows.append((name, group, 'SKIP', 'did not build here'))
            continue
        G, S = Prog(pg), Prog(ps)
        # GLSL blocks take the loader's bindings; the SPIR-V ones carry theirs
        ub = g('glUniformBlockBinding', None, GLuint, GLuint, GLuint)
        fixed = {'globalUniforms': 0, 'skinningUniforms': 1, 'wwShadowFrame': 2}
        nxt = 3
        for b in G.blocks:
            if b[0] not in fixed:
                fixed[b[0]] = nxt; nxt += 1
            ub(pg, b[1], fixed[b[0]]); b[3] = fixed[b[0]]
        prim, patch_n, nverts = 0x0004, 0, 48
        hasts = any(e in ('tesc', 'tese') for e, _p, _s in st)
        if hasts:
            prim, patch_n = 0x000E, (G.has(0x8E75) or 3)
            nverts = patch_n * 12
        elif any(e == 'geom' for e, _p, _s in st):
            it = G.has(0x8917)
            prim = {0x0000: 0x0000, 0x0001: 0x0001, 0x0004: 0x0004, 0x000A: 0x000A, 0x000C: 0x000C}.get(it, 0x0004)
            nverts = 48
        if not hasts and not any(e == 'geom' for e, _p, _s in st) and any(
                e == 'frag' and 'wwPointCoord' in open(stamps.real(p_)[0], encoding='utf-8', errors='replace').read() for e, p_, _s in st):
            prim = 0x0000   # R9: a point-sprite program (cell_fardots, selection: gl_PointCoord): the app draws points
        def sweep():
            verdict, detail, cov = 'EXACT', '', 0
            o1 = o2 = None
            try:
                for seed in range(seeds):
                    o1, e1 = rig.run(G, G, seed, False, prim, nverts, patch_n)
                    o2, e2 = rig.run(S, G, seed, plant, prim, nverts, patch_n)
                    cov += int((o1 != 0.25).sum())
                    if rig.uerr:
                        detail = 'uniform refused: ' + ','.join(rig.uerr[:6])
                    if e1 != e2:
                        verdict, detail = 'FAIL', 'GL error glsl=%#x spv=%#x' % (e1, e2)
                        break
                    v, d = compare(o1, o2)
                    if v == 'FAIL' or (v == 'BOUND' and verdict == 'EXACT'):
                        verdict, detail = v, (d + ' | ' + detail if detail.startswith('uniform refused') else d)
                        if v == 'FAIL':
                            break
            except Exception as ex:
                verdict, detail = 'SKIP', 'harness: %s' % ex
            return verdict, detail, cov, o1, o2
        global MODE
        MODE = ''
        verdict, detail, cov, o1, o2 = sweep()
        for md in ('zero', 'one'):
            if verdict in ('EXACT', 'BOUND') and cov == 0:
                MODE = md
                verdict, detail, cov, o1, o2 = sweep()
                if cov:
                    detail = ('ints and bools all ' + md + (' | ' + detail if detail else '')).strip()
        MODE = ''
        if verdict == 'FAIL' and os.environ.get('A4_DEBUG'):
            dd = np.nonzero(o1.view(np.uint32) != o2.view(np.uint32))[0]
            print('  DEBUG prim %#x nverts %d patch %d | glsl nonclear %d spv nonclear %d' % (prim, nverts, patch_n, int((o1 != 0.25).sum()), int((o2 != 0.25).sum())))
            print('  attribs', [(n_, l_, hex(t_)) for n_, l_, t_ in G.attribs])
            for nm_, _l, ty_, sz_ in sorted(G.unis) + sorted(G.samps):
                for el in range(max(sz_, 1)):
                    en = nm_ if el == 0 else nm_[:-3] + '[%d]' % el if nm_.endswith('[0]') else nm_
                    la = g('glGetUniformLocation', GLint, GLuint, c_c)(G.p, en.encode())
                    lb = g('glGetUniformLocation', GLint, GLuint, c_c)(S.p, en.encode())
                    if lb < 0:
                        print('  uniform %s: absent in the SPIR-V program' % en)
                        continue
                    isf = ty_ in FLOAT_T or ty_ in MAT_T
                    va, vb = np.zeros(64, dtype=np.float32 if isf else np.int32), np.zeros(64, dtype=np.float32 if isf else np.int32)
                    fnn = g('glGetUniformfv' if isf else 'glGetUniformiv', None, GLuint, GLint, c_p)
                    fnn(G.p, la, va.ctypes.data); fnn(S.p, lb, vb.ctypes.data)
                    if not np.array_equal(va, vb):
                        print('  uniform %s: glsl %s spv %s' % (en, va[:4], vb[:4]))
            for i in dd[:8]:
                print('  att %d pix %d comp %d glsl %.6g spv %.6g' % (i // (SZ * SZ * 4), (i // 4) % (SZ * SZ), i % 4, o1[i], o2[i]))
        if verdict in ('EXACT', 'BOUND') and cov == 0:
            verdict, detail = 'NOCOV', 'the GLSL render wrote no pixel over %d seeds' % seeds
        rows.append((name, group, verdict, detail))
        if verdict != 'SKIP':
            newcache[name] = [ckey, group, verdict, detail]
        print('%-6s %-22s %-34s %s' % (verdict, group, name, detail), flush=True)
    summary(rows, plant)
    names_now = {r[0] for r in rows}
    keep = {n: v for n, v in newcache.items() if n in names_now or (only and not shard)}
    if shard:
        tag = 'a4_plant' if plant else 'a4'
        sp = lambda base: os.path.join(spvdir, 'spirv', '%s.shard%dof%d.txt' % (base, shard[0], shard[1]))
        open(sp(tag), 'w', newline='\n').write(''.join('%d\t%s\n' % (p, json.dumps(list(r))) for p, r in zip(ridx, rows)))
        if not plant:
            open(sp('a4cache'), 'w', newline='\n').write(cache_text(keep))
        print('A4 shard %d of %d written' % shard)
        return
    if not plant:
        open(cachef, 'w', newline='\n').write(cache_text(keep))
    finish(spvdir, rows, plant, apply_)


if __name__ == "__main__":
    main()
