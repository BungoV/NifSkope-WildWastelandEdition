"""Lane PERFFIX3 (item 2): the sky INSIDE the cell's linear frame (WW_SKY_LINEAR=1) - one tone map, one exposure.
Offline on the real driver (hidden 4.1 core context, tests/spells/shader_link_check.py); runs no NifSkope.

    python -I tests/skylinear/skylinear_check.py [tree] [--red noblend|old|wiring=<base commit>] [--out file]

The frame is built as NifSkope builds it with the row on: the physical sky (the REAL atmo_sky.frag, view transform 3,
exposure 1: scene units) into a multisample RGBA32F frame, the sky's air over it (L, 1 - T) with the blend state
cellvolair.cpp sets over a sky in the frame, then the REAL cell_hdr.frag mode 0 (the cell imagespace) over it all.
Gates:
  S sum       the displayed frame == the same tone map over a numpy reference frame sky x T + L (float64): max 1 level
              of 255 (the frame's alpha stays 1)
  E exposure  doubling the cell exposure brightens >= 95 % of the sky's pixels (the sky follows the frame's one
              exposure); today's path (the sky drawn in display space with the studio exposure + AgX before the frame,
              the air tone-mapped over it by mode 2) moves only the air's share
  K curve     a sky pixel and a surface pixel of the same linear light display the same (one tone map): max 1 level
  W wiring    the C++ the offline frame mirrors: lookdevstage marks the sky stencil 1 (REPLACE) and draws it at exposure
              1 / view transform 3; cellhdr drops the sky split while the sky is in; cellvolair blends
              (ONE, ONE_MINUS_SRC_ALPHA, ZERO, ONE) over it; glview calls the frame's sky draw after wwCellHdrBegin
Reds: noblend = the air pass with today's state (no blend over the sky): S fails; old = today's order (display-space
sky + mode 2): S, E and K fail; wiring=<commit> reads the C++ of that commit: W fails.
"""
import ctypes, math, os, re, subprocess, sys
import numpy as np

TREE = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 and not sys.argv[1].startswith('--') else \
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.environ['TREE'] = TREE
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S   # noqa: E402
import atmo_skyrender as SR     # noqa: E402

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
SDIR = os.path.join(TREE, 'res', 'shaders')
W, H = 320, 180


def agx(c):
    """lookdev_output.glsl agxDisplay (numpy twin): today's display-space sky"""
    m = np.array([[0.842479062253094, 0.0423282422610123, 0.0423756549057051],
                  [0.0784335999999992, 0.878468636469772, 0.0784336],
                  [0.0792237451477643, 0.0791661274605434, 0.879142973793104]])   # GLSL mat3 columns
    mi = np.array([[1.19687900512017, -0.0528968517574562, -0.0529716355144438],
                   [-0.0980208811401368, 1.15190312990417, -0.0980434501171241],
                   [-0.0990297440797205, -0.0989611768448433, 1.15107367264116]])
    x = np.maximum(c, 1e-10) @ m
    x = np.clip(np.log2(np.maximum(x, 1e-30)), -12.47393, 4.026069)
    x = (x + 12.47393) / (4.026069 + 12.47393)
    x2 = x * x
    x4 = x2 * x2
    x = 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232
    return np.clip(x @ mi, 0.0, 1.0)


def main():
    a = sys.argv[1:]
    red = a[a.index('--red') + 1] if '--red' in a else ''
    out = a[a.index('--out') + 1] if '--out' in a else ''
    lines = []
    fails = 0

    def say(s):
        print(s)
        lines.append(s)

    def gate(name, ok, msg):
        nonlocal fails
        say(('PASS ' if ok else 'FAIL ') + name + ': ' + msg)
        fails += 0 if ok else 1

    sky = SR.Sky(SDIR, haze=(0.0, 1.0, 0.0))
    L_sky, _ = sky.render(W, H, (20.0, 12.0), (35.0, 18.0), vfov=60.0)   # linear scene units, row 0 = top
    L_sky = np.maximum(L_sky, 0.0)
    g = sky.g
    # the sky's air: clear in the top third (T 1, L 0), a haze band lower down; L in scene units
    yy = np.linspace(0.0, 1.0, H)[:, None] * np.ones((1, W))
    T = np.where(yy < 1 / 3, 1.0, 1.0 - 0.7 * (yy - 1 / 3) * 1.5)
    T = np.clip(T, 0.3, 1.0)
    fogc = np.array([0.55, 0.6, 0.7])
    L_air = (1.0 - T)[..., None] * fogc[None, None, :]

    # programs: a texel copy, the air writer, the real tone map
    vsrc = S.expand(os.path.join(SDIR, 'cell_ao.vert'))
    pcopy = SR.compile_src(vsrc, '#version 410 core\nuniform sampler2D src;\nout vec4 c;\n'
                                 'void main() { c = texelFetch( src, ivec2( gl_FragCoord.xy ), 0 ); }\n')
    pair = SR.compile_src(vsrc, '#version 410 core\nuniform sampler2D src;\nout vec4 c;\n'
                                'void main() { vec4 a = texelFetch( src, ivec2( gl_FragCoord.xy ), 0 ); c = vec4( a.rgb, 1.0 - a.a ); }\n')
    hsrc = S.expand(os.path.join(SDIR, 'cell_hdr.frag'))
    hsrc, n = re.subn(r'layout\s*\(\s*std140\s*\)\s*uniform\s+globalUniforms\s*\{.*?\};',
                      'uniform mat3 viewMatrix;\nuniform mat4 projectionMatrix;', hsrc, flags=re.S)
    phdr = SR.compile_src(S.expand(os.path.join(SDIR, 'cell_hdr.vert')), hsrc)

    tex2d = gl.glTexImage2D
    tex2d.argtypes = [GLenum, GLint, GLint, ctypes.c_int, ctypes.c_int, GLint, GLenum, GLenum, ctypes.c_void_p]
    texms = S.fn('glTexImage2DMultisample', None, GLenum, ctypes.c_int, GLenum, ctypes.c_int, ctypes.c_int, ctypes.c_ubyte)
    fbt2 = S.fn('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)
    blendsep = S.fn('glBlendFuncSeparate', None, GLenum, GLenum, GLenum, GLenum)
    keep = []

    def tex_rgba(img4):
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t))
        gl.glBindTexture(0x0DE1, t)
        arr = np.ascontiguousarray(img4[::-1].astype(np.float32))   # GL rows bottom-up
        keep.append(arr)
        tex2d(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, arr.ctypes.data)
        for k, v in ((0x2801, 0x2600), (0x2800, 0x2600)):
            gl.glTexParameteri(0x0DE1, k, v)
        return t.value

    def frame_ms():
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t))
        gl.glBindTexture(0x9100, t)
        texms(0x9100, 1, 0x8814, W, H, 1)
        fb = GLuint()
        g.genfb(1, ctypes.byref(fb))
        g.bindfb(0x8D40, fb)
        fbt2(0x8D40, 0x8CE0, 0x9100, t, 0)
        gl.glClearColor(0, 0, 0, 0)
        gl.glClear(0x4000)
        return t.value, fb.value

    def view8():
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t))
        gl.glBindTexture(0x0DE1, t)
        tex2d(0x0DE1, 0, 0x8058, W, H, 0, 0x1908, 0x1401, None)   # RGBA8, the view
        fb = GLuint()
        g.genfb(1, ctypes.byref(fb))
        g.bindfb(0x8D40, fb)
        fbt2(0x8D40, 0x8CE0, 0x0DE1, t, 0)
        return t.value, fb.value

    def quad(prog, src_tex):
        g.use(prog)
        g.active(0x84C0)
        gl.glBindTexture(0x0DE1, src_tex)
        g.u1i(g.loc(prog, b'src'), 0)
        gl.glDrawArrays(5, 0, 4)

    # every sampler of the tone map on its own unit (one type a unit)
    nact = GLint()
    S.fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))(phdr, 0x8B86, ctypes.byref(nact))
    samplers = {}
    for i in range(nact.value):
        nm = ctypes.create_string_buffer(256)
        ln, sz, ty = GLint(), GLint(), GLenum()
        S.fn('glGetActiveUniform', None, GLuint, GLuint, ctypes.c_int, ctypes.POINTER(GLint), ctypes.POINTER(GLint),
             ctypes.POINTER(GLenum), ctypes.c_char_p)(phdr, i, 256, ctypes.byref(ln), ctypes.byref(sz), ctypes.byref(ty), nm)
        if ty.value in (0x8B5E, 0x8B5F, 0x9108, 0x910A, 0x8DC8, 0x8DD2, 0x910D):
            samplers[nm.value.decode()] = ty.value

    def tonemap(frame_tex, dst_fb, mode, exposure, blend=False):
        g.bindfb(0x8D40, dst_fb)
        gl.glViewport(0, 0, W, H)
        g.use(phdr)
        unit = 1
        for nm, ty in samplers.items():
            g.active(0x84C0 + unit)
            g.u1i(g.loc(phdr, nm.encode()), unit)
            unit += 1
        g.active(0x84C0 + 1 + list(samplers).index('hdrSrc'))
        gl.glBindTexture(0x9100, frame_tex)
        u = dict(cellOn=1, cellIsOn=1, cellIsLinear=1, hdrMode=mode, hdrStencilTest=0, wwDitherOn=0, cellIsRed=0,
                 cellIsBloomOn=0, cellIsLutOn=0, cellIsFoBloomOn=0, cellFogBloomOn=0, hdrBackdropOn=0)
        for k, v in u.items():
            l = g.loc(phdr, k.encode())
            if l >= 0:
                g.u1i(l, v)
        for k, v in dict(cellIsExposure=(exposure,), cellIsE=(0.0,), cellIsAdapted=(0.18,),
                         cellIsCine=(1.0, 1.0, 1.0), cellBloomGain=(1.0,)).items():
            l = g.loc(phdr, k.encode())
            if l >= 0:
                {1: g.u1f, 3: g.u3f}[len(v)](l, *v)
        l = g.loc(phdr, b'cellIsTint')
        if l >= 0:
            sky.u4f(l, 0.0, 1.0, 1.0, 1.0)
        if blend:
            gl.glEnable(0x0BE2)
            blendsep(1, 0x0303, 1, 0x0303)   # ONE, ONE_MINUS_SRC_ALPHA (cellhdr's mode 2 blend)
        gl.glDrawArrays(5, 0, 4)
        gl.glDisable(0x0BE2)

    def read8(fb):
        g.bindfb(0x8D40, fb)
        buf = (ctypes.c_ubyte * (W * H * 4))()
        gl.glReadPixels(0, 0, W, H, 0x1908, 0x1401, buf)
        return np.frombuffer(buf, np.uint8).reshape(H, W, 4)[::-1].astype(np.int32)

    def readms_alpha(tex):
        # resolve the 1-sample frame through a copy into a float target to read its alpha
        t, fb = sky.g.target(W, H)
        g.bindfb(0x8D40, fb)
        gl.glViewport(0, 0, W, H)
        p = readms_alpha.p
        g.use(p)
        g.active(0x84C0)
        gl.glBindTexture(0x9100, tex)
        g.u1i(g.loc(p, b'src'), 0)
        gl.glDrawArrays(5, 0, 4)
        buf = (ctypes.c_float * (W * H * 4))()
        gl.glReadPixels(0, 0, W, H, 0x1908, 0x1406, buf)
        return np.frombuffer(buf, np.float32).reshape(H, W, 4)[::-1]
    readms_alpha.p = SR.compile_src(vsrc, '#version 410 core\nuniform sampler2DMS src;\nout vec4 c;\n'
                                          'void main() { c = texelFetch( src, ivec2( gl_FragCoord.xy ), 0 ); }\n')

    sky4 = np.concatenate([L_sky, np.ones((H, W, 1))], axis=2)
    air4 = np.concatenate([L_air, T[..., None]], axis=2)
    t_sky, t_air = tex_rgba(sky4), tex_rgba(air4)
    ref = L_sky * T[..., None] + L_air
    t_ref = tex_rgba(np.concatenate([ref, np.ones((H, W, 1))], axis=2))
    gl.glDisable(0x0B71)   # depth
    gl.glDisable(0x0B90)   # stencil (the stencil marks are wiring: gate W)

    def build_new(exposure, blend_air=True):
        f, ffb = frame_ms()
        gl.glViewport(0, 0, W, H)
        quad(pcopy, t_sky)                   # the backdrop into the frame (scene units)
        if blend_air:
            gl.glEnable(0x0BE2)
            blendsep(1, 0x0303, 0, 1)        # ONE, ONE_MINUS_SRC_ALPHA, ZERO, ONE (cellvolair over the sky)
        quad(pair, t_air)
        gl.glDisable(0x0BE2)
        v, vfb = view8()
        tonemap(f, vfb, 0, exposure)
        return read8(vfb), f

    def build_old(exposure, studio=1.0):
        # today: the sky in display space (studio exposure + AgX) in the view, the frame holds only the air (L, 1 - T),
        # mode 2 tone-maps it and lays it over the view by coverage
        disp = agx(L_sky * studio)
        v, vfb = view8()
        vt = tex_rgba(np.concatenate([disp, np.ones((H, W, 1))], axis=2))
        gl.glViewport(0, 0, W, H)
        quad(pcopy, vt)
        f, ffb = frame_ms()
        gl.glViewport(0, 0, W, H)
        quad(pair, t_air)
        tonemap(f, vfb, 2, exposure, blend=True)
        return read8(vfb), f

    def build_ref(exposure):
        f, ffb = frame_ms()
        gl.glViewport(0, 0, W, H)
        quad(pcopy, t_ref)
        v, vfb = view8()
        tonemap(f, vfb, 0, exposure)
        return read8(vfb)

    e0 = 1.6
    D_ref = build_ref(e0)
    if red == 'old':
        D, f = build_old(e0)
    else:
        D, f = build_new(e0, blend_air=red != 'noblend')
    alpha = readms_alpha(f)[..., 3]
    dS = int(np.abs(D[..., :3] - D_ref[..., :3]).max())
    gate('S sum', dS <= 1 and (red == 'old' or float(np.abs(alpha - 1).max()) < 1e-6),
         'tone map of the built frame vs of sky x T + L: max %d level(s) (<= 1); frame alpha 1 - %.2g' % (
             dS, float(np.abs(alpha - 1).max())))
    D2 = build_old(2 * e0)[0] if red == 'old' else build_new(2 * e0, blend_air=red != 'noblend')[0]
    lum = lambda x: x[..., :3] @ np.array([0.2126, 0.7152, 0.0722])
    sky_px = L_sky.sum(axis=2) > 1e-4
    up = float(((lum(D2) > lum(D) + 0.5) & sky_px).sum()) / max(int(sky_px.sum()), 1)
    gate('E exposure', up >= 0.95, 'share of sky pixels brighter at 2 x the cell exposure: %.3f (>= 0.95)' % up)
    # K: a surface of the same linear light as the sky (a plain linear patch in the frame, tone-mapped by mode 0)
    Dk = D   # the built frame at e0: its clear top third holds the sky alone (T 1, L 0)
    f, ffb = frame_ms()
    gl.glViewport(0, 0, W, H)
    quad(pcopy, t_sky)
    v, vfb = view8()
    tonemap(f, vfb, 0, e0)
    Ds = read8(vfb)   # the same light as a cell surface would write it
    top = slice(0, H // 3)
    dK = int(np.abs(Dk[top, :, :3] - Ds[top, :, :3]).max())
    gate('K curve', dK <= 1, 'clear sky (top third) vs a surface of the same linear light: max %d level(s) (<= 1)' % dK)

    # W: the C++ wiring
    def src(path):
        if red.startswith('wiring='):
            return subprocess.run([os.environ.get('GIT', 'git'), '-C', TREE, 'show', red.split('=', 1)[1] + ':' + path],
                                  capture_output=True, text=True, encoding='latin-1').stdout
        return open(os.path.join(TREE, path), encoding='latin-1').read()
    ld, hd, va, gv = (src('src/gl/lookdevstage.cpp'), src('src/gl/cellhdr.cpp'), src('src/gl/cellvolair.cpp'),
                      src('src/glview.cpp'))
    checks = {
        'lookdev stencil 1 REPLACE': 'glStencilFunc( GL_ALWAYS, 1, 0xFF );' in ld.split('void wwLookdevSkyLinearDraw')[-1]
        and 'wwLookdevSkyLinearDraw' in ld,
        'lookdev exposure 1 / vt 3': '( g_skyCapture || g_skyLinear ) ? 1.0f' in ld and '( g_skyCapture || g_skyLinear ) ? 3' in ld,
        'cellhdr no split with the sky in': '!skyholeOld && !skyIn' in hd,
        'cellvolair blend over the sky': 'glBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE )' in va
        and 'wwCellHdrSkyIn()' in va,
        'glview after wwCellHdrBegin': re.search(r'wwCellHdrBegin\( scene \);[^\n]*\r?\n\s*wwLookdevSkyLinearDraw\( scene, hdr \);', gv) is not None,
    }
    bad = [k for k, ok in checks.items() if not ok]
    gate('W wiring', not bad, 'the C++ the offline frame mirrors: %d of %d present%s' % (
        len(checks) - len(bad), len(checks), (' (missing: ' + ', '.join(bad) + ')') if bad else ''))
    say('SKYLINEAR %s (%d gate(s) failed)%s' % ('PASS' if not fails else 'FAIL', fails, ' RED=' + red if red else ''))
    if out:
        open(out, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
