"""lane FIX1031B: do the wind's waves on a river move at the river's speed? (res/shaders/water_flowwaves.glsl)

bungo's 10-31 river frames (SHOTS1031B run4c rvc_flow_before, run6 rvc2_flow_before = WW_CELL_WATER_OCEAN_RED=noflow)
showed no flow: the baked flow at those cameras is 0.36-0.79 bake units (notes fix1031b/water/tools/flow_cells.py over
the run's water census; the whole bake's median is 0.397, WATERFLOWBAKE1), and the look read it as u/s: the waves
moved 6 mm/s, 1.6 u in a 4 s period. The bake's numbers are the WATR NAM0 linear velocity, which the game's physics
(metres) takes: 0.397 m/s = 27.8 u/s.
The REAL wwWaterOceanFlowed() runs through the driver (hidden GL 4.1 context) with the rest of the ocean stubbed: the
flow field is the bake's median (0.397, along x), read A alone moved 2 s (waterOceanFlow = 2, 2, 1, 0), and the stub
spectrum returns the read's offset as its slope, so the function's result IS the distance the flow carried the waves.
  speed    the carried distance / 2 s, in u/s, for the bake's median      in [15, 60] u/s (a river, 0.2-0.85 m/s)
  still    the same number on a lake / the sea body (the stub wwWaterInfo class 2 / 0)   <= 1 u/s (as before)
VERDICT PASS = all three. The red is --red 1024 (WW_CELL_WATER_OCEAN_RED=rawflow, main 5422be6c's reading) and must
FAIL; run.sh also runs main's own water_flowwaves.glsl (--shaders <dir>).

python -I gate.py [--shaders DIR] [--red N]
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
import shader_link_check as S  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--shaders', default=os.path.join(TREE, 'res', 'shaders'))
ap.add_argument('--red', type=int, default=0)
a = ap.parse_args()

MEDIAN = 0.397   # the bake's median river flow (WATERFLOWBAKE1 RESULTS)
ocean = S.expand(os.path.join(a.shaders, 'water_ocean.glsl'))
struct = re.search(r'struct WwOcean\s*\{.*?\};', ocean, re.S).group(0)
metre = re.search(r'const float kOcMetre = [^;]+;', ocean).group(0)
STUB = '#version 410 core\n' + metre + '\n' + struct + r"""
uniform int waterOceanRed;
uniform vec4 waterOceanInvL[2];
uniform vec4 waterOceanWind;
uniform vec4 waterFlowGrid;	// origin x, y, texel (u), scale
uniform vec2 flowV;	// the stub flow field (bake units)
uniform float bodyClass;	// the stub body class: 0 sea, 1 river, 2 lake
vec2 wwWaterFlow( vec2 worldXY ) { return flowV; }
vec4 wwWaterInfo( vec2 worldXY ) { return vec4( 8160.0, 256.0, 1.0, bodyClass ); }
WwOcean wwWaterOcean( vec2 P, vec2 dPx, vec2 dPy ) { return WwOcean( vec2( 0.0 ), 0.0, 1.0, 0.0, 1.0 ); }
// the stub spectrum: its slope is the read's offset from the pixel (u)
WwOcean wwWaterOceanQ( vec2 P, vec2 Q, vec2 dPx, vec2 dPy, out int setOut ) { setOut = 1; return WwOcean( Q - P, 0.0, 1.0, 0.0, 1.0 ); }
"""
MAIN = r"""
out vec4 oc;
void main()
{
	WwOcean o = wwWaterOceanFlowed( vec2( 1000.3, 2000.7 ), vec2( 1.0, 0.0 ), vec2( 0.0, 1.0 ) );
	oc = vec4( o.slope, 0.0, 1.0 );
}
"""
FRAG = STUB + S.expand(os.path.join(a.shaders, 'water_flowwaves.glsl')) + MAIN
VERT = """#version 410 core
void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }
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
u4f = fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
genVA = fn('glGenVertexArrays', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindVA = fn('glBindVertexArray', None, GLuint)
genTex = fn('glGenTextures', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindTex = fn('glBindTexture', None, GLenum, GLuint)
texImage = fn('glTexImage2D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, ctypes.c_void_p)
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
tex = GLuint()
genTex(1, ctypes.byref(tex))
bindTex(0x0DE1, tex)
texImage(0x0DE1, 0, 0x8814, 4, 4, 0, 0x1908, 0x1406, None)
fb = GLuint()
genFB(1, ctypes.byref(fb))
bindFB(0x8D40, fb)
fbTex(0x8D40, 0x8CE0, 0x0DE1, tex, 0)
useProgram(prog)


def U(n):
    return uloc(prog, n.encode())


u1i(U('waterOceanRed'), a.red)
u4f(U('waterOceanFlow'), 2.0, 2.0, 1.0, 0.0)
u4f(U('waterFlowGrid'), 0.0, 0.0, 128.0, 1.0)
u4f(U('waterOceanWind'), 1.0, 0.0, 5.0, 0.0)
u2f(U('flowV'), MEDIAN, 0.0)
print('arm: shaders %s, waterOceanRed %d' % (a.shaders, a.red))
sp = {}
for cls, name in ((1.0, 'river'), (2.0, 'lake'), (0.0, 'sea')):
    fn('glUniform1f', None, GLint, ctypes.c_float)(U('bodyClass'), cls)
    viewport(0, 0, 4, 4)
    drawArrays(4, 0, 3)
    out = np.empty((4, 4, 4), np.float32)
    readPixels(0, 0, 4, 4, 0x1908, 0x1406, out.ctypes.data)
    off = out[0, 0, :2].astype(np.float64)
    sp[name] = float(np.hypot(*off)) / 2.0
    print('%-5s: the flow %.3f -> the waves carried %.2f u/s (%.3f m/s); offset over 2 s (%.2f, %.2f) u' % (
        name, MEDIAN, sp[name], sp[name] * 0.0142875, off[0], off[1]))
# rivers carry their waves at the river's speed; a lake's / the sea's constant NAM0 keeps the reading before (<= 1 u/s)
good = 15.0 <= sp['river'] <= 60.0 and sp['lake'] <= 1.0 and sp['sea'] <= 1.0
print('VERDICT %s' % ('PASS' if good else 'FAIL'))
sys.exit(0 if good else 1)
