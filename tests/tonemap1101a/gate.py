"""lane FIX1101A area T (LATE ITEM 26): is the cell view's tone curve Fusion Shaders' Physical operator?

Runs the REAL shader text through the driver in a hidden GL 4.1 core context (tests/spells/shader_link_check.py), no
NifSkope: res/shaders/cell_tonemap.glsl + cellImageSpace() as cell_lights.glsl has it (taken from the file) + cellCurveW /
cellCurveInv as cell_hdr.frag has them, and compares with physical.py (a numpy port of FO4CS res/Post/TonemapCS.hlsl,
SDR mode, FO4CS's default constants).
Inputs (exposed scene-linear Rec.709, 1.0 = paper white): a grey ramp 0..64 (2048 steps, half linear, half log), the 24
Macbeth-ish patches (ColorChecker sRGB 8-bit values, linearised) at 1/16..64 x, and saturated ramps (R, G, B, orange,
cyan, magenta) 0..64.
  A  cellImageSpace with grade + LUT off (cellIsRed 5), exposure 1: display (after the 1 / 2.2) vs physical^(1/2.2)
     max error < 1/255 (and the linear one printed)
  B  the same with the exposure at 0.37 and the input / 0.37 (the exposure stays before the curve): max < 1/255
  C  cell_hdr.frag cellCurveW(2 e) vs the Physical grey axis < 1/255 (display-linear), and cellCurveInv(cellCurveW(2 e))
     / 2 == e within 1e-3 relative for e in 0.02..1.2 (the curve's unclipped part)
VERDICT PASS = A, B and C pass. Reds that must FAIL: --red vanilla (cellIsVanCurve: WW_TONEMAP_RED=vanilla) and the
base's own shader files (run.sh: --shaders <git archive of the base>).

python -I gate.py [--shaders DIR] [--red vanilla]
"""
import argparse
import ctypes
import os
import re
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
sys.path.insert(0, HERE)
import shader_link_check as S  # noqa: E402
import physical as P  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--shaders', default=os.path.join(TREE, 'res', 'shaders'))
ap.add_argument('--red', default='')
ap.add_argument('--cost', action='store_true')
a = ap.parse_args()

# ---- the inputs
MACBETH = [(115, 82, 68), (194, 150, 130), (98, 122, 157), (87, 108, 67), (133, 128, 177), (103, 189, 170),
           (214, 126, 44), (80, 91, 166), (193, 90, 99), (94, 60, 108), (157, 188, 64), (224, 163, 46),
           (56, 61, 150), (70, 148, 73), (175, 54, 60), (231, 199, 31), (187, 86, 149), (8, 133, 161),
           (243, 243, 242), (200, 200, 200), (160, 160, 160), (122, 122, 121), (85, 85, 85), (52, 52, 52)]


def srgb_lin(v):
    v = np.asarray(v, np.float64) / 255.0
    return np.where(v <= 0.04045, v / 12.92, ((v + 0.055) / 1.055) ** 2.4)


rows = []
g = np.concatenate([np.linspace(0.0, 2.0, 1024), np.geomspace(2.0, 64.0, 1024)])
rows += [(v, v, v) for v in g]
mac = srgb_lin(MACBETH)
for s in (1 / 16, 1 / 4, 1 / 2, 1, 2, 4, 8, 16, 64):
    rows += [tuple(p * s) for p in mac]
for hue in ((1, 0, 0), (0, 1, 0), (0, 0, 1), (1, 0.5, 0.05), (0.05, 0.8, 1), (1, 0.1, 0.8)):
    for v in np.geomspace(0.01, 64.0, 256):
        rows.append(tuple(np.array(hue) * v))
X = np.array(rows, np.float64)
N = len(X)
W = 256
H = (N + W - 1) // W
pad = np.zeros((W * H, 3), np.float64)
pad[:N] = X

# ---- the shader text
lights = S.expand(os.path.join(a.shaders, 'features', 'lights', 'cell_lights.glsl'))
lines = lights.split('\n')
i0 = [i for i, l in enumerate(lines) if l.startswith('vec3 cellImageSpace( vec3 sqrtColor )')][0]
i1 = [i for i, l in enumerate(lines) if i > i0 and l.rstrip('\r') == '}'][0]
isFn = '\n'.join(lines[i0:i1 + 1])
tm_path = os.path.join(a.shaders, 'cell_tonemap.glsl')
tm = S.expand(tm_path) if os.path.exists(tm_path) else ''
hdr = S.expand(os.path.join(a.shaders, 'cell_hdr.frag')).split('\n')
j0 = [i for i, l in enumerate(hdr) if l.startswith('float cellCurveW( float x )')][0]
j1 = [i for i, l in enumerate(hdr) if l.startswith('vec3 cellGlowShoulder(')][0]
curveFns = '\n'.join(hdr[j0:j1])
curveFns = curveFns[:curveFns.rfind('/*')]   # drop the shoulder's comment block

VERT = """#version 410 core
void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }
"""
FRAG = """#version 410 core
uniform bool cellIsOn;
uniform float cellIsExposure;
uniform float cellIsE;
uniform float cellIsAdapted;
uniform vec3 cellIsCine;
uniform vec4 cellIsTint;
uniform bool cellIsLutOn;
uniform sampler3D cellIsLut;
uniform int cellIsRed;
uniform bool cellIsVanCurve;
uniform bool cellIsBloomOn;
uniform sampler2D cellIsBloom;
uniform vec4 cellIsBloomRect;
uniform vec3 cellIsWbD;
""" + tm + '\n' + isFn + '\n' + curveFns + r"""
uniform sampler2D src;
uniform int mode;
out vec4 oc;
void main()
{
	vec3 c = texelFetch( src, ivec2( gl_FragCoord.xy ), 0 ).rgb;
	if ( mode == 0 )
		oc = vec4( cellImageSpace( sqrt( c ) ), 1.0 );
	else {
		float w = cellCurveW( 2.0 * c.r );
		oc = vec4( w, cellCurveInv( w ) * 0.5, 0.0, 1.0 );
	}
}
"""

S.context()
gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint


def fn(name, res, *args):   # GL 1.1 entry points come from opengl32 itself, not wglGetProcAddress
    addr = gl.wglGetProcAddress(name.encode())
    if addr:
        return ctypes.WINFUNCTYPE(res, *args)(addr)
    f = getattr(gl, name)
    f.restype, f.argtypes = res, list(args)
    return f


createShader = fn('glCreateShader', GLuint, GLenum)
shaderSource = fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)
compileShader = fn('glCompileShader', None, GLuint)
getShaderiv = fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
getShaderInfoLog = fn('glGetShaderInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
createProgram = fn('glCreateProgram', GLuint)
attach = fn('glAttachShader', None, GLuint, GLuint)
link = fn('glLinkProgram', None, GLuint)
getProgramiv = fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
getProgramInfoLog = fn('glGetProgramInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
useProgram = fn('glUseProgram', None, GLuint)
uloc = fn('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
u1i = fn('glUniform1i', None, GLint, GLint)
u1f = fn('glUniform1f', None, GLint, ctypes.c_float)
u3f = fn('glUniform3f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float)
u4f = fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
genVA = fn('glGenVertexArrays', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindVA = fn('glBindVertexArray', None, GLuint)
genTex = fn('glGenTextures', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindTex = fn('glBindTexture', None, GLenum, GLuint)
texImage = fn('glTexImage2D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, ctypes.c_void_p)
texParam = fn('glTexParameteri', None, GLenum, GLenum, GLint)
activeTex = fn('glActiveTexture', None, GLenum)
genFB = fn('glGenFramebuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindFB = fn('glBindFramebuffer', None, GLenum, GLuint)
fbTex = fn('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)
viewport = fn('glViewport', None, GLint, GLint, ctypes.c_int, ctypes.c_int)
drawArrays = fn('glDrawArrays', None, GLenum, GLint, ctypes.c_int)
readPixels = fn('glReadPixels', None, GLint, GLint, ctypes.c_int, ctypes.c_int, GLenum, GLenum, ctypes.c_void_p)


def compile_(kind, src):
    s = createShader(kind)
    b = ctypes.c_char_p(src.encode('latin-1'))
    shaderSource(s, 1, ctypes.byref(b), None)
    compileShader(s)
    ok = GLint()
    getShaderiv(s, 0x8B81, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16)
        getShaderInfoLog(s, 1 << 16, None, log)
        print('COMPILE FAIL', log.value.decode(errors='replace')[:2000])
        print('VERDICT FAIL (compile)')
        sys.exit(2)
    return s


prog = createProgram()
attach(prog, compile_(0x8B31, VERT))
attach(prog, compile_(0x8B30, FRAG))
link(prog)
ok = GLint()
getProgramiv(prog, 0x8B82, ctypes.byref(ok))
if not ok.value:
    log = ctypes.create_string_buffer(1 << 16)
    getProgramInfoLog(prog, 1 << 16, None, log)
    print('LINK FAIL', log.value.decode(errors='replace')[:2000])
    print('VERDICT FAIL (link)')
    sys.exit(2)
va = GLuint()
genVA(1, ctypes.byref(va))
bindVA(va)
dst = GLuint()
genTex(1, ctypes.byref(dst))
bindTex(0x0DE1, dst)
texImage(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, None)    # RGBA32F
fb = GLuint()
genFB(1, ctypes.byref(fb))
bindFB(0x8D40, fb)
fbTex(0x8D40, 0x8CE0, 0x0DE1, dst, 0)
srcT = GLuint()
genTex(1, ctypes.byref(srcT))
activeTex(0x84C0 + 1)
bindTex(0x0DE1, srcT)
texParam(0x0DE1, 0x2801, 0x2600)    # nearest
texParam(0x0DE1, 0x2800, 0x2600)
useProgram(prog)


def U(name):
    return uloc(prog, name.encode())


u1i(U('src'), 1)
u1i(U('cellIsOn'), 1)
u1f(U('cellIsE'), 0.0)
u1f(U('cellIsAdapted'), 0.18)
u3f(U('cellIsCine'), 1.0, 1.0, 1.0)
u4f(U('cellIsTint'), 0.0, 1.0, 1.0, 1.0)
u1i(U('cellIsLutOn'), 0)
u1i(U('cellIsBloomOn'), 0)
u1i(U('cellIsVanCurve'), 1 if a.red == 'vanilla' else 0)


def run(inp, mode, exposure, red):
    data = np.zeros((H, W, 4), np.float32)
    data.reshape(-1, 4)[:, :3] = inp
    data = np.ascontiguousarray(data)
    texImage(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, data.ctypes.data)
    u1i(U('mode'), mode)
    u1f(U('cellIsExposure'), exposure)
    u1i(U('cellIsRed'), red)
    viewport(0, 0, W, H)
    drawArrays(4, 0, 3)
    out = np.empty((H, W, 4), np.float32)
    readPixels(0, 0, W, H, 0x1908, 0x1406, out.ctypes.data)
    return out.reshape(-1, 4)[:N].astype(np.float64)


ref = P.physical(X)
refD = ref ** (1 / 2.2)
ok = True
print('inputs: %d (%d grey ramp 0..64, %d patches, %d saturated)' % (N, len(g), 24 * 9, 6 * 256))
gA = run(pad, 0, 1.0, 5)[:, :3]
eA = np.abs(gA - refD).max(1)
eAl = np.abs(np.maximum(gA, 0) ** 2.2 - ref).max(1)
worst = int(np.argmax(eA))
passA = eA.max() < 1 / 255
ok &= passA
print('A  display max %.6f (%.3f / 255) at in %s: glsl %s ref %s; linear max %.6f; grey part %.6f, patches %.6f, '
      'saturated %.6f  %s' % (eA.max(), eA.max() * 255, np.round(X[worst], 4), np.round(gA[worst], 4),
                              np.round(refD[worst], 4), eAl.max(), eA[:len(g)].max(), eA[len(g):len(g) + 216].max(),
                              eA[len(g) + 216:].max(), 'ok' if passA else 'FAIL'))
gB = run(pad / 0.37, 0, 0.37, 5)[:, :3]
eB = np.abs(gB - refD).max(1)
passB = eB.max() < 1 / 255
ok &= passB
print('B  exposure 0.37, input / 0.37: display max %.6f (%.3f / 255)  %s' % (eB.max(), eB.max() * 255, 'ok' if passB else 'FAIL'))
gC = run(pad, 1, 1.0, 5)
grey = X[:len(g), 0]
wRef = ref[:len(g), 0]
eC = np.abs(gC[:len(g), 0] - wRef).max()
sel = (grey >= 0.02) & (grey <= 1.2)
rt = np.abs(gC[:len(g), 1][sel] - grey[sel]) / grey[sel]
passC = eC < 1 / 255 and rt.max() < 1e-3
ok &= passC
print('C  cellCurveW vs the grey axis max %.6f (%.3f / 255); inverse round trip max rel %.2e over %d greys  %s'
      % (eC, eC * 255, rt.max(), sel.sum(), 'ok' if passC else 'FAIL'))
# the shapes, for the record: display level of the mid grey, paper white and 4 x paper white
for e in (0.18, 1.0, 4.0):
    k = int(np.argmin(np.abs(grey - e)))
    print('   grey %.3f -> display %.4f (vanilla curve %.4f)' % (grey[k], gA[k, 0], P.vanilla(grey[k]) ** (1 / 2.2)))
print('VERDICT %s' % ('PASS' if ok else 'FAIL'))

if a.cost:  # GPU time of the whole cellImageSpace at 1920 x 1080 (grade on, no LUT), Physical vs the filmic curve
    import time
    finish = fn('glFinish', None)
    CW, CH = 1920, 1080
    big = np.ascontiguousarray(np.random.default_rng(1).random((CH, CW, 4), np.float32) * 4.0)
    texImage(0x0DE1, 0, 0x8814, CW, CH, 0, 0x1908, 0x1406, big.ctypes.data)   # unit 1: the source
    activeTex(0x84C0)
    bindTex(0x0DE1, dst)
    texImage(0x0DE1, 0, 0x8814, CW, CH, 0, 0x1908, 0x1406, None)    # the target, grown (still attached)
    bindTex(0x0DE1, 0)
    activeTex(0x84C0 + 1)
    u1i(U('mode'), 0)
    u1i(U('cellIsRed'), 1)
    u1f(U('cellIsExposure'), 1.0)
    viewport(0, 0, CW, CH)
    res = {}
    for rep in range(3):
        for van in (0, 1):
            u1i(U('cellIsVanCurve'), van)
            for _ in range(20):
                drawArrays(4, 0, 3)
            finish()
            t0 = time.perf_counter()
            for _ in range(200):
                drawArrays(4, 0, 3)
            finish()
            res.setdefault(van, []).append((time.perf_counter() - t0) / 200 * 1000.0)
    print('COST ms a 1920x1080 pass (min of 3 x 200 draws, RGBA32F in/out): physical %.4f  vanilla %.4f  delta %.4f'
          % (min(res[0]), min(res[1]), min(res[0]) - min(res[1])))
