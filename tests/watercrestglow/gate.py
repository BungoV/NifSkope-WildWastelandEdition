"""lane FIX1031B: can the crest glow (res/shaders/water_sss.glsl wwWaterCrestGlow) be seen facing a low sun?

bungo's 10-31 sea frames (SHOTS1031B run6 sea2n_all vs sea2n_glow_before = WW_CELL_WATER_OCEAN_RED=noglow, sun 15.8 deg
straight ahead, CommonwealthClear 8:30): MEASURED with the tone curve's inverse (notes fix1031b/water/tools/glow_scale.py)
the glow was 0.0075 of the near water's light (rows 450-1040; mean +0.3 levels, p99 +2.3): it cannot be seen.
This runs the REAL function through the driver (hidden GL 4.1 context, tests/spells/shader_link_check.py), per pixel of
that camera's near band (path log: eye, view, tangents, sun; 1920 x 1045, rows 450-1040), over a sea of Gaussian
slopes (0.12 a axis, Cox-Munk at ~5 m/s), the ocean WATR's body colour (ExtOceanWater shallow 45, 62, 62), sun 1.
The water's light at those pixels is taken from the frame: today's term (gain 0.25 = the red) / 0.0075.
  share    mean glow / the water's light                                in [0.05, 0.15]
  crest    the 90th percentile pixel's glow / the water's light       >= 0.12 (crest faces show a few levels)
VERDICT PASS = both. The red is --red 512 (WW_CELL_WATER_OCEAN_RED=dimglow, the gain of main 5422be6c) and must
FAIL; run.sh also runs main's own water_sss.glsl (--shaders <dir>).

python -I gate.py [--shaders DIR] [--red N]
"""
import argparse
import ctypes
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--shaders', default=os.path.join(TREE, 'res', 'shaders'))
ap.add_argument('--red', type=int, default=0)
a = ap.parse_args()

W, H, R0, R1 = 1920, 1045, 450, 1040
EYE = (71200.0018311, 199.999938965, 760.0)
FWD = (0.788920640945, -0.594262599945, -0.156384885311)
RGT = (-0.601665377617, -0.798748254776, 0.0)
UP = (0.124912157655, -0.0940913707018, 0.987696170807)
LTRAVEL = (-0.769444214763, 0.577853312675, -0.272105033761)
TAN = (0.956448599346, 0.520567051785)
BODY = tuple((c / 255.0) ** 2.2 for c in (45, 62, 62))
SHARE_TODAY = 0.0075   # measured: the frame's glow / the water's light

VERT = """#version 410 core
void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }
"""
FRAG = '#version 410 core\nuniform int waterOceanRed;\n' + S.expand(os.path.join(a.shaders, 'water_sss.glsl')) + r"""
uniform vec3 eye, fwd, rgt, upv, Ldir, body;
uniform vec2 tanXY, vp;
uniform sampler2D slopes;
out vec4 oc;
void main()
{
	vec2 ndc = gl_FragCoord.xy / vp * 2.0 - 1.0;
	vec3 d = normalize( fwd + rgt * ndc.x * tanXY.x + upv * ndc.y * tanXY.y );
	vec2 s = texelFetch( slopes, ivec2( gl_FragCoord.xy ), 0 ).xy;
	oc = vec4( d.z < 0.0 ? wwWaterCrestGlow( s, 0.0, d, Ldir, vec3( 1.0 ), body ).g : -1.0, 0.0, 0.0, 1.0 );
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
useProgram = fn('glUseProgram', None, GLuint)
uloc = fn('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
u1i = fn('glUniform1i', None, GLint, GLint)
u2f = fn('glUniform2f', None, GLint, ctypes.c_float, ctypes.c_float)
u3f = fn('glUniform3f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float)
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
    print('VERDICT FAIL (link)')
    sys.exit(2)
va = GLuint()
genVA(1, ctypes.byref(va))
bindVA(va)


def tex(data, ifmt, fmt):
    t = GLuint()
    genTex(1, ctypes.byref(t))
    bindTex(0x0DE1, t)
    texImage(0x0DE1, 0, ifmt, W, H, 0, fmt, 0x1406, None if data is None else data.ctypes.data)
    texParam(0x0DE1, 0x2801, 0x2600)
    texParam(0x0DE1, 0x2800, 0x2600)
    return t


rng = np.random.default_rng(31)
sl = np.ascontiguousarray(rng.normal(0.0, 0.12, (H, W, 4)).astype(np.float32))
activeTex(0x84C0 + 1)
t_sl = tex(sl, 0x8814, 0x1908)
activeTex(0x84C0)
t_out = tex(None, 0x8814, 0x1908)
fb = GLuint()
genFB(1, ctypes.byref(fb))
bindFB(0x8D40, fb)
fbTex(0x8D40, 0x8CE0, 0x0DE1, t_out, 0)
useProgram(prog)


def U(n):
    return uloc(prog, n.encode())


u3f(U('eye'), *EYE)
u3f(U('fwd'), *FWD)
u3f(U('rgt'), *RGT)
u3f(U('upv'), *UP)
u3f(U('Ldir'), *[-x for x in LTRAVEL])
u3f(U('body'), *BODY)
u2f(U('tanXY'), *TAN)
u2f(U('vp'), float(W), float(H))
u1i(U('slopes'), 1)


def draw(red):
    u1i(U('waterOceanRed'), red)
    viewport(0, 0, W, H)
    drawArrays(4, 0, 3)
    out = np.empty((H, W, 4), np.float32)
    readPixels(0, 0, W, H, 0x1908, 0x1406, out.ctypes.data)
    g = out[::-1, :, 0].astype(np.float64)[R0:R1]
    return g[g >= 0.0]


today = draw(512)
arm = draw(a.red)
water = today.mean() / SHARE_TODAY
share = arm.mean() / water
crest = np.percentile(arm, 90) / water
good = 0.05 <= share <= 0.15 and crest >= 0.12
print('arm: shaders %s, waterOceanRed %d' % (a.shaders, a.red))
print('glow / sun: today (gain of main) mean %.5f; arm mean %.5f p90 %.5f p99 %.5f' % (
    today.mean(), arm.mean(), np.percentile(arm, 90), np.percentile(arm, 99)))
print('share of the water\'s light: mean %.4f (in 0.05..0.15) p90 %.4f (>= 0.12) p99 %.4f' % (
    share, crest, np.percentile(arm, 99) / water))
print('VERDICT %s' % ('PASS' if good else 'FAIL'))
sys.exit(0 if good else 1)
