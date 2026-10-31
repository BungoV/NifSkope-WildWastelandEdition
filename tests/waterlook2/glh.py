"""lane FIX1031B (WATERLOOK1 overhaul): a hidden GL 4.1 context (tests/spells/shader_link_check.py) to run the REAL water
.glsl functions per pixel: compile a fragment source, set uniforms, bind float / uint textures, draw W x H, read RGBA32F.
Each gate in this folder builds its fragment from the tree's (or another tree's) res/shaders files."""
import ctypes
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S  # noqa: E402

S.context()
gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint


def _fn(name, res, *args):   # GL 1.1 entry points come from opengl32 itself, not wglGetProcAddress
    addr = gl.wglGetProcAddress(name.encode())
    if addr:
        return ctypes.WINFUNCTYPE(res, *args)(addr)
    f = getattr(gl, name)
    f.restype, f.argtypes = res, list(args)
    return f


F = ctypes.c_float
createShader = _fn('glCreateShader', GLuint, GLenum)
shaderSource = _fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)
compileShader = _fn('glCompileShader', None, GLuint)
getShaderiv = _fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
getShaderInfoLog = _fn('glGetShaderInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
createProgram = _fn('glCreateProgram', GLuint)
attach = _fn('glAttachShader', None, GLuint, GLuint)
link = _fn('glLinkProgram', None, GLuint)
getProgramiv = _fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
useProgram = _fn('glUseProgram', None, GLuint)
uloc = _fn('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
u1i = _fn('glUniform1i', None, GLint, GLint)
u1f = _fn('glUniform1f', None, GLint, F)
u2f = _fn('glUniform2f', None, GLint, F, F)
u3f = _fn('glUniform3f', None, GLint, F, F, F)
u4f = _fn('glUniform4f', None, GLint, F, F, F, F)
u4fv = _fn('glUniform4fv', None, GLint, ctypes.c_int, ctypes.POINTER(F))
genVA = _fn('glGenVertexArrays', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindVA = _fn('glBindVertexArray', None, GLuint)
genTex = _fn('glGenTextures', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindTex = _fn('glBindTexture', None, GLenum, GLuint)
texImage = _fn('glTexImage2D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, ctypes.c_void_p)
texParam = _fn('glTexParameteri', None, GLenum, GLenum, GLint)
activeTex = _fn('glActiveTexture', None, GLenum)
genFB = _fn('glGenFramebuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindFB = _fn('glBindFramebuffer', None, GLenum, GLuint)
fbTex = _fn('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)
viewport = _fn('glViewport', None, GLint, GLint, ctypes.c_int, ctypes.c_int)
drawArrays = _fn('glDrawArrays', None, GLenum, GLint, ctypes.c_int)
readPixels = _fn('glReadPixels', None, GLint, GLint, ctypes.c_int, ctypes.c_int, GLenum, GLenum, ctypes.c_void_p)
pixelStore = _fn('glPixelStorei', None, GLenum, GLint)

VERT = """#version 410 core
void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }
"""
_va = GLuint()
genVA(1, ctypes.byref(_va))
bindVA(_va)
pixelStore(0x0CF5, 1)   # GL_UNPACK_ALIGNMENT


def glsl(shaders, name):
    """a .glsl file of a shaders dir, includes expanded (the loader's own expansion)"""
    return S.expand(os.path.join(shaders, name))


def _compile(kind, src):
    s = createShader(kind)
    b = ctypes.c_char_p(src.encode('latin-1'))
    shaderSource(s, 1, ctypes.byref(b), None)
    compileShader(s)
    ok = GLint()
    getShaderiv(s, 0x8B81, ctypes.byref(ok))
    if not ok.value:
        log = ctypes.create_string_buffer(1 << 16)
        getShaderInfoLog(s, 1 << 16, None, log)
        print('COMPILE FAIL', log.value.decode(errors='replace')[:3000])
        print('VERDICT FAIL (compile)')
        sys.exit(2)
    return s


class Prog:
    def __init__(self, frag, W, H):
        self.W, self.H = W, H
        self.p = createProgram()
        attach(self.p, _compile(0x8B31, VERT))
        attach(self.p, _compile(0x8B30, frag))
        link(self.p)
        ok = GLint()
        getProgramiv(self.p, 0x8B82, ctypes.byref(ok))
        if not ok.value:
            print('VERDICT FAIL (link)')
            sys.exit(2)
        useProgram(self.p)
        t = GLuint()
        genTex(1, ctypes.byref(t))
        activeTex(0x84C0)
        bindTex(0x0DE1, t)
        texImage(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, None)
        texParam(0x0DE1, 0x2801, 0x2600)
        texParam(0x0DE1, 0x2800, 0x2600)
        fb = GLuint()
        genFB(1, ctypes.byref(fb))
        bindFB(0x8D40, fb)
        fbTex(0x8D40, 0x8CE0, 0x0DE1, t, 0)
        self.unit = 1

    def loc(self, n):
        return uloc(self.p, n.encode())

    def set(self, n, *v):
        lc = self.loc(n)
        if lc < 0:
            return
        if len(v) == 1 and isinstance(v[0], (bool, int)) and not isinstance(v[0], float):
            u1i(lc, int(v[0]))
        elif len(v) == 1:
            u1f(lc, v[0])
        elif len(v) == 2:
            u2f(lc, *v)
        elif len(v) == 3:
            u3f(lc, *v)
        else:
            u4f(lc, *v)

    def set4v(self, n, rows):
        a = (F * (4 * len(rows)))(*[x for r in rows for x in r])
        u4fv(self.loc(n), len(rows), a)

    def tex(self, n, data, kind='f'):
        """data H x W x C: 'f' float (RGBA32F, C 4), 'u' uint (R32UI C 1 / RG32UI C 2); nearest"""
        data = np.ascontiguousarray(data)
        h, w = data.shape[:2]
        c = 1 if data.ndim == 2 else data.shape[2]
        t = GLuint()
        genTex(1, ctypes.byref(t))
        activeTex(0x84C0 + self.unit)
        bindTex(0x0DE1, t)
        if kind == 'f':
            fmt = {1: (0x822E, 0x1903), 2: (0x8230, 0x8227), 4: (0x8814, 0x1908)}[c]
            d = np.ascontiguousarray(data, np.float32)   # held: a temporary's address would be freed before the call
            texImage(0x0DE1, 0, fmt[0], w, h, 0, fmt[1], 0x1406, d.ctypes.data)
        else:
            fmt = {1: (0x8236, 0x8D94), 2: (0x823C, 0x8228)}[c]
            d = data.astype(np.uint32)
            texImage(0x0DE1, 0, fmt[0], w, h, 0, fmt[1], 0x1405, d.ctypes.data)
        texParam(0x0DE1, 0x2801, 0x2600)
        texParam(0x0DE1, 0x2800, 0x2600)
        self.set(n, self.unit)
        self.unit += 1
        activeTex(0x84C0)

    def draw(self):
        useProgram(self.p)
        viewport(0, 0, self.W, self.H)
        drawArrays(4, 0, 3)
        out = np.empty((self.H, self.W, 4), np.float32)
        readPixels(0, 0, self.W, self.H, 0x1908, 0x1406, out.ctypes.data)
        return out


def srgb(lin):
    """display: linear -> sRGB levels (0..255), a plain gamma curve (scale-free hue / saturation use)"""
    return 255.0 * np.clip(lin, 0.0, 1.0) ** (1.0 / 2.2)


def hsv(c):
    """hue (deg) and HSV saturation of display colours (..., 3)"""
    mx, mn = c.max(-1), c.min(-1)
    s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-9), 0.0)
    r, g, b = c[..., 0], c[..., 1], c[..., 2]
    d = np.maximum(mx - mn, 1e-9)
    h = np.where(mx == r, (g - b) / d % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60.0
    return h, s
