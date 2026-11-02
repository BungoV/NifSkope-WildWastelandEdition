"""lane RTREFLUNIT1: the water's read of the ray-traced reflection hits (res/shaders/water_rtrefl.glsl, the file
fo4_water.frag includes under WW_CELL_RTREFL) on the real driver in a hidden GL 4.1 context. Runs no NifSkope.

    python -I tests/rtreflunit/rtreflunit_gate.py <shaders dir> [--red texel|flip|row|gamma|weight|off|under]

A fragment pass over viewports of even and odd sizes (one with an offset), unbent and bent (a per-pixel bend in the
march's top-row-first uv, as wwWaterSsrBend gives), reads a hit buffer the size src/rtddgicell.cpp binds
(reflHalf(W) x reflHalf(H) RGBA32F texels, bottom row first). The twin is the producer's own rule (src/rtddgi/refl.comp
reflPick: half texel (hx, hy) = full pixels 2hx..2hx+1 x 2hy..2hy+1). Green, every viewport:
  texel     the texel read == the full pixel's block (fp / 2), 0 off (bend points within 1e-3 px of a pixel edge are
            left out: under 0.5 %);
  units     a hit's rgb comes out bit for bit (HDR 1e-4..1e4, no tone, no sky visibility), its distance never leaks;
  fallback  a miss (a = 0), waterRtOn false, and under the surface keep the sky's bits.
Floors: >= 1000 hits and >= 200 misses a viewport, two odd-size viewports. Reds (each must FAIL): texel = the upload
WW_CELL_RTREFL_TEXEL_RED=1 gives (full size 0 0: RTREFL1's half-size scale), and doctored copies of the file:
flip = the bend's y not turned to bottom row first, row = the index transposed, gamma = rgb gamma-encoded, weight = the
distance taken as the blend weight, off = waterRtOn ignored, under = the under test dropped."""
import ctypes, importlib.util, os, sys
import numpy as np

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('slc', os.path.join(here, '..', 'spells', 'shader_link_check.py'))
slc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(slc)
gl = slc.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint

MUTANTS = {
    'flip': ('vec2( bendTop.x, -bendTop.y )', 'vec2( bendTop.x, bendTop.y )'),
    'row': ('rp.y * rs.x + rp.x', 'rp.x * rs.y + rp.y'),
    'gamma': ('vec4( rt.rgb, 1.0 )', 'vec4( pow( rt.rgb, vec3( 1.0 / 2.2 ) ), 1.0 )'),
    'weight': ('rt.a > 0.0 ? vec4( rt.rgb, 1.0 ) : vec4( 0.0 )', 'vec4( rt.rgb, clamp( rt.a, 0.0, 1.0 ) )'),
    'off': ('if ( !waterRtOn || under )', 'if ( under )'),
    'under': ('if ( !waterRtOn || under )', 'if ( !waterRtOn )'),
}

VERT = """#version 410 core
layout( location = 0 ) in vec3 pos;
void main() { gl_Position = vec4( pos, 1.0 ); }
"""
FRAG_HEAD = """#version 410 core
uniform ivec4 viewportDimensions;
"""
FRAG_MAIN = """
uniform vec3 skyIn;
uniform bool bentIn;
uniform bool underIn;
uniform sampler2D bendTex;
layout( location = 0 ) out vec4 outCol;
layout( location = 1 ) out ivec4 outTex;
void main()
{
	ivec2 px = ivec2( gl_FragCoord.xy ) - viewportDimensions.xy;
	vec2 b = texelFetch( bendTex, px, 0 ).xy;
	vec4 h = wwWaterRtHit( gl_FragCoord.xy, bentIn, b, underIn );
	outCol = vec4( wwWaterRtOver( skyIn, h ), h.a );
	outTex = ivec4( wwWaterRtTexel( wwWaterRtView( gl_FragCoord.xy, bentIn, b ) ), px );
}
"""
VIEWS = [(0, 0, 320, 200), (0, 0, 321, 201), (7, 5, 319, 183), (0, 0, 641, 361)]
SKY = (0.3125, 0.5, 0.875)

def fn(name, res, *args):
    addr = gl.wglGetProcAddress(name.encode())
    if addr:
        return ctypes.WINFUNCTYPE(res, *args)(addr)
    f = getattr(gl, name)
    f.restype = res
    f.argtypes = list(args)
    return f

def half(n):
    return (n + 1) // 2   # rtddgi_gpu.h reflHalf

def main():
    a = sys.argv[1:]
    sdir = a[0]
    red = a[a.index('--red') + 1] if '--red' in a else ''
    inc = open(os.path.join(sdir, 'water_rtrefl.glsl'), 'rb').read().decode('latin-1')
    if red in MUTANTS:
        old, new = MUTANTS[red]
        if inc.count(old) != 1:
            print('MUTANT %s: anchor found %d times' % (red, inc.count(old))); return 2
        inc = inc.replace(old, new)
    elif red not in ('', 'texel'):
        print('unknown red', red); return 2

    slc.context()
    print('driver:', gl.glGetString(0x1F01).decode(), '|', gl.glGetString(0x1F02).decode())
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
    loc_ = fn('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
    u1i = fn('glUniform1i', None, GLint, GLint)
    u3f = fn('glUniform3f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float)
    u4f = fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
    u4i = fn('glUniform4i', None, GLint, GLint, GLint, GLint, GLint)
    genTextures = fn('glGenTextures', None, ctypes.c_int, ctypes.POINTER(GLuint))
    deleteTextures = fn('glDeleteTextures', None, ctypes.c_int, ctypes.POINTER(GLuint))
    bindTexture = fn('glBindTexture', None, GLenum, GLuint)
    texImage2D = fn('glTexImage2D', None, GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, ctypes.c_void_p)
    texParameteri = fn('glTexParameteri', None, GLenum, GLenum, GLint)
    texBuffer = fn('glTexBuffer', None, GLenum, GLenum, GLuint)
    activeTexture = fn('glActiveTexture', None, GLenum)
    genFramebuffers = fn('glGenFramebuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
    bindFramebuffer = fn('glBindFramebuffer', None, GLenum, GLuint)
    framebufferTexture2D = fn('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)
    checkFb = fn('glCheckFramebufferStatus', GLenum, GLenum)
    drawBuffers = fn('glDrawBuffers', None, ctypes.c_int, ctypes.POINTER(GLenum))
    readBuffer = fn('glReadBuffer', None, GLenum)
    viewport = fn('glViewport', None, GLint, GLint, ctypes.c_int, ctypes.c_int)
    genVertexArrays = fn('glGenVertexArrays', None, ctypes.c_int, ctypes.POINTER(GLuint))
    bindVertexArray = fn('glBindVertexArray', None, GLuint)
    genBuffers = fn('glGenBuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
    bindBuffer = fn('glBindBuffer', None, GLenum, GLuint)
    bufferData = fn('glBufferData', None, GLenum, ctypes.c_ssize_t, ctypes.c_void_p, GLenum)
    vap = fn('glVertexAttribPointer', None, GLuint, GLint, GLenum, ctypes.c_ubyte, ctypes.c_int, ctypes.c_void_p)
    eva = fn('glEnableVertexAttribArray', None, GLuint)
    drawArrays = fn('glDrawArrays', None, GLenum, GLint, ctypes.c_int)
    readPixels = fn('glReadPixels', None, GLint, GLint, ctypes.c_int, ctypes.c_int, GLenum, GLenum, ctypes.c_void_p)
    getError = fn('glGetError', GLenum)

    p = createProgram()
    for kind, src in ((0x8B31, VERT), (0x8B30, FRAG_HEAD + inc + FRAG_MAIN)):
        s = createShader(kind)
        b = ctypes.c_char_p(src.encode('latin-1'))
        shaderSource(s, 1, ctypes.byref(b), None)
        compileShader(s)
        ok = GLint(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(s, 1 << 16, None, log)
            print('COMPILE FAIL', log.value.decode(errors='replace')[:1200]); return 2
        attach(p, s)
    link(p)
    ok = GLint(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        print('LINK FAIL'); return 2
    useProgram(p)
    L = lambda n: loc_(p, n.encode())
    vao = GLuint(); genVertexArrays(1, ctypes.byref(vao)); bindVertexArray(vao)
    vbo = GLuint(); genBuffers(1, ctypes.byref(vbo)); bindBuffer(0x8892, vbo)
    quad = np.array([-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0], dtype=np.float32)
    bufferData(0x8892, quad.nbytes, quad.ctypes.data, 0x88E4)
    vap(0, 3, 0x1406, 0, 0, None); eva(0)
    u1i(L('waterRt'), 3); u1i(L('bendTex'), 4)
    u3f(L('skyIn'), *SKY)
    sky = np.array(SKY, np.float32)

    rng = np.random.default_rng(1102)
    fails = []
    odd = 0
    for (vx, vy, W, H) in VIEWS:
        odd += (W % 2) or (H % 2)
        Wh, Hh = half(W), half(H)
        # the hits: HDR rgb, distances 1e-3..1e5, a fifth of them misses (0 0 0 0, as refl.comp writes)
        rgb = np.exp(rng.uniform(np.log(1e-4), np.log(1e4), (Hh, Wh, 3))).astype(np.float32)
        dist = np.exp(rng.uniform(np.log(1e-3), np.log(1e5), (Hh, Wh))).astype(np.float32)
        miss = rng.uniform(size=(Hh, Wh)) < 0.2
        buf = np.zeros((Hh, Wh, 4), np.float32)
        buf[..., :3] = np.where(miss[..., None], 0.0, rgb)
        buf[..., 3] = np.where(miss, 0.0, dist)
        tb = GLuint(); genBuffers(1, ctypes.byref(tb)); bindBuffer(0x8C2A, tb)
        bufferData(0x8C2A, buf.nbytes, buf.ctypes.data, 0x88E4)
        activeTexture(0x84C0 + 3)
        tt = GLuint(); genTextures(1, ctypes.byref(tt)); bindTexture(0x8C2A, tt); texBuffer(0x8C2A, 0x8814, tb)
        # the bend (top-row-first uv), some past the view's edge (the clamps)
        bend = rng.uniform(-0.06, 0.06, (H, W, 2)).astype(np.float32)
        bend[::7, ::5] *= 12.0
        bend4 = np.zeros((H, W, 4), np.float32); bend4[..., :2] = bend
        activeTexture(0x84C0 + 4)
        bt = GLuint(); genTextures(1, ctypes.byref(bt)); bindTexture(0x0DE1, bt)
        texImage2D(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, bend4.ctypes.data)
        texParameteri(0x0DE1, 0x2801, 0x2600); texParameteri(0x0DE1, 0x2800, 0x2600)
        FW, FH = vx + W, vy + H
        tc = GLuint(); genTextures(1, ctypes.byref(tc)); bindTexture(0x0DE1, tc)
        texImage2D(0x0DE1, 0, 0x8814, FW, FH, 0, 0x1908, 0x1406, None)
        texParameteri(0x0DE1, 0x2801, 0x2600); texParameteri(0x0DE1, 0x2800, 0x2600)
        ti = GLuint(); genTextures(1, ctypes.byref(ti)); bindTexture(0x0DE1, ti)
        texImage2D(0x0DE1, 0, 0x8D82, FW, FH, 0, 0x8D99, 0x1404, None)   # RGBA32I
        texParameteri(0x0DE1, 0x2801, 0x2600); texParameteri(0x0DE1, 0x2800, 0x2600)
        bindTexture(0x0DE1, 0)
        activeTexture(0x84C0 + 4); bindTexture(0x0DE1, bt)
        fbo = GLuint(); genFramebuffers(1, ctypes.byref(fbo)); bindFramebuffer(0x8D40, fbo)
        framebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, tc, 0)
        framebufferTexture2D(0x8D40, 0x8CE1, 0x0DE1, ti, 0)
        bufs = (GLenum * 2)(0x8CE0, 0x8CE1); drawBuffers(2, bufs)
        if checkFb(0x8D40) != 0x8CD5:
            print('FBO incomplete'); return 2
        viewport(vx, vy, W, H)
        u4i(L('viewportDimensions'), vx, vy, W, H)
        fw, fh = (0, 0) if red == 'texel' else (W, H)   # src/gl/cellwater.cpp: rtddgiReflFullSize (0 0 under the red)
        u4f(L('waterRtSize'), float(Wh), float(Hh), float(fw), float(fh))

        # the twin, in float32 as the shader does it; ambiguous = a bent point within 1e-3 px of a pixel edge
        ys, xs = np.mgrid[0:H, 0:W]
        rv0x = np.clip(((np.float32(vx) + xs.astype(np.float32) + np.float32(0.5)) - np.float32(vx)) / np.float32(W), 0, 1).astype(np.float32)
        rv0y = np.clip(((np.float32(vy) + ys.astype(np.float32) + np.float32(0.5)) - np.float32(vy)) / np.float32(H), 0, 1).astype(np.float32)

        def run(bent, under, on):
            u1i(L('bentIn'), int(bent)); u1i(L('underIn'), int(under)); u1i(L('waterRtOn'), int(on))
            drawArrays(0x0005, 0, 4)
            col = np.zeros((H, W, 4), np.float32); tex = np.zeros((H, W, 4), np.int32)
            readBuffer(0x8CE0); readPixels(vx, vy, W, H, 0x1908, 0x1406, col.ctypes.data)
            readBuffer(0x8CE1); readPixels(vx, vy, W, H, 0x8D99, 0x1404, tex.ctypes.data)
            e = getError()
            if e:
                print('GL error 0x%x' % e)
            return col, tex

        for bent in (False, True):
            col, tex = run(bent, False, True)
            if bent:
                rx = np.clip(rv0x + bend[..., 0], 0, 1).astype(np.float32)
                ry = np.clip(rv0y - bend[..., 1], 0, 1).astype(np.float32)
            else:
                rx, ry = rv0x, rv0y
            fx = (rx.astype(np.float64) * W); fy = (ry.astype(np.float64) * H)
            amb = (np.abs(fx - np.round(fx)) < 1e-3) & (np.round(fx) > 0) & (np.round(fx) < W)
            amb |= (np.abs(fy - np.round(fy)) < 1e-3) & (np.round(fy) > 0) & (np.round(fy) < H)
            if not bent:
                amb[:] = False
            fpx = np.clip(np.floor(fx).astype(np.int64), 0, W - 1)
            fpy = np.clip(np.floor(fy).astype(np.int64), 0, H - 1)
            hx, hy = np.minimum(fpx // 2, Wh - 1), np.minimum(fpy // 2, Hh - 1)
            pxok = (tex[..., 2] == xs) & (tex[..., 3] == ys)
            good = ~amb
            texBad = good & ((tex[..., 0] != hx) | (tex[..., 1] != hy))
            want = buf[hy, hx]
            hit = want[..., 3] > 0.0
            exp = np.where(hit[..., None], want[..., :3], sky)
            colBad = good & np.any(col[..., :3].view(np.uint32) != exp.astype(np.float32).view(np.uint32), axis=-1)
            alphaBad = good & (col[..., 3] != np.where(hit, 1.0, 0.0))
            nHit, nMiss = int((good & hit).sum()), int((good & ~hit).sum())
            unitsBad = int((colBad & hit).sum()) + int((alphaBad & hit).sum())
            fallBad = int((colBad & ~hit).sum()) + int((alphaBad & ~hit).sum())
            tag = '%dx%d+%d+%d %s' % (W, H, vx, vy, 'bent' if bent else 'flat')
            print('  %-20s texel off %6d of %6d (ambiguous %d = %.3f %%), hits %6d units off %6d, misses %6d fallback off %6d%s' % (
                tag, int(texBad.sum()), int(good.sum()), int(amb.sum()), 100.0 * amb.mean(), nHit, unitsBad, nMiss, fallBad,
                '' if pxok.all() else ' PIXEL GRID WRONG'))
            if not pxok.all(): fails.append(tag + ' grid')
            if texBad.any(): fails.append(tag + ' texel')
            if amb.mean() >= 0.005: fails.append(tag + ' ambiguous share')
            if unitsBad: fails.append(tag + ' units')
            if fallBad: fails.append(tag + ' fallback')
            if nHit < 1000 or nMiss < 200: fails.append(tag + ' floor')
        for under, on, name in ((True, True, 'under'), (False, False, 'off')):
            col, _ = run(True, under, on)
            bad = int(np.any(col[..., :3].view(np.uint32) != np.broadcast_to(sky, (H, W, 3)).view(np.uint32), axis=-1).sum())
            bad += int((col[..., 3] != 0.0).sum())
            print('  %-20s %-5s: %d of %d px not the sky' % ('%dx%d+%d+%d' % (W, H, vx, vy), name, bad, W * H))
            if bad: fails.append('%dx%d %s fallback' % (W, H, name))
        bindFramebuffer(0x8D40, 0)
        for t in (tt, bt, tc, ti):
            deleteTextures(1, ctypes.byref(t))
    if odd < 2:
        fails.append('floor: fewer than two odd-size viewports')
    ok = not fails
    if fails:
        print('failed:', ', '.join(fails[:12]) + (' ...' if len(fails) > 12 else ''))
    print('RTREFLUNIT %s%s' % ('PASS' if ok else 'FAIL', ' red=' + red if red else ''))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
