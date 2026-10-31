"""lane FIX1031B (WATERGLINT2): does the near water's sun path keep its shown brightness with the glitter on?

bungo's 10-31 frames (SHOTS1031B sheet_W6fault): with WATERGLINT1's glitter the near sun path lost its glare
(river box 88.9 levels today vs 123.2 with WW_CELL_WATER_GLINT_RED=noglitter; sea 75.2 vs 160.3) and showed no specks.
This runs the REAL glare code through the driver in a hidden GL 4.1 context (tests/spells/shader_link_check.py), no
NifSkope: res/shaders/water_refl.glsl + water_glitter.glsl, and the lines of fo4_water.frag that make the glare
(between `if ( waterGlintOn ...` and `glare = sun * glX;`, taken from the file as it is), per pixel of the two cameras
of those frames (eye, view, tangents and sun from the runs' path logs; 1920 x 1045), over a flat water plane at two
clearances, three GGX roughnesses, small wave slopes.
Per case, over the frame's sun-path box (pixels the noglitter glare reaches):
  lin      mean glare (arm) / mean glare (noglitter)                          within 3%
  shown    the box's mean display level: the bed at the frame's measured level, the glare scaled so the noglitter box
           shows the frame's measured level, through the cell view's tone curve (cell_lights.glsl, E 0, grade and LUT
           left out); arm / noglitter                                          within 3%
  specks   share of box pixels at >= 1.5 x the noglitter glare, and at <= 0.5 x  each >= 10%
VERDICT PASS = every case passes. The red is --red 128 (WW_CELL_WATER_GLINT_RED=sparse: the count and order of
WATERGLINT1, main 5422be6c) and must FAIL; run.sh also runs main's own shader files (--shaders <dir>).

python -I gate.py [--shaders DIR] [--red N] [--lammin X (patch kWwGlitLamMin, tuning only)]
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
ap.add_argument('--lammin', type=float, default=-1.0)
a = ap.parse_args()

W, H = 1920, 1045
# the runs' path logs (last frame): cam=eye|fwd|tanX,tanY, camrows=right|up, L = the light's travel direction
CAMS = {
    'river': dict(  # run4c rvc_*: river_detail.path, CommonwealthClear 12:00, sun 33.5 deg ahead
        eye=(-27000.0014038, -70300.0070801, 1559.99951172), fwd=(-0.241538673639, -0.966154694557, -0.0905769988894),
        rgt=(-0.970142483711, 0.242535620928, 0.0), up=(-0.0219681505114, -0.0878726020455, 0.995889484882),
        L=(0.0, 0.833520237803, -0.552488925838), box=(700, 1040, 450, 840), bed=88.9, shown=123.2),
    'sea': dict(  # run6 sea2n_*: sea_near_ese.path, CommonwealthClear 8:30, sun 15.8 deg ahead
        eye=(71200.0018311, 199.999938965, 760.0), fwd=(0.788920640945, -0.594262599945, -0.156384885311),
        rgt=(-0.601665377617, -0.798748254776, 0.0), up=(0.124912157655, -0.0940913707018, 0.987696170807),
        L=(-0.769444214763, 0.577853312675, -0.272105033761), box=(428, 1006, 772, 1032), bed=75.2, shown=160.3),
}
TAN = (0.956448599346, 0.520567051785)
CLEAR = (150.0, 300.0)
A2 = (0.01, 0.03, 0.1)

frag_src = S.expand(os.path.join(a.shaders, 'fo4_water.frag'))
lines = frag_src.split('\n')
i0 = [i for i, l in enumerate(lines) if 'if ( waterGlintOn && ( waterGlintRed & 1 ) == 0 && !under )' in l][0]
i1 = [i for i, l in enumerate(lines) if i > i0 and 'glare = sun * glX;' in l][0]
block = '\n'.join(lines[i0 + 1:i1])
glit = S.expand(os.path.join(a.shaders, 'water_glitter.glsl'))
if a.lammin > 0:
    glit, n = re.subn(r'const float kWwGlitLamMin = [0-9.]+;', 'const float kWwGlitLamMin = %r;' % a.lammin, glit)
    assert n == 1, 'no kWwGlitLamMin to patch'

VERT = """#version 410 core
void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }
"""
FRAG = '#version 410 core\nuniform vec4 waterSky[3];\n' + S.expand(os.path.join(a.shaders, 'water_refl.glsl')) + '\n' + glit + r"""
uniform vec3 eye, fwd, rgt, upv, Ldir;
uniform vec2 tanXY, vp;
uniform float wz, reflA2, waveA;
out vec4 oc;
float glareAt( vec3 Ns, vec3 V, vec3 Ldir, float reflA2, vec3 P, float glFoot )
{
""" + block + r"""
	return glX;
}
void main()
{
	vec2 ndc = gl_FragCoord.xy / vp * 2.0 - 1.0;
	vec3 d = normalize( fwd + rgt * ndc.x * tanXY.x + upv * ndc.y * tanXY.y );
	if ( d.z > -1e-4 ) {
		oc = vec4( -1.0 );
		return;
	}
	vec3 P = eye + d * ( ( wz - eye.z ) / d.z );
	vec2 s = waveA * vec2( sin( P.x * 0.37 + 1.3 * sin( P.y * 0.11 ) ) + 0.6 * sin( P.y * 0.83 - P.x * 0.21 ),
		cos( P.y * 0.41 + 1.1 * sin( P.x * 0.13 ) ) + 0.6 * cos( P.x * 0.77 + P.y * 0.29 ) );
	vec3 Ns = normalize( vec3( -s, 1.0 ) );
	float glFoot = length( cross( dFdx( P ), dFdy( P ) ) );
	oc = vec4( glareAt( Ns, d, Ldir, reflA2, P, glFoot ), glFoot, 0.0, 1.0 );
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
u2f = fn('glUniform2f', None, GLint, ctypes.c_float, ctypes.c_float)
u3f = fn('glUniform3f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float)
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
    log = ctypes.create_string_buffer(1 << 16)
    getProgramInfoLog(prog, 1 << 16, None, log)
    print('LINK FAIL', log.value.decode(errors='replace')[:2000])
    print('VERDICT FAIL (link)')
    sys.exit(2)
va = GLuint()
genVA(1, ctypes.byref(va))
bindVA(va)
tex = GLuint()
genTex(1, ctypes.byref(tex))
bindTex(0x0DE1, tex)
texImage(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, None)
fb = GLuint()
genFB(1, ctypes.byref(fb))
bindFB(0x8D40, fb)
fbTex(0x8D40, 0x8CE0, 0x0DE1, tex, 0)
useProgram(prog)


def U(name):
    return uloc(prog, name.encode())


u4f(U('waterGlintK'), 16.0, 0.0204, 0.18, 0.015)   # src/gl/cellwater.cpp
u2f(U('tanXY'), *TAN)
u2f(U('vp'), float(W), float(H))
u1f(U('waveA'), 0.02)


def draw(cam, wz, a2, red):
    u3f(U('eye'), *cam['eye'])
    u3f(U('fwd'), *cam['fwd'])
    u3f(U('rgt'), *cam['rgt'])
    u3f(U('upv'), *cam['up'])
    u3f(U('Ldir'), *[-x for x in cam['L']])
    u1f(U('wz'), wz)
    u1f(U('reflA2'), a2)
    u1i(U('waterGlintRed'), red)
    viewport(0, 0, W, H)
    drawArrays(4, 0, 3)
    out = np.empty((H, W, 4), np.float32)
    readPixels(0, 0, W, H, 0x1908, 0x1406, out.ctypes.data)
    return out[::-1].astype(np.float64)   # row 0 = the picture's top row


def curve(x, E=0.0):
    c = (x * (0.15 * x + 0.05) + 0.2 * E) / (x * (0.15 * x + 0.5) + 0.06) - E / 0.3
    return c / ((0.2 * E + 19.376) * 0.040856 - E / 0.3)


def curve_inv(y, E=0.0):
    yp = y * ((0.2 * E + 19.376) * 0.040856 - E / 0.3) + E / 0.3
    qa, qb, qc = 0.15 * (yp - 1.0), 0.5 * yp - 0.05, 0.06 * yp - 0.2 * E
    return max((-qb - np.sqrt(max(qb * qb - 4 * qa * qc, 0.0))) / (2 * qa), 0.0)


def shown(u):
    return 255.0 * np.clip(curve(u), 0.0, 1.0) ** (1 / 2.2)


fails = []
print('arm: shaders %s, waterGlintRed %d%s' % (a.shaders, a.red, ', kWwGlitLamMin %g' % a.lammin if a.lammin > 0 else ''))
for name, cam in CAMS.items():
    y0, y1, x0, x1 = cam['box']
    ub = curve_inv((cam['bed'] / 255.0) ** 2.2)
    for clear in CLEAR:
        wz = cam['eye'][2] - clear
        for a2 in A2:
            ng = draw(cam, wz, a2, 8)                     # red noglitter: the mean glare alone
            ar = draw(cam, wz, a2, a.red)
            gn, ga = ng[y0:y1, x0:x1, 0], ar[y0:y1, x0:x1, 0]
            m = gn > 0.0
            if m.mean() < 0.5 or gn[m].mean() <= 0.0:
                print('%-5s clear %3.0f a2 %.2f: the glare misses the box (water %.2f)' % (name, clear, a2, m.mean()))
                fails.append(name)
                continue
            gn, ga = np.where(m, gn, 0.0), np.where(m, ga, 0.0)
            lin = ga.mean() / gn.mean()
            lo, hi = 0.0, 1e4                             # glare scale: the noglitter box shows the frame's level
            for _ in range(80):
                s = 0.5 * (lo + hi)
                if shown(ub + s * gn).mean() < cam['shown']:
                    lo = s
                else:
                    hi = s
            ref = shown(ub + s * gn).mean()
            disp = shown(ub + s * ga).mean() / ref
            hiS = (ga[m] >= 1.5 * gn[m]).mean()
            loS = (ga[m] <= 0.5 * gn[m]).mean()
            foot = ar[y0:y1, x0:x1, 1][m]
            good = abs(lin - 1) <= 0.03 and abs(disp - 1) <= 0.03 and hiS >= 0.10 and loS >= 0.10
            print('%-5s clear %3.0f a2 %.2f: lin %.3f shown %.3f (%.1f of %.1f levels) specks >=1.5x %.3f <=0.5x %.3f '
                  '| footprint u^2 p50 %.3f p95 %.3f | %s' % (
                      name, clear, a2, lin, disp, disp * ref, ref, hiS, loS, np.median(foot), np.percentile(foot, 95),
                      'PASS' if good else 'FAIL'))
            if not good:
                fails.append('%s/%g/%g' % (name, clear, a2))
print('VERDICT %s%s' % ('PASS' if not fails else 'FAIL', '' if not fails else ' (%d cases: %s)' % (len(fails), ' '.join(fails))))
sys.exit(0 if not fails else 1)
