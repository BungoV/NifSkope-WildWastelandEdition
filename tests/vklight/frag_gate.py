"""Lane VKLIGHT1 round 2 (2026-11-12): converted fragment passes, GL .frag vs HLSL ps on Vulkan, on the same inputs.

    python -I tests/vklight/frag_gate.py <out dir> <fragmatch.exe> [<job> ...]

The HLSL is HLSLBULK1's spirv-cross port (res/shaders/hlsl/<module>/<pass>_ps[__<defines>].hlsl, read-only here); the GL
side is the tree's own .frag with the loader's defines (shader_link_check.py's hidden 4.1 context). Everything else is
generic: the Vulkan uniform blocks are packed from spirv-cross --reflect of the SPIR-V fragmatch draws, the GL ones from
the program's own offsets, every texture either side samples is given (a 1x1 zero one where a job names none: GL reads an
incomplete texture as (0,0,0,1), Vulkan a zero image as 0). Jobs live in frag_jobs.py. Each job's reds (literal replaces
in a copy of the HLSL) must FAIL. Last line `FRAG GATE OK` / `FRAG GATE FAILED`. FRAG_TIME=1 times both sides.
"""
import ctypes, json, os, re, subprocess, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
FEAT = os.path.join(TREE, 'res', 'shaders', 'features')
HLSL = os.path.join(TREE, 'res', 'shaders', 'hlsl')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
sys.path.insert(0, HERE)
import shader_link_check as S  # noqa: E402

DXC = os.environ.get('DXC', 'E:/Tools/dxc/bin/x64/dxc.exe')
XC = os.environ.get('SPIRV_CROSS', 'E:/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/spirv-cross.exe')
OUT = os.path.abspath(sys.argv[1])
EXE = os.path.abspath(sys.argv[2])
WANT = sys.argv[3:]
TIME = os.environ.get('FRAG_TIME') == '1'

S.context()
gl = S.gl


def _fn(name, res, *a):
    addr = gl.wglGetProcAddress(name.encode())
    if addr:
        return ctypes.WINFUNCTYPE(res, *a)(addr)
    f = getattr(gl, name)
    f.restype, f.argtypes = res, list(a)
    return f


U, I, E, F = ctypes.c_uint, ctypes.c_int, ctypes.c_uint, ctypes.c_float
VP = ctypes.c_void_p
PI = ctypes.POINTER(I)
createShader = _fn('glCreateShader', U, E); shaderSource = _fn('glShaderSource', None, U, I, ctypes.POINTER(ctypes.c_char_p), VP)
compileShader = _fn('glCompileShader', None, U); getShaderiv = _fn('glGetShaderiv', None, U, E, PI)
getShaderInfoLog = _fn('glGetShaderInfoLog', None, U, I, VP, ctypes.c_char_p)
getProgramInfoLog = _fn('glGetProgramInfoLog', None, U, I, VP, ctypes.c_char_p)
createProgram = _fn('glCreateProgram', U); attach = _fn('glAttachShader', None, U, U); link = _fn('glLinkProgram', None, U)
getProgramiv = _fn('glGetProgramiv', None, U, E, PI); useProgram = _fn('glUseProgram', None, U)
getActiveUniform = _fn('glGetActiveUniform', None, U, U, I, PI, PI, ctypes.POINTER(E), ctypes.c_char_p)
getActiveUniformsiv = _fn('glGetActiveUniformsiv', None, U, I, ctypes.POINTER(U), E, PI)
getActiveUniformBlockiv = _fn('glGetActiveUniformBlockiv', None, U, U, E, PI)
getActiveUniformBlockName = _fn('glGetActiveUniformBlockName', None, U, U, I, PI, ctypes.c_char_p)
uniformBlockBinding = _fn('glUniformBlockBinding', None, U, U, U)
getFragDataLocation = _fn('glGetFragDataLocation', I, U, ctypes.c_char_p)
uloc = _fn('glGetUniformLocation', I, U, ctypes.c_char_p)
u1iv = _fn('glUniform1iv', None, I, I, VP); u2iv = _fn('glUniform2iv', None, I, I, VP)
u3iv = _fn('glUniform3iv', None, I, I, VP); u4iv = _fn('glUniform4iv', None, I, I, VP)
u1uiv = _fn('glUniform1uiv', None, I, I, VP)
u1fv = _fn('glUniform1fv', None, I, I, VP); u2fv = _fn('glUniform2fv', None, I, I, VP)
u3fv = _fn('glUniform3fv', None, I, I, VP); u4fv = _fn('glUniform4fv', None, I, I, VP)
uM3 = _fn('glUniformMatrix3fv', None, I, I, ctypes.c_ubyte, VP); uM4 = _fn('glUniformMatrix4fv', None, I, I, ctypes.c_ubyte, VP)
genVA = _fn('glGenVertexArrays', None, I, ctypes.POINTER(U)); bindVA = _fn('glBindVertexArray', None, U)
genTex = _fn('glGenTextures', None, I, ctypes.POINTER(U)); bindTex = _fn('glBindTexture', None, E, U)
texImage2 = _fn('glTexImage2D', None, E, I, I, I, I, I, E, E, VP)
texImage3 = _fn('glTexImage3D', None, E, I, I, I, I, I, I, E, E, VP)
texImage2Ms = _fn('glTexImage2DMultisample', None, E, I, E, I, I, ctypes.c_ubyte)
texParam = _fn('glTexParameteri', None, E, E, I); activeTex = _fn('glActiveTexture', None, E)
genFB = _fn('glGenFramebuffers', None, I, ctypes.POINTER(U)); bindFB = _fn('glBindFramebuffer', None, E, U)
fbTex = _fn('glFramebufferTexture2D', None, E, E, E, U, I)
drawBuffers = _fn('glDrawBuffers', None, I, VP); readBuffer = _fn('glReadBuffer', None, E)
checkFB = _fn('glCheckFramebufferStatus', E, E)
clearBufferfv = _fn('glClearBufferfv', None, E, I, VP)
viewport = _fn('glViewport', None, I, I, I, I); drawArrays = _fn('glDrawArrays', None, E, I, I)
readPixels = _fn('glReadPixels', None, I, I, I, I, E, E, VP)
pixelStore = _fn('glPixelStorei', None, E, I); finish = _fn('glFinish', None)
genQ = _fn('glGenQueries', None, I, ctypes.POINTER(U)); beginQ = _fn('glBeginQuery', None, E, U); endQ = _fn('glEndQuery', None, E)
getQ64 = _fn('glGetQueryObjectui64v', None, U, E, ctypes.POINTER(ctypes.c_uint64))
genBuf = _fn('glGenBuffers', None, I, ctypes.POINTER(U)); bindBuf = _fn('glBindBuffer', None, E, U)
bufData = _fn('glBufferData', None, E, ctypes.c_ssize_t, VP, E); bindBufBase = _fn('glBindBufferBase', None, E, U, U)
getError = _fn('glGetError', E)
disable = _fn('glDisable', None, E)

FBO = 0x8D40
TARGET = {'2d': 0x0DE1, '3d': 0x806F, '2darray': 0x8C1A, '2dms': 0x9100}
# fmt: GL internal, pixel format, type, numpy dtype, channels
FMT = {'rgba32f': (0x8814, 0x1908, 0x1406, np.float32, 4), 'rgba16f': (0x881A, 0x1908, 0x1406, np.float32, 4),
       'rg32f': (0x8230, 0x8227, 0x1406, np.float32, 2), 'r32f': (0x822E, 0x1903, 0x1406, np.float32, 1),
       'r8': (0x8229, 0x1903, 0x1401, np.uint8, 1), 'rgba8': (0x8058, 0x1908, 0x1401, np.uint8, 4),
       'rg32ui': (0x823C, 0x8228, 0x1405, np.uint32, 2), 'r32ui': (0x8236, 0x8D94, 0x1405, np.uint32, 1),
       'rgba32ui': (0x8D70, 0x8D99, 0x1405, np.uint32, 4)}
VERT = """#version 410 core
void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }
"""
va = U(); genVA(1, ctypes.byref(va)); bindVA(va)
pixelStore(0x0CF5, 1); pixelStore(0x0D05, 1)
disable(0x0B71); disable(0x0BE2)  # depth test, blend


def fail(msg):
    print('FRAG GATE FAILED (%s)' % msg)
    sys.exit(2)


def compile_(kind, src):
    s = createShader(kind)
    b = ctypes.c_char_p(src.encode('latin-1'))
    shaderSource(s, 1, ctypes.byref(b), None)
    compileShader(s)
    ok = I(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(s, 1 << 16, None, log)
        print('COMPILE FAIL', log.value.decode(errors='replace')[:2000]); fail('gl compile')
    return s


def edits(src, pairs, what):
    """a probe's literal replaces (each anchor exactly once)"""
    for old, new in pairs:
        if src.count(old) != 1:
            fail('%s probe anchor found %d times: %s' % (what, src.count(old), old[:60]))
        src = src.replace(old, new)
    return src


def gl_program(frag, defines, gl_edit=()):
    p = createProgram()
    attach(p, compile_(0x8B31, VERT))
    attach(p, compile_(0x8B30, edits(S.inject(S.expand(os.path.join(FEAT, frag)), defines), gl_edit, 'gl')))
    link(p)
    ok = I(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16); getProgramInfoLog(p, 1 << 16, None, log)
        print(log.value.decode(errors='replace')[:2000]); fail('gl link %s' % frag)
    return p


# ---- values -------------------------------------------------------------------------------------------------------
def flat(v, dt):
    return np.ascontiguousarray(np.asarray(v, dtype=np.float64).reshape(-1)).astype(dt)


GLT = {0x1406: ('f', 1), 0x8B50: ('f', 2), 0x8B51: ('f', 3), 0x8B52: ('f', 4), 0x1404: ('i', 1), 0x8B53: ('i', 2),
       0x8B54: ('i', 3), 0x8B55: ('i', 4), 0x8B56: ('i', 1), 0x8B57: ('i', 2), 0x8B58: ('i', 3), 0x8B59: ('i', 4),
       0x1405: ('u', 1), 0x8B5B: ('m3', 9), 0x8B5C: ('m4', 16)}


def gl_uniforms(p, vals, units):
    """loose uniforms by glUniform*, the members of each block into a buffer at binding = block index + 1"""
    useProgram(p)
    n = I(); getProgramiv(p, 0x8B86, ctypes.byref(n))
    blocks = {}
    for i in range(n.value):
        ln, sz, ty = I(), I(), E()
        nm = ctypes.create_string_buffer(256)
        getActiveUniform(p, i, 256, ctypes.byref(ln), ctypes.byref(sz), ctypes.byref(ty), nm)
        name = nm.value.decode()
        base = name[:-3] if name.endswith('[0]') else name
        idx = U(i)
        bi = I(); getActiveUniformsiv(p, 1, ctypes.byref(idx), 0x8A3A, ctypes.byref(bi))
        if ty.value not in GLT:  # a sampler: its unit
            if base in units:
                loc = uloc(p, base.encode())
                arr = np.array([units[base]], np.int32)
                u1iv(loc, 1, arr.ctypes.data)
            continue
        kind, comp = GLT[ty.value]
        if bi.value >= 0:
            off, ast, mst = I(), I(), I()
            getActiveUniformsiv(p, 1, ctypes.byref(idx), 0x8A3B, ctypes.byref(off))
            getActiveUniformsiv(p, 1, ctypes.byref(idx), 0x8A3C, ctypes.byref(ast))
            getActiveUniformsiv(p, 1, ctypes.byref(idx), 0x8A3D, ctypes.byref(mst))
            blocks.setdefault(bi.value, []).append((base, kind, comp, sz.value, off.value, ast.value, mst.value))
            continue
        if base not in vals:
            continue
        loc = uloc(p, name.encode())
        if kind == 'f':
            a = flat(vals[base], np.float32)
            {1: u1fv, 2: u2fv, 3: u3fv, 4: u4fv}[comp](loc, len(a) // comp, a.ctypes.data)
        elif kind == 'i':
            a = flat(vals[base], np.int32)
            {1: u1iv, 2: u2iv, 3: u3iv, 4: u4iv}[comp](loc, len(a) // comp, a.ctypes.data)
        elif kind == 'u':
            a = flat(vals[base], np.uint32)
            u1uiv(loc, len(a), a.ctypes.data)
        elif kind == 'm3':
            a = flat(vals[base], np.float32)
            uM3(loc, len(a) // 9, 0, a.ctypes.data)
        else:
            a = flat(vals[base], np.float32)
            uM4(loc, len(a) // 16, 0, a.ctypes.data)
    bufs = []
    for bi, members in blocks.items():
        size = I(); getActiveUniformBlockiv(p, bi, 0x8A40, ctypes.byref(size))
        raw = bytearray(size.value)
        for (base, kind, comp, sz, off, ast, mst) in members:
            if base not in vals:
                continue
            put_member(raw, vals[base], kind, comp, sz, off, ast, mst)
        b = U(); genBuf(1, ctypes.byref(b)); bindBuf(0x8A11, b)
        data = (ctypes.c_ubyte * len(raw)).from_buffer(raw)
        bufData(0x8A11, len(raw), ctypes.addressof(data), 0x88E4)
        bindBufBase(0x8A11, bi + 1, b)
        uniformBlockBinding(p, bi, bi + 1)
        bufs.append(b)
    return bufs


SAMPLER_KIND = {0x8B5E: '2d', 0x8B5F: '3d', 0x8DC1: '2darray', 0x9108: '2dms', 0x8DD2: '2d', 0x8DCA: '2d', 0x8DD3: '3d',
                0x8DD7: '2darray', 0x910A: '2dms', 0x8DCF: '2darray', 0x8DCB: '3d', 0x9109: '2dms'}


def gl_samplers(p):
    n = I(); getProgramiv(p, 0x8B86, ctypes.byref(n))
    out = {}
    for i in range(n.value):
        ln, sz, ty = I(), I(), E()
        nm = ctypes.create_string_buffer(256)
        getActiveUniform(p, i, 256, ctypes.byref(ln), ctypes.byref(sz), ctypes.byref(ty), nm)
        if ty.value in SAMPLER_KIND:
            out[nm.value.decode()] = SAMPLER_KIND[ty.value]
        elif ty.value not in GLT:
            fail('gl uniform type 0x%x (%s)' % (ty.value, nm.value.decode()))
    return out


def put_member(raw, v, kind, comp, count, off, ast, mst):
    """one member (count array elements) into a block's bytes: matrices are GL columns at the matrix stride"""
    if kind in ('m3', 'm4'):
        n = 3 if kind == 'm3' else 4
        a = flat(v, np.float32).reshape(-1, n, n)
        for e in range(min(count, len(a))):
            for c in range(n):
                o = off + e * (ast or n * mst) + c * mst
                raw[o:o + 4 * n] = a[e, c].tobytes()
        return
    dt = {'f': np.float32, 'i': np.int32, 'u': np.uint32}[kind]
    a = flat(v, dt).reshape(-1, comp)
    for e in range(min(count, len(a))):
        o = off + e * (ast or 0)
        raw[o:o + 4 * comp] = a[e].tobytes()


XT = {'float': ('f', 1), 'vec2': ('f', 2), 'vec3': ('f', 3), 'vec4': ('f', 4), 'int': ('i', 1), 'ivec2': ('i', 2),
      'ivec3': ('i', 3), 'ivec4': ('i', 4), 'uint': ('u', 1), 'uvec2': ('u', 2), 'uvec3': ('u', 3), 'uvec4': ('u', 4),
      'bool': ('u', 1), 'mat3': ('m3', 9), 'mat4': ('m4', 16)}


def vk_blocks(refl, vals, jobdir, tag):
    """the SPIR-V's uniform blocks packed from its reflection: [(binding, file)]"""
    out = []
    for ub in refl.get('ubos', []):
        t = refl['types'][ub['type']]
        raw = bytearray(ub['block_size'])
        for m in t['members']:
            if m['name'] not in vals:
                continue
            if m['type'] not in XT:
                fail('block member type %s (%s)' % (m['type'], m['name']))
            kind, comp = XT[m['type']]
            count = m['array'][0] if m.get('array') else 1
            put_member(raw, vals[m['name']], kind, comp, count, m['offset'], m.get('array_stride', 0), m.get('matrix_stride', 16))
        f = os.path.join(jobdir, '%s_ubo%d.bin' % (tag, ub['binding']))
        with open(f, 'wb') as fh:
            fh.write(bytes(raw))
        out.append((ub['binding'], f))
    return out


# ---- textures -----------------------------------------------------------------------------------------------------
class Tex:
    """kind 2d / 3d / 2darray / 2dms; data (h, w, ch) or (d, h, w, ch); filt linear / nearest; wrap clamp / repeat"""
    def __init__(self, kind, fmt, data, filt='linear', wrap='clamp', samples=4):
        self.kind, self.fmt, self.filt, self.wrap = kind, fmt, filt, wrap
        dt, ch = FMT[fmt][3], FMT[fmt][4]
        if kind == '2dms':
            self.data = None
            self.h, self.w, self.d = data[0], data[1], samples
            return
        a = np.asarray(data)
        if a.ndim == 2 + (kind != '2d'):
            a = a[..., None]
        if fmt == 'rgba16f':  # the halves both sides store (GL rounds a float upload the same way)
            a = a.astype(np.float16).astype(np.float32)
        self.data = np.ascontiguousarray(a.astype(dt))
        if a.shape[-1] != ch:
            fail('texture channels %s vs %s' % (a.shape, fmt))
        if kind == '2d':
            self.h, self.w = a.shape[:2]; self.d = 1
        else:
            self.d, self.h, self.w = a.shape[:3]


def zero_tex(kind):
    if kind == '2dms':
        return Tex('2dms', 'r32f', (1, 1), 'nearest', samples=4)
    shape = (1, 1, 4) if kind == '2d' else (1, 1, 1, 4)
    return Tex(kind, 'rgba32f', np.zeros(shape, np.float32), 'nearest')


def gl_texture(t, unit):
    tgt = TARGET[t.kind]
    internal, pf, ty, dt, ch = FMT[t.fmt]
    x = U(); genTex(1, ctypes.byref(x))
    activeTex(0x84C0 + unit); bindTex(tgt, x)
    if t.kind == '2dms':
        texImage2Ms(tgt, t.d, internal, t.w, t.h, 1)
        return x.value
    up = t.data
    if t.fmt in ('rgba16f', 'r16f'):  # the same halves Vulkan gets (GL's own float -> half rounds otherwise)
        up = np.ascontiguousarray(t.data.astype(np.float16)); ty = 0x140B
    t.keep = up
    p = up.ctypes.data
    if t.kind == '2d':
        texImage2(tgt, 0, internal, t.w, t.h, 0, pf, ty, p)
    else:
        texImage3(tgt, 0, internal, t.w, t.h, t.d, 0, pf, ty, p)
    f = 0x2601 if t.filt == 'linear' else 0x2600
    w = 0x2901 if t.wrap == 'repeat' else 0x812F
    texParam(tgt, 0x2801, f); texParam(tgt, 0x2800, f)
    texParam(tgt, 0x2802, w); texParam(tgt, 0x2803, w); texParam(tgt, 0x8072, w)
    texParam(tgt, 0x813D, 0)
    return x.value


# ---- one job ------------------------------------------------------------------------------------------------------
def shifts(hlsl_path):
    src = open(hlsl_path, encoding='latin-1').read()
    m = re.search(r'#include\s+"(\w+BindingsVk\.hlsli)"', src)
    sh = {'b': 0, 't': 0, 's': 0, 'u': 0}
    if m:
        b = open(os.path.join(os.path.dirname(hlsl_path), m.group(1)), encoding='latin-1').read()
        for c, n in re.findall(r'\b([btsu]) (\d+)', b.split('vk shift:')[1].split('\n')[0]):
            sh[c] = int(n)
    return sh


def dxc(src, spv, profile, inc, sh):
    args = [DXC, '-spirv', '-fspv-target-env=vulkan1.2', '-T', profile, '-E', 'main', '-O0', '-I', inc]
    for c in 'btsu':
        args += ['-fvk-%s-shift' % c, str(sh[c]), '0']
    r = subprocess.run(args + ['-Fo', spv, src], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr[-3000:]); fail('dxc %s' % os.path.basename(src))


def reflect(spv):
    r = subprocess.run([XC, spv, '--reflect'], capture_output=True, text=True)
    if r.returncode:
        fail('spirv-cross reflect %s' % spv)
    return json.loads(r.stdout)


def outputs(hlsl_src):
    m = re.search(r'struct SPIRV_Cross_Output\s*\{(.*?)\};', hlsl_src, re.S)
    return [(n, int(k)) for n, k in re.findall(r'(\w+)\s*:\s*SV_Target(\d+)', m.group(1))] if m else []


def ulp_ord(a):
    i = a.view(np.int32).astype(np.int64)
    return np.where(i < 0, -(i & 0x7FFFFFFF), i)


def compare(gl_a, vk_a):
    """per value: ulp distance (two NaNs equal, NaN vs number = huge), and the relative difference"""
    gn, vn = np.isnan(gl_a), np.isnan(vk_a)
    d = np.abs(ulp_ord(gl_a) - ulp_ord(vk_a))
    d = np.where(gn | vn, np.where(gn == vn, 0, 1 << 40), d)
    with np.errstate(invalid='ignore', over='ignore'):
        rel = np.abs(gl_a.astype(np.float64) - vk_a) / np.maximum(np.maximum(np.abs(gl_a), np.abs(vk_a)), 1e-30)
    rel = np.where(gn | vn | (d == 0), 0.0, np.nan_to_num(rel, nan=1.0, posinf=1.0))
    return d, rel


def verdict(job, d, rel):
    n = d.size
    nd = int((d > 0).sum()); n1 = int((d > 1).sum()); n16 = int((d > 16).sum())
    mx = int(d.max()) if n else 0
    stats = 'values %d, differ %d, >1ulp %d, >16ulp %d, max ulp %d, max rel %.3g' % (n, nd, n1, n16, mx, float(rel.max()) if n else 0.0)
    if nd == 0:
        return 'EXACT', stats
    rule = job.get('rule', ('ulp', 1))
    if rule[0] == 'ulp':
        ok = mx <= rule[1]
    elif rule[0] == 'rel':  # (rel, max relative difference, share of values allowed past it)
        ok = float((rel > rule[1]).sum()) <= rule[2] * n
    else:
        ok = False
    return ('BOUND' if ok else 'FAIL'), stats + ' rule %s' % (rule,)


def run_job(job, jobdir):
    os.makedirs(jobdir, exist_ok=True)
    W, H = job['size']
    hpath = os.path.join(HLSL, job['hlsl'])
    hsrc = open(hpath, encoding='latin-1').read()
    sh = shifts(hpath)
    spv = os.path.join(jobdir, 'ps.spv')
    if job.get('vk_edit'):
        hsrc = edits(hsrc, job['vk_edit'], 'vk')
        src = os.path.join(jobdir, 'probe.hlsl')
        with open(src, 'w', encoding='latin-1', newline='') as fh:
            fh.write(hsrc)
        dxc(src, spv, 'ps_6_0', os.path.dirname(hpath), sh)
    else:
        dxc(hpath, spv, 'ps_6_0', os.path.dirname(hpath), sh)
    vs = os.path.join(OUT, 'frag_vs.spv')
    refl = reflect(spv)
    vals = job['uniforms']
    texs = job['textures']
    outs = outputs(hsrc)
    nrt = max(k for _, k in outs) + 1 if outs else 1

    # GL
    p = gl_program(job['glsl'], job.get('defines', []), job.get('gl_edit', ()))
    kinds = gl_samplers(p)  # every sampler GL declares gets its own unit (two types on one unit = a draw error)
    kinds.update({im['name']: ('3d' if '3D' in im['type'] else '2darray' if 'Array' in im['type'] else '2dms' if 'MS' in im['type']
                               else '2d') for im in refl.get('separate_images', [])})
    names = sorted(set(kinds) | set(texs))
    full = {n: texs.get(n) or zero_tex(kinds.get(n, '2d')) for n in names}
    units = {}
    for u, n in enumerate(names):
        gl_texture(full[n], u)
        units[n] = u
    gl_uniforms(p, vals, units)
    useProgram(p)
    fbo = U(); genFB(1, ctypes.byref(fbo)); bindFB(FBO, fbo)
    rts = []
    bufs = (U * 8)(*([0] * 8))
    for name, k in outs or [('fragColor', 0)]:
        t = U(); genTex(1, ctypes.byref(t)); activeTex(0x84C0 + 31); bindTex(0x0DE1, t)
        texImage2(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, None)
        texParam(0x0DE1, 0x2801, 0x2600); texParam(0x0DE1, 0x2800, 0x2600)
        fbTex(FBO, 0x8CE0 + k, 0x0DE1, t, 0)
        loc = getFragDataLocation(p, name.encode())
        if loc < 0:
            fail('gl output %s has no location' % name)
        bufs[loc] = 0x8CE0 + k
        rts.append(t)
    drawBuffers(8, bufs)
    if checkFB(FBO) != 0x8CD5:
        fail('gl fbo')
    viewport(0, 0, W, H)
    zero = np.zeros(4, np.float32)
    for k in range(nrt):
        clearBufferfv(0x1800, k, zero.ctypes.data)
    useProgram(p)
    drawArrays(0x0004, 0, 3)
    finish()
    err = getError()
    if err:
        fail('gl error 0x%x' % err)
    gl_out = []
    for k in range(nrt):
        readBuffer(0x8CE0 + k)
        a = np.zeros((H, W, 4), np.float32)
        readPixels(0, 0, W, H, 0x1908, 0x1406, a.ctypes.data)
        gl_out.append(a)
    gl_ms = None
    if TIME:
        q = U(); genQ(1, ctypes.byref(q))
        t0 = time.time()
        while time.time() - t0 < 0.5:
            drawArrays(0x0004, 0, 3); finish()
        ms = []
        for _ in range(32):
            beginQ(0x88BF, q); drawArrays(0x0004, 0, 3); endQ(0x88BF)
            r = ctypes.c_uint64(); getQ64(q, 0x8866, ctypes.byref(r)); ms.append(r.value * 1e-6)
        gl_ms = sorted(ms)[16]

    # Vulkan job
    def write_job(path, ps_spv, tag):
        lines = ['size %d %d' % (W, H), 'vs %s' % vs.replace('\\', '/'), 'ps %s' % ps_spv.replace('\\', '/'), 'rt %d' % nrt]
        for b, f in vk_blocks(refl, vals, jobdir, tag):
            lines.append('ubo %d %s' % (b, f.replace('\\', '/')))
        for im in refl.get('separate_images', []):
            t = full[im['name']]
            f = os.path.join(jobdir, 'tex_%s.bin' % im['name'])
            if t.data is not None and tag == 'green':
                (t.data.astype(np.float16) if t.fmt in ('rgba16f', 'r16f') else t.data).tofile(f)
            lines.append('tex %d %s %s %d %d %d %s' % (im['binding'], t.kind, t.fmt, t.w, t.h, t.d, f.replace('\\', '/')))
        for sm in refl.get('separate_samplers', []):
            base = sm['name'][1:-8] if sm['name'].startswith('_') and sm['name'].endswith('_sampler') else sm['name']
            t = full.get(base) or zero_tex('2d')
            lines.append('smp %d %s %s' % (sm['binding'], t.filt, t.wrap))
        with open(path, 'w') as fh:
            fh.write('\n'.join(lines) + '\n')

    write_job(os.path.join(jobdir, 'job.txt'), spv, 'green')
    r = subprocess.run([EXE, os.path.join(jobdir, 'job.txt')] + (['time'] if TIME else []), capture_output=True, text=True)
    line = r.stdout.strip().split('\n')[-1] if r.stdout.strip() else 'no output'
    if r.returncode or 'RAN' not in line:
        print(r.stdout[-2000:]); fail('fragmatch %s' % job['name'])
    vk_ms = float(line.split('vk_ms')[1]) if 'vk_ms' in line else None

    ok = True
    for k in range(nrt):
        vk = np.fromfile(os.path.join(jobdir, 'vk_rt%d.bin' % k), np.float32, offset=8).reshape(H, W, 4)
        np.save(os.path.join(jobdir, 'gl_rt%d.npy' % k), gl_out[k])
        d, rel = compare(gl_out[k], vk)
        v, stats = verdict(job, d, rel)
        lit = int((np.abs(gl_out[k]) > 0).any(-1).sum())
        print('FRAGMATCH %s rt%d %s (%s; nonzero px %d of %d)' % (job['name'], k, v, stats, lit, W * H))
        ok = ok and v != 'FAIL'
    if gl_ms is not None or vk_ms is not None:
        print('time %s gl_ms %s vk_ms %s' % (job['name'], gl_ms, vk_ms))

    # reds
    nred = 0
    for i, (old, new) in enumerate(job.get('reds', [])):
        if hsrc.count(old) < 1:
            fail('red %d anchor not found in %s' % (i, job['hlsl']))
        rdir = os.path.join(jobdir, 'red%d' % i)
        os.makedirs(rdir, exist_ok=True)
        rsrc = os.path.join(rdir, 'red.hlsl')
        with open(rsrc, 'w', encoding='latin-1', newline='') as fh:
            fh.write(hsrc.replace(old, new, 1))
        rspv = os.path.join(rdir, 'ps.spv')
        dxc(rsrc, rspv, 'ps_6_0', os.path.dirname(hpath), sh)
        write_job(os.path.join(rdir, 'job.txt'), rspv, 'red')
        r = subprocess.run([EXE, os.path.join(rdir, 'job.txt')], capture_output=True, text=True)
        if 'RAN' not in r.stdout:
            print(r.stdout[-1000:]); fail('fragmatch red %s %d' % (job['name'], i))
        worst, rstats = 'EXACT', ''
        for k in range(nrt):
            vk = np.fromfile(os.path.join(rdir, 'vk_rt%d.bin' % k), np.float32, offset=8).reshape(H, W, 4)
            d, rel = compare(gl_out[k], vk)
            v, stats = verdict(job, d, rel)
            rstats += ' rt%d %s' % (k, stats.split(' rule')[0])
            if v == 'FAIL':
                worst = 'FAIL'
        print('red %s.%d (%s -> %s): %s (%s)' % (job['name'], i, old.strip()[:50], new.strip()[:50], worst, rstats.strip()))
        if worst != 'FAIL':
            print('RED %s.%d DID NOT FAIL' % (job['name'], i)); ok = False
        nred += 1
    return ok, nred


def main():
    import frag_jobs
    os.makedirs(OUT, exist_ok=True)
    dxc(os.path.join(HERE, 'frag_vs.hlsl'), os.path.join(OUT, 'frag_vs.spv'), 'vs_6_0', HERE, {'b': 0, 't': 0, 's': 0, 'u': 0})
    jobs = frag_jobs.jobs(WANT, Tex)
    if not jobs:
        fail('no jobs')
    ok, nred, njobs = True, 0, 0
    for job in jobs:
        jok, jr = run_job(job, os.path.join(OUT, job['name']))
        ok, nred, njobs = ok and jok, nred + jr, njobs + 1
    print('jobs run: %d, reds run: %d' % (njobs, nred))
    print('FRAG GATE OK' if ok and nred >= njobs else 'FRAG GATE FAILED')
    sys.exit(0 if ok and nred >= njobs else 1)


main()
