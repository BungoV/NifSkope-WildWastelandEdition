"""GIFIX1101 HO (PERFFIX2 2c, 2026-11-02): the SSGI hand-over to the traced GI volume, offline on the REAL law.

    python -I tests/features/gi/ssgihandover/ssgihandover_gl.py <worktree> [--red nohandover]

No NifSkope. A hidden 4.1 core context (tests/spells/shader_link_check.py's) runs cellSsgiE, cut byte for byte from
res/shaders/features/lights/cell_lights.glsl under WW_CELL_RTDDGI, once a pixel over a row of analytic cases. Each case is a hemisphere
with two occluder classes: a BIG one (share b, a wall the probes resolve: the volume's sky visibility at the normal is
1 - b, its E already darkened and lit by it) and a CONTACT one (share c, under the probe spacing: the volume cannot see
it). The screen march sees q x b + c (q = the big one's share inside the frame) and its bounce is b q Lb + c Lc.
Truth: the volume's E darkened by the contact share only, plus the contact share's bounce:
    eProbe (1 - Kx c) + Ky c Lc
Today's law (the red WW_CELL_SSGI_RED=nohandover, 8192) darkens by q b + c and adds the big one's bounce a second time.
Bars (set before the first run):
  A (q = 1, Lb = Lc): rel err max <= 1e-4; and the red's median >= 0.05 (the set holds double darkening at all);
  B (q in 0.6..1, Lb != Lc, K random): mean rel err <= 0.5 x the red's mean;
  I (identity: interior, volume off, no sky mode, fade 0, red stack, contact only b = 0): equal to today's law, rel <= 1e-6.
Exit 0 = PASS. With --red nohandover the A bar must FAIL.
"""
import ctypes, os, sys, random, struct
tree = sys.argv[1]
args = sys.argv[2:]
RED = args[args.index('--red') + 1] if '--red' in args else ''
sys.path.insert(0, os.path.join(tree, 'tests', 'spells'))
import shader_link_check as slc

src = open(os.path.join(tree, 'res', 'shaders', 'features', 'lights', 'cell_lights.glsl'), 'rb').read().decode('latin-1').replace('\r\n', '\n')
def cut(s, sig):
    i = s.index(sig); j = s.index('\n}', i); return s[i:j + 2] + '\n'
law = cut(src, 'vec3 cellSsgiE( vec3 eProbe )\n{')

slc.context()
gl = slc.gl
def fn(name, res, *a):
    addr = gl.wglGetProcAddress(name.encode())
    if addr and addr not in (1, 2, 3, -1, 0xFFFFFFFF):
        return ctypes.WINFUNCTYPE(res, *a)(addr)
    f = getattr(gl, name); f.restype = res; f.argtypes = list(a); return f
GLuint, GLint, GLenum, GLsizei, GLf = ctypes.c_uint, ctypes.c_int, ctypes.c_uint, ctypes.c_int, ctypes.c_float
Pt = ctypes.POINTER
createShader = fn('glCreateShader', GLuint, GLenum)
shaderSource = fn('glShaderSource', None, GLuint, ctypes.c_int, Pt(ctypes.c_char_p), ctypes.c_void_p)
compileShader = fn('glCompileShader', None, GLuint)
getShaderiv = fn('glGetShaderiv', None, GLuint, GLenum, Pt(GLint))
getShaderInfoLog = fn('glGetShaderInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
createProgram = fn('glCreateProgram', GLuint)
attach = fn('glAttachShader', None, GLuint, GLuint)
link = fn('glLinkProgram', None, GLuint)
getProgramiv = fn('glGetProgramiv', None, GLuint, GLenum, Pt(GLint))
getProgramInfoLog = fn('glGetProgramInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
bindAttrib = fn('glBindAttribLocation', None, GLuint, GLuint, ctypes.c_char_p)
useProgram = fn('glUseProgram', None, GLuint)
uloc = fn('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
u1i = fn('glUniform1i', None, GLint, GLint)
u2f = fn('glUniform2f', None, GLint, GLf, GLf)
u4f = fn('glUniform4f', None, GLint, GLf, GLf, GLf, GLf)
genBuffers = fn('glGenBuffers', None, GLsizei, Pt(GLuint))
bindBuffer = fn('glBindBuffer', None, GLenum, GLuint)
bufferData = fn('glBufferData', None, GLenum, ctypes.c_ssize_t, ctypes.c_void_p, GLenum)
genTextures = fn('glGenTextures', None, GLsizei, Pt(GLuint))
bindTexture = fn('glBindTexture', None, GLenum, GLuint)
activeTexture = fn('glActiveTexture', None, GLenum)
texImage2D = fn('glTexImage2D', None, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, ctypes.c_void_p)
texParameteri = fn('glTexParameteri', None, GLenum, GLenum, GLint)
genFramebuffers = fn('glGenFramebuffers', None, GLsizei, Pt(GLuint))
bindFramebuffer = fn('glBindFramebuffer', None, GLenum, GLuint)
fbTex = fn('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)
checkFb = fn('glCheckFramebufferStatus', GLenum, GLenum)
viewport = fn('glViewport', None, GLint, GLint, GLsizei, GLsizei)
genVA = fn('glGenVertexArrays', None, GLsizei, Pt(GLuint))
bindVA = fn('glBindVertexArray', None, GLuint)
vap = fn('glVertexAttribPointer', None, GLuint, GLint, GLenum, ctypes.c_ubyte, GLsizei, ctypes.c_void_p)
eva = fn('glEnableVertexAttribArray', None, GLuint)
drawArrays = fn('glDrawArrays', None, GLenum, GLint, GLsizei)
readPixels = fn('glReadPixels', None, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, ctypes.c_void_p)

def compile_prog(vs, fs):
    p = createProgram()
    for s_, kind in ((vs, 0x8B31), (fs, 0x8B30)):
        s = createShader(kind)
        b = ctypes.c_char_p(s_.encode('latin-1'))
        shaderSource(s, 1, ctypes.byref(b), None); compileShader(s)
        ok = GLint(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(s, 1 << 16, None, log)
            print('COMPILE FAIL'); print(log.value.decode(errors='replace')[:3000]); sys.exit(2)
        attach(p, s)
    bindAttrib(p, 0, b'vertexPosition')
    link(p)
    ok = GLint(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16); getProgramInfoLog(p, 1 << 16, None, log)
        print('LINK FAIL'); print(log.value.decode(errors='replace')[:3000]); sys.exit(2)
    return p

VS = """#version 410 core
layout ( location = 0 ) in vec3 vertexPosition;
void main() { gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 ); }
"""
FS = """#version 410 core
#define WW_CELL_RTDDGI 1
bool cellSsgiOn = true;
bool cellDdgiOn = true;
bool cellGiSky = true;
bool cellInterior = false;
vec2 cellGiVisN = vec2( 1.0, 0.0 );
uniform sampler2D cellSsgi;
uniform vec4 cellSsgiRect;
uniform int cellSsgiRed;
uniform sampler2D caseA;	// eProbe rgb, the volume's vis at the normal
uniform sampler2D caseB;	// fade, interior, volume on, sky mode
uniform sampler2D caseK;	// Kx, Ky, red stack
vec2 cellSsgiK;
layout ( location = 0 ) out vec4 o;
%s
void main()
{
	ivec2 t = ivec2( gl_FragCoord.xy );
	vec4 a = texelFetch( caseA, t, 0 ), b = texelFetch( caseB, t, 0 ), k = texelFetch( caseK, t, 0 );
	cellGiVisN = vec2( a.w, b.x );
	cellInterior = b.y > 0.5;
	cellDdgiOn = b.z > 0.5;
	cellGiSky = b.w > 0.5;
	cellSsgiK = k.xy;
	o = vec4( cellSsgiE( a.rgb ), 1.0 );
}
"""
# the red stack rides cellSsgiRed per case: a separate uniform is not possible per pixel, so stack cases run in pass 2
fs = FS % law.replace('cellSsgiRed', 'cellSsgiRedC')
fs = fs.replace('uniform int cellSsgiRed;', 'uniform int cellSsgiRed;\nint cellSsgiRedC = 0;')
fs = fs.replace('	cellSsgiK = k.xy;', '	cellSsgiK = k.xy;\n	cellSsgiRedC = cellSsgiRed | ( k.z > 0.5 ? 2 : 0 );')
prog = compile_prog(VS, fs)

rnd = random.Random(1101)
cases = []   # (set, sA rgb a, eProbe rgb, vis, fade, interior, on, sky, Kx, Ky, stack, truth rgb or None)
def lum(v): return 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2]
def rgb(lo, hi): return [rnd.uniform(lo, hi) for _ in range(3)]
for i in range(400):   # A: the screen sees the whole big occluder, one bounce colour
    b = rnd.uniform(0.02, 0.6); c = rnd.uniform(0.0, min(0.3, 0.98 - b)); L = rgb(0.05, 0.6); e = rgb(0.2, 2.0)
    Kx, Ky = (1.0, 1.0) if i % 2 else (rnd.uniform(0.3, 1.0), rnd.uniform(0.3, 1.0))
    sr = [(b + c) * l for l in L]
    tr = [e[j] * (1 - Kx * c) + Ky * c * L[j] for j in range(3)]
    cases.append(('A', sr + [1 - b - c], e, 1 - b, 1.0, 0, 1, 1, Kx, Ky, 0, tr))
for i in range(400):   # B: part of the big one off screen, its bounce another colour
    b = rnd.uniform(0.02, 0.6); c = rnd.uniform(0.0, min(0.3, 0.98 - b)); q = rnd.uniform(0.6, 1.0)
    Lb, Lc = rgb(0.05, 0.6), rgb(0.05, 0.6); e = rgb(0.2, 2.0)
    Kx, Ky = rnd.uniform(0.3, 1.0), rnd.uniform(0.3, 1.0)
    sr = [q * b * Lb[j] + c * Lc[j] for j in range(3)]
    tr = [e[j] * (1 - Kx * c) + Ky * c * Lc[j] for j in range(3)]
    cases.append(('B', sr + [1 - q * b - c], e, 1 - b, 1.0, 0, 1, 1, Kx, Ky, 0, tr))
for i in range(240):   # I: every route that must keep today's law exactly
    kind = i % 6
    b = rnd.uniform(0.02, 0.6); c = rnd.uniform(0.0, min(0.3, 0.98 - b)); L = rgb(0.05, 0.6); e = rgb(0.2, 2.0)
    Kx, Ky = rnd.uniform(0.3, 1.0), rnd.uniform(0.3, 1.0)
    vis, fade, inter, on, sky, stack = 1 - b, 1.0, 0, 1, 1, 0
    if kind == 0: inter = 1
    elif kind == 1: on = 0
    elif kind == 2: sky = 0
    elif kind == 3: fade = 0.0
    elif kind == 4: stack = 1
    else: vis = 1.0   # the volume sees no occluder: the screen's all is contact
    sr = [(b + c) * l for l in L]
    sa = 1 - b - c
    keep = 1.0 if stack else 1 - Kx * (1 - sa)
    tr = [e[j] * keep + Ky * sr[j] for j in range(3)]
    cases.append(('I%d' % kind, sr + [sa], e, vis, fade, inter, on, sky, Kx, Ky, stack, tr))

N = len(cases)
def tex(unit, data):
    t = GLuint(); genTextures(1, ctypes.byref(t)); activeTexture(0x84C0 + unit); bindTexture(0x0DE1, t)
    buf = (GLf * (4 * N))(*data)
    texImage2D(0x0DE1, 0, 0x8814, N, 1, 0, 0x1908, 0x1406, buf)
    texParameteri(0x0DE1, 0x2801, 0x2600); texParameteri(0x0DE1, 0x2800, 0x2600)
    return t
dS, dA, dB, dK = [], [], [], []
for c in cases:
    dS += c[1]; dA += c[2] + [c[3]]; dB += [c[4], c[5], c[6], c[7]]; dK += [c[8], c[9], c[10], 0.0]
tex(0, dS); tex(1, dA); tex(2, dB); tex(3, dK)
out = GLuint(); genTextures(1, ctypes.byref(out)); activeTexture(0x84C0 + 4); bindTexture(0x0DE1, out)
texImage2D(0x0DE1, 0, 0x8814, N, 1, 0, 0x1908, 0x1406, None)
fbo = GLuint(); genFramebuffers(1, ctypes.byref(fbo)); bindFramebuffer(0x8D40, fbo)
fbTex(0x8D40, 0x8CE0, 0x0DE1, out, 0)
assert checkFb(0x8D40) == 0x8CD5
vao = GLuint(); genVA(1, ctypes.byref(vao)); bindVA(vao)
vb = GLuint(); genBuffers(1, ctypes.byref(vb)); bindBuffer(0x8892, vb)
quad = (GLf * 18)(-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, -1, 0, 1, 1, 0, -1, 1, 0)
bufferData(0x8892, ctypes.sizeof(quad), quad, 0x88E4)
vap(0, 3, 0x1406, 0, 0, None); eva(0)
useProgram(prog)
for n_, u in (('cellSsgi', 0), ('caseA', 1), ('caseB', 2), ('caseK', 3)):
    u1i(uloc(prog, n_.encode()), u)
u4f(uloc(prog, b'cellSsgiRect'), 0.0, 0.0, 1.0 / N, 1.0)
u1i(uloc(prog, b'cellSsgiRed'), 8192 if RED == 'nohandover' else 0)
viewport(0, 0, N, 1)
drawArrays(0x0004, 0, 6)
res = (GLf * (4 * N))()
readPixels(0, 0, N, 1, 0x1908, 0x1406, res)

def today(c):
    sa = c[1][3]; Kx, Ky = c[8], c[9]
    keep = 1.0 if c[10] else 1 - Kx * (1 - sa)
    return [c[2][j] * keep + Ky * c[1][j] for j in range(3)]
def rel(a, b): return abs(lum(a) - lum(b)) / max(lum(b), 1e-6)
errs, reds = {}, {}
for i, c in enumerate(cases):
    got = [res[4 * i + j] for j in range(3)]
    errs.setdefault(c[0][:1] if c[0][0] != 'I' else 'I', []).append(rel(got, c[11]))
    reds.setdefault(c[0][:1] if c[0][0] != 'I' else 'I', []).append(rel(today(c), c[11]))
def med(v): v = sorted(v); return v[len(v) // 2]
ok = True
eA, rA = errs['A'], reds['A']
print('A  law max %.2e median %.2e | today median %.4f max %.4f' % (max(eA), med(eA), med(rA), max(rA)))
if max(eA) > 1e-4: print('A FAIL: law max > 1e-4'); ok = False
if med(rA) < 0.05: print('A FAIL: the set holds no double darkening (today median < 0.05)'); ok = False
eB, rB = errs['B'], reds['B']
mB, mR = sum(eB) / len(eB), sum(rB) / len(rB)
print('B  law mean %.4f median %.4f p99 %.4f | today mean %.4f median %.4f | ratio %.3f' % (
    mB, med(eB), sorted(eB)[int(0.99 * len(eB))], mR, med(rB), mB / mR))
if mB > 0.5 * mR: print('B FAIL: mean > 0.5 x today'); ok = False
eI = errs['I']
print('I  identity max rel %.2e (%d cases)' % (max(eI), len(eI)))
if max(eI) > 1e-6: print('I FAIL: a route that keeps today\'s law moved'); ok = False
print('red %s: %s' % (RED or 'none', 'PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
