"""lane PERFFIX3 (item 8): the FO4CS bloom's final tent once per PIXEL instead of once per MSAA sample, proven on the real
driver in a hidden GL 4.1 context (tests/spells/shader_link_check.py), no NifSkope.

    python -I tests/bloomcomb/bloomcomb_check.py [--base <old cell_hdr.frag>] [--red half|nolod] [--time]

Arms, each over a random HDR top (RGBA16F, half the frame, sanitize-edge values: NaN, inf, > 65504) with and without its
mips, radius 1 / 1.6, dispersion 0 / 0.15, Bloom Mip mode w 0 / -1 (red wide, lod 2) / +1 with the fog gain 1:
  OLD  the tent block of the BASE cell_hdr.frag (cut by text), per sample, into a 4x MS RGBA32F target
  NEW  the tree's cell_bloom_comb.frag once per pixel into RGBA32F, then the tree's cell_hdr.frag tent block per sample
       (cut by text, cellIsFoBloomCombOn true: one texelFetch)
Gate E (exact): every sample's rgb bits equal, OLD vs NEW, in every arm.
Gate X (the edge rule): Bloom Mip +1 with a fog gain that differs between a pixel's samples (1 on samples 0-1, 3 on 2-3):
  the per-sample lod differs, so the per-pixel tent CANNOT stand in; NEW there must differ from OLD (it is why
  cellbloom.cpp runs no combine when Bloom Mip and Fog Bloom Boost are both on: that arm keeps the tone map's own taps).
Reds (each must FAIL gate E): half = the combine target RGBA16F; nolod = the combine ignores the red wide's lod 2.
--time: GL_TIME_ELAPSED at 1920x1080 4x MSAA, OLD (tent per sample) vs NEW (combine pass + one fetch per sample),
20 interleaved rounds, medians; this machine's GPU, offline, the tent part of the tone map only.
"""
import ctypes, os, re, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
SH = os.path.join(TREE, 'res', 'shaders')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

args = sys.argv[1:]
RED = args[args.index('--red') + 1] if '--red' in args else ''
TIME = '--time' in args
BASE = args[args.index('--base') + 1] if '--base' in args else ''
if not BASE:
    git = os.environ.get('GIT', 'E:/Tools/Git/cmd/git.exe')
    BASE_SRC = subprocess.run([git, '-C', TREE, 'show', '4c9ea88e:res/shaders/cell_hdr.frag'], capture_output=True).stdout.decode('latin-1')
else:
    BASE_SRC = open(BASE, 'rb').read().decode('latin-1')
TREE_SRC = open(S.shader_path(os.path.join(SH, 'cell_hdr.frag')), 'rb').read().decode('latin-1')

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
createShader = _fn('glCreateShader', U, E); shaderSource = _fn('glShaderSource', None, U, I, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)
compileShader = _fn('glCompileShader', None, U); getShaderiv = _fn('glGetShaderiv', None, U, E, ctypes.POINTER(I))
getShaderInfoLog = _fn('glGetShaderInfoLog', None, U, I, ctypes.c_void_p, ctypes.c_char_p)
createProgram = _fn('glCreateProgram', U); attach = _fn('glAttachShader', None, U, U); link = _fn('glLinkProgram', None, U)
getProgramiv = _fn('glGetProgramiv', None, U, E, ctypes.POINTER(I)); useProgram = _fn('glUseProgram', None, U)
uloc = _fn('glGetUniformLocation', I, U, ctypes.c_char_p)
u1i = _fn('glUniform1i', None, I, I); u1f = _fn('glUniform1f', None, I, F); u2f = _fn('glUniform2f', None, I, F, F)
u4f = _fn('glUniform4f', None, I, F, F, F, F); u1fv = _fn('glUniform1fv', None, I, I, ctypes.POINTER(F))
genVA = _fn('glGenVertexArrays', None, I, ctypes.POINTER(U)); bindVA = _fn('glBindVertexArray', None, U)
genTex = _fn('glGenTextures', None, I, ctypes.POINTER(U)); bindTex = _fn('glBindTexture', None, E, U)
delTex = _fn('glDeleteTextures', None, I, ctypes.POINTER(U))
texImage = _fn('glTexImage2D', None, E, I, I, I, I, I, E, E, ctypes.c_void_p)
texImageMS = _fn('glTexImage2DMultisample', None, E, I, E, I, I, ctypes.c_ubyte)
texParam = _fn('glTexParameteri', None, E, E, I); activeTex = _fn('glActiveTexture', None, E)
genMip = _fn('glGenerateMipmap', None, E)
genFB = _fn('glGenFramebuffers', None, I, ctypes.POINTER(U)); bindFB = _fn('glBindFramebuffer', None, E, U)
fbTex = _fn('glFramebufferTexture2D', None, E, E, E, U, I); viewport = _fn('glViewport', None, I, I, I, I)
drawArrays = _fn('glDrawArrays', None, E, I, I)
readPixels = _fn('glReadPixels', None, I, I, I, I, E, E, ctypes.c_void_p)
pixelStore = _fn('glPixelStorei', None, E, I); finish = _fn('glFinish', None)
genQ = _fn('glGenQueries', None, I, ctypes.POINTER(U)); beginQ = _fn('glBeginQuery', None, E, U); endQ = _fn('glEndQuery', None, E)
getQ64 = _fn('glGetQueryObjectui64v', None, U, E, ctypes.POINTER(ctypes.c_uint64))
getError = _fn('glGetError', E)

T2D, TMS, FBO = 0x0DE1, 0x9100, 0x8D40
SRC_ON = 0
va = U(); genVA(1, ctypes.byref(va)); bindVA(va)
pixelStore(0x0CF5, 1)
VERT = """#version 410 core
void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }
"""


def compile_(kind, src):
    s = createShader(kind)
    b = ctypes.c_char_p(src.encode('latin-1'))
    shaderSource(s, 1, ctypes.byref(b), None)
    compileShader(s)
    ok = I(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(s, 1 << 16, None, log)
        print('COMPILE FAIL', log.value.decode(errors='replace')[:2000]); print('BLOOMCOMB-CHECK FAIL (compile)'); sys.exit(2)
    return s


def program(frag):
    p = createProgram()
    attach(p, compile_(0x8B31, VERT)); attach(p, compile_(0x8B30, frag)); link(p)
    ok = I(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        print('BLOOMCOMB-CHECK FAIL (link)'); sys.exit(2)
    return p


def cut_tent(src, comb):
    """the foBloom block of a cell_hdr.frag, from `float bloomLod` to the end of its last tent branch"""
    i = src.index('\tfloat bloomLod')
    end = 'cellIsFoBloomParams.z ) * cellIsFoBloomParams.y;'   # the last branch (the plain tent); the lod one ends `bloomLod ) *`
    j = src.index(end, i) + len(end)
    block = src[i:j]
    if comb and 'cellIsFoBloomCombOn' not in block:
        print('BLOOMCOMB-CHECK FAIL (the tree cell_hdr.frag has no combine read)'); sys.exit(2)
    return block


BLOOM = S.expand(os.path.join(SH, 'cell_bloom.glsl'))
PROBE = """#version 410 core
%s
uniform sampler2D cellIsFoBloom;
uniform bool cellIsFoBloomOn;
uniform vec4 cellIsFoBloomParams;
uniform vec2 cellIsFoBloomFrame;
uniform bool cellIsFoBloomCombOn;
uniform sampler2D cellIsFoBloomComb;
uniform float gains[4];
uniform sampler2DMS hdrSrc;	// --time only: the tone map's own per-sample read of the linear frame
uniform bool srcOn;
out vec4 fragColor;
void main()
{
	float cellBloomGain = gains[gl_SampleID];
	vec3 foBloom = vec3( 0.0 );
%s
	if ( srcOn )
		foBloom += texelFetch( hdrSrc, ivec2( gl_FragCoord.xy ), gl_SampleID ).rgb;
	fragColor = vec4( foBloom, 1.0 );
}
"""
p_old = program(PROBE % (BLOOM, cut_tent(BASE_SRC, False)))
p_new = program(PROBE % (BLOOM, cut_tent(TREE_SRC, True)))
comb_src = S.expand(os.path.join(SH, 'cell_bloom_comb.frag'))
if RED == 'nolod':
    comb_src = comb_src.replace('cellIsFoBloomParams.w < 0.0 ? 2.0 : 0.0', '0.0')
p_comb = program(comb_src)
FETCH = """#version 410 core
uniform sampler2DMS ms;
uniform int s;
out vec4 fragColor;
void main() { fragColor = texelFetch( ms, ivec2( gl_FragCoord.xy ), s ); }
"""
p_fetch = program(FETCH)


def tex2d(w, h, fmt, data=None, nearest=True):
    t = U(); genTex(1, ctypes.byref(t)); bindTex(T2D, t)
    d = None if data is None else np.ascontiguousarray(data, np.float32)
    texImage(T2D, 0, fmt, w, h, 0, 0x1908, 0x1406, None if d is None else d.ctypes.data)
    f = 0x2600 if nearest else 0x2601
    texParam(T2D, 0x2801, f); texParam(T2D, 0x2800, f)
    texParam(T2D, 0x2802, 0x812F); texParam(T2D, 0x2803, 0x812F)
    texParam(T2D, 0x813D, 0)
    return t


def fbo_for(tex, target=T2D):
    fb = U(); genFB(1, ctypes.byref(fb)); bindFB(FBO, fb); fbTex(FBO, 0x8CE0, target, tex, 0)
    return fb


def setu(p, name, *v):
    l = uloc(p, name.encode())
    if l < 0:
        return
    if len(v) == 1 and isinstance(v[0], (bool, int)):
        u1i(l, int(v[0]))
    elif len(v) == 1:
        u1f(l, v[0])
    elif len(v) == 2:
        u2f(l, *v)
    else:
        u4f(l, *v)


class Frame:
    def __init__(self, W, H, comb_fmt, ms_fmt=0x8814):
        self.W, self.H = W, H
        self.ms = U(); genTex(1, ctypes.byref(self.ms)); bindTex(TMS, self.ms)
        texImageMS(TMS, 4, ms_fmt, W, H, 1)
        self.msfb = fbo_for(self.ms, TMS)
        self.comb = tex2d(W, H, comb_fmt)
        self.combfb = fbo_for(self.comb)
        self.out = tex2d(W, H, 0x8814)
        self.outfb = fbo_for(self.out)


def top_texture(w, h, mips, rng):
    v = np.exp(rng.normal(-1.0, 2.0, (h, w, 4))).astype(np.float32)
    v[rng.random((h, w)) < 0.002] = np.inf
    v[rng.random((h, w)) < 0.002] = np.nan
    v[rng.random((h, w)) < 0.002] = 1e6
    t = tex2d(w, h, 0x881A, v, nearest=False)   # RGBA16F, linear, clamped: the chain's makeTex
    if mips:
        texParam(T2D, 0x813D, 3); texParam(T2D, 0x2801, 0x2703); genMip(T2D)
    return t


def bind_unit(unit, target, tex):
    activeTex(0x84C0 + unit); bindTex(target, tex); activeTex(0x84C0)


def uniforms(p, fr, params, gains, comb_on):
    useProgram(p)
    setu(p, 'cellIsFoBloom', 1); setu(p, 'cellIsFoBloomOn', 1); setu(p, 'cellIsFoBloomComb', 2)
    setu(p, 'cellIsFoBloomCombOn', 1 if comb_on else 0)
    setu(p, 'hdrSrc', 4); setu(p, 'srcOn', SRC_ON)
    setu(p, 'cellIsFoBloomParams', *params); setu(p, 'cellIsFoBloomFrame', float(fr.W), float(fr.H))
    l = uloc(p, b'gains')
    if l >= 0:
        a = (F * 4)(*gains); u1fv(l, 4, a)


def draw_old(fr, params, gains):
    bindFB(FBO, fr.msfb); viewport(0, 0, fr.W, fr.H)
    uniforms(p_old, fr, params, gains, False); drawArrays(4, 0, 3)


def draw_new(fr, params, gains):
    bindFB(FBO, fr.combfb); viewport(0, 0, fr.W, fr.H)
    uniforms(p_comb, fr, params, gains, False); drawArrays(4, 0, 3)
    bind_unit(2, T2D, fr.comb)
    bindFB(FBO, fr.msfb)
    uniforms(p_new, fr, params, gains, True); drawArrays(4, 0, 3)


def read_samples(fr):
    out = []
    bind_unit(3, TMS, fr.ms)
    for s in range(4):
        bindFB(FBO, fr.outfb); viewport(0, 0, fr.W, fr.H)
        useProgram(p_fetch); setu(p_fetch, 'ms', 3); setu(p_fetch, 's', s); drawArrays(4, 0, 3)
        a = np.empty((fr.H, fr.W, 4), np.float32)
        readPixels(0, 0, fr.W, fr.H, 0x1908, 0x1406, a.ctypes.data)
        out.append(a[..., :3].copy())
    return np.stack(out)


def run_arm(fr, top, params, gains):
    bind_unit(1, T2D, top)
    draw_old(fr, params, gains); a = read_samples(fr)
    draw_new(fr, params, gains); b = read_samples(fr)
    same = (a.view(np.uint32) == b.view(np.uint32)) | (np.isnan(a) & np.isnan(b))
    return int((~same).sum()), int(same.size), a, b


def main():
    rng = np.random.default_rng(1101)
    comb_fmt = 0x881A if RED == 'half' else 0x8814
    W, H = 331, 187   # odd sizes: the half-size top rounds up, uv off the texel grid
    fr = Frame(W, H, comb_fmt)
    tops = {False: top_texture((W + 1) // 2, (H + 1) // 2, False, rng), True: top_texture((W + 1) // 2, (H + 1) // 2, True, rng)}
    bad = 0
    print('gate E: OLD (base tent per sample) vs NEW (combine once per pixel + one fetch), every sample, bits')
    for mips, w in ((False, 0.0), (True, 0.0), (True, -1.0), (True, 1.0)):
        for radius in (1.0, 1.6):
            for disp in (0.0, 0.15):
                params = (radius, 0.37, disp, w)
                n, tot, a, b = run_arm(fr, tops[mips], params, (1.0, 1.0, 1.0, 1.0))
                ok = n == 0
                bad += 0 if ok else 1
                print('  mips=%d w=%+.0f radius=%.1f disp=%.2f: %d of %d sample channels differ %s' % (mips, w, radius, disp, n, tot,
                      'PASS' if ok else 'FAIL'))
    # gate X: the edge rule, per-sample lod
    n, tot, a, b = run_arm(fr, tops[True], (1.0, 0.37, 0.15, 1.0), (1.0, 1.0, 3.0, 3.0))
    okx = n > 0
    print('gate X (edge rule): Bloom Mip +1, fog gain 1,1,3,3 per sample: %d of %d differ (must differ: the combine stands down there) %s'
          % (n, tot, 'PASS' if okx else 'FAIL'))
    err = getError()
    if err:
        print('GL error 0x%x' % err); bad += 1
    if TIME:
        timing()
    verdict = bad == 0 and okx
    print('BLOOMCOMB-CHECK %s%s' % ('PASS' if verdict else 'FAIL', (' (red %s)' % RED) if RED else ''))
    return 0 if verdict else 1


def timing():
    """the view as the tone map writes it (4x MS RGBA8), each sample also reading a 4x MS RGBA32F linear frame (hdrSrc)"""
    global SRC_ON
    W, H = 1920, 1080
    fr = Frame(W, H, 0x8814, 0x8058)
    rng = np.random.default_rng(7)
    top = top_texture(W // 2, H // 2, False, rng)
    bind_unit(1, T2D, top)
    src = U(); genTex(1, ctypes.byref(src)); bindTex(TMS, src); texImageMS(TMS, 4, 0x8814, W, H, 1)
    bind_unit(4, TMS, src)
    SRC_ON = 1
    q = U(); genQ(1, ctypes.byref(q))

    def t(fn):
        finish(); beginQ(0x88BF, q); fn(); endQ(0x88BF)
        v = ctypes.c_uint64(); getQ64(q, 0x8866, ctypes.byref(v))
        return v.value / 1e6
    for disp in (0.0, 0.15):
        params = (1.0, 0.37, disp, 0.0)
        o, n = [], []
        for _ in range(3):
            draw_old(fr, params, (1, 1, 1, 1)); draw_new(fr, params, (1, 1, 1, 1))
        for _ in range(20):
            o.append(t(lambda: draw_old(fr, params, (1, 1, 1, 1))))
            n.append(t(lambda: draw_new(fr, params, (1, 1, 1, 1))))
        print('time 1920x1080 4x dispersion %.2f: OLD tent per sample %.4f ms, NEW combine + fetch %.4f ms (median of 20)'
              % (disp, float(np.median(o)), float(np.median(n))))

    def comb_only(params):
        bindFB(FBO, fr.combfb); viewport(0, 0, fr.W, fr.H)
        uniforms(p_comb, fr, params, (1, 1, 1, 1), False); drawArrays(4, 0, 3)

    def fetch_only(params):
        bind_unit(2, T2D, fr.comb); bindFB(FBO, fr.msfb); viewport(0, 0, fr.W, fr.H)
        uniforms(p_new, fr, params, (1, 1, 1, 1), True); drawArrays(4, 0, 3)

    def bare(params):   # the pass with the tent off (cellIsFoBloomOn false): the frame read + the view write alone
        bindFB(FBO, fr.msfb); viewport(0, 0, fr.W, fr.H)
        uniforms(p_old, fr, params, (1, 1, 1, 1), False); setu(p_old, 'cellIsFoBloomOn', 0); drawArrays(4, 0, 3)
    for disp in (0.0, 0.15):
        params = (1.0, 0.37, disp, 0.0)
        c, f, b = [], [], []
        for _ in range(20):
            c.append(t(lambda: comb_only(params)))
            f.append(t(lambda: fetch_only(params)))
            b.append(t(lambda: bare(params)))
        print('  parts dispersion %.2f: combine pass %.4f, tone pass with one fetch %.4f, tone pass with no bloom %.4f ms'
              % (disp, float(np.median(c)), float(np.median(f)), float(np.median(b))))


if __name__ == '__main__':
    sys.exit(main())
