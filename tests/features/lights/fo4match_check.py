"""Lane FO4MATCH1: the interior specular terms of cellLit (readable half of res/shaders/hlsl/lights/cell_lights.hlsli)
against the game's own sun pass, offline. No NifSkope window: a probe pixel shader made from the far-ground entry's
prologue (its includes, uniforms and statics) calls cellLit on a 32 x 32 grid of normals, views and glosses; DXC ->
SPIR-V (tests/hlsllive/build_spv + spvgl) -> the real driver in a hidden GL context.

    python -I -X utf8 tests/features/lights/fo4match_check.py <scratch dir>

The probe draws an interior with albedo 0, cube 0, spec mask 1 and spec colour 1, so out^2 is the specular sum alone.
The twin is the game's sun pass (Fallout 4 Community Shaders, branch bungo, res/Lighting/bsdf_light_sun.hlsl, the
legacy branch: ambient specular from the DALC, normalized Blinn-Phong with Cook-Torrance visibility, Schlick F0 0.2,
x 0.25, min 15, x pi x saturate(NdotL)), written here from that text, wetness 0 (interiors).
Bars (written before the first run, notes/fo4match1/bounds.txt):
  E   ambient specular alone; D directional specular alone (3 light directions); ED both: |out^2 - twin| <=
      2e-3 x max(|twin|, 0.01) per pixel; pixels with NdotV <= 0.02 left out of D / ED (the game divides by it);
      at least 30 % of the pixels with twin > 1e-4, else NOCOV.
  Reds, each must FAIL: r1 the E twin with (2 - gloss); r2 the D twin with F0 0.04; r3 the probe on the
  cell_lights.hlsli before this lane (git show <rev>:<path>, rev from FO4MATCH_BASE, default 62c7bdb2).
Exit 0 only when E, D, ED pass and every red fails.
"""
import ctypes, os, re, subprocess, sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
HL = os.path.join(TREE, 'res', 'shaders', 'hlsl')
sys.path.insert(0, os.path.join(TREE, 'tests', 'hlsllive'))
from glctx import *          # noqa: E402,F401
import build_spv, spvgl      # noqa: E402
import spirv_probe as SP     # noqa: E402

SCR = os.path.abspath(sys.argv[1])
os.makedirs(SCR, exist_ok=True)
W = H = 32
GIT = 'E:/Tools/Git/cmd/git.exe'
BASE = os.environ.get('FO4MATCH_BASE', '')

ROWS = np.array([[0.10, 0.05, 0.20, 0.50], [0.05, 0.10, 0.15, 0.45], [-0.05, 0.10, 0.25, 0.40]])   # positive on the sphere
DIRC = np.array([1.0, 0.8, 0.6])
LDIRS = [np.array([0.3, 0.2, 1.0]), np.array([-0.6, 0.4, 0.7]), np.array([0.1, -0.9, 0.35])]

BODY = r'''
struct WwProbeIn { float4 pos : SV_Position; };
float4 main( WwProbeIn i ) : SV_Target0
{
	gl_FragCoord = i.pos;
	float2 q = floor( i.pos.xy );
	float u = ( q.x + 0.5 ) / 32.0, v = ( q.y + 0.5 ) / 32.0;
	float3 N = normalize( float3( 0.3 * sin( 7.0 * v ), 0.3 * cos( 5.0 * u ), 1.0 ) );
	float tv = u * 1.5, ph = v * 6.2831853;
	float3 V = float3( sin( tv ) * cos( ph ), sin( tv ) * sin( ph ), cos( tv ) );
	float gl = ( fmod( q.x * 7.0 + q.y * 3.0, 64.0 ) + 0.5 ) / 64.0;	// exact in float: no wrap differs from the twin
	float alphaR = ( 1.0 - gl ) * ( 1.0 - gl );
	cellFarFade = false;
	float3 c = cellLit( (float3)( 0.0 ), (float3)( 0.0 ), N, float3( 0.0, 0.0, -5.0 ), V, 1.0, (float3)( 1.0 ), alphaR, 0.0,
	                    (float3)( 0.0 ), (float3)( 0.0 ) );
	return float4( c, q.x + 32.0 * q.y );
}
'''

VS = r'''
struct VOut { float4 pos : SV_Position; };
VOut main( uint id : SV_VertexID )
{
	VOut o;
	o.pos = float4( ( id & 1 ) != 0 ? 1.0 : -1.0, ( id & 2 ) != 0 ? 1.0 : -1.0, 0.0, 1.0 );
	return o;
}
'''


def probe_source(cell_lights):
    """the far-ground entry's prologue (everything before its frag_main), includes made absolute, + BODY"""
    fg = os.path.join(HL, 'far-ground')
    src = open(os.path.join(fg, 'fo4_cellfgc_ps.hlsl'), encoding='utf-8').read()
    src = src[:re.search(r'^void frag_main\(\)', src, re.M).start()]

    def absinc(m):
        p = os.path.normpath(os.path.join(fg, m.group(1))).replace('\\', '/')
        if p.endswith('lights/cell_lights.hlsli'):
            p = cell_lights.replace('\\', '/')
        return '#include "%s"' % p
    return re.sub(r'#include "([^"]+)"', absinc, src) + BODY


def shifts():
    t = open(os.path.join(HL, 'far-ground', 'FarGroundBindingsVk.hlsli'), encoding='utf-8').read()
    sh = []
    for part in re.search(r'// vk shift: (.*)', t).group(1).split('/'):
        c, n = part.split()
        sh += ['-fvk-%s-shift' % c, n, '0']
    return sh


def compile_stage(name, src, prof, sh):
    path = os.path.join(SCR, name + '.hlsl')
    open(path, 'w', encoding='utf-8', newline='\n').write(src)
    raw, out = path + '.raw', os.path.join(SCR, name + '.spv')
    cmd = [build_spv.DXC, '-spirv', '-fspv-target-env=vulkan1.0'] + sh + ['-O0', '-T', prof, '-E', 'main',
           '-I', os.path.join(HL, 'lights'), '-I', os.path.join(HL, 'far-ground'), '-Fo', raw, path]
    r = subprocess.run(cmd, capture_output=True, text=True, stdin=subprocess.DEVNULL)
    if r.returncode != 0:
        raise SystemExit('DXC FAIL %s: %s' % (name, r.stderr[-1500:]))
    bools = build_spv.bool_names(build_spv.with_loose_halves(path, src))
    data, _ = spvgl.patch(open(raw, 'rb').read(), build_spv.BLOCKS, {}, {}, bools)
    os.remove(raw)
    open(out, 'wb').write(data)
    return out


def program(cell_lights, tag):
    vs = compile_stage('probe_vs', VS, 'vs_6_0', [])
    ps = compile_stage('probe_ps_' + tag, probe_source(cell_lights), 'ps_6_0', shifts())
    p, log = SP.build([('vs', vs), ('ps', ps)])
    if p is None:
        raise SystemExit('LINK FAIL %s: %s' % (tag, log[:1500]))
    return p


class Target:
    def __init__(self):
        gen, bindt = fn('glGenTextures', None, GLsizei, ctypes.POINTER(GLuint)), gl.glBindTexture
        t = GLuint(); gen(1, ctypes.byref(t))
        bindt(0x0DE1, t.value)
        gl.glTexImage2D(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, None)
        gl.glTexParameteri(0x0DE1, 0x2801, 0x2600); gl.glTexParameteri(0x0DE1, 0x2800, 0x2600)
        fb = GLuint(); fn('glGenFramebuffers', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(fb))
        self.fb = fb.value
        fn('glBindFramebuffer', None, GLenum, GLuint)(0x8D40, self.fb)
        fn('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)(0x8D40, 0x8CE0, 0x0DE1, t.value, 0)
        va = GLuint(); fn('glGenVertexArrays', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(va))
        fn('glBindVertexArray', None, GLuint)(va.value)

    def read(self):
        buf = (ctypes.c_float * (W * H * 4))()
        gl.glReadPixels(0, 0, W, H, 0x1908, 0x1406, buf)
        return np.frombuffer(buf, np.float32).reshape(H * W, 4).astype(np.float64)


def set_uni(p, uni):
    loc = fn('glGetUniformLocation', GLint, GLuint, ctypes.c_char_p)
    for k, v in uni.items():
        l = loc(p, k.encode())
        if l < 0:
            l = loc(p, (k + '[0]').encode())
        if l < 0:
            print('   (no active uniform %s: the driver folded it away)' % k)
            continue
        if isinstance(v, (bool, int)):
            fn('glUniform1i', None, GLint, GLint)(l, int(v))
            continue
        a = np.ascontiguousarray(np.asarray(v, np.float32).reshape(-1))
        if a.size == 3:
            fn('glUniform3fv', None, GLint, GLsizei, ctypes.c_void_p)(l, 1, a.ctypes.data)
        else:
            fn('glUniform4fv', None, GLint, GLsizei, ctypes.c_void_p)(l, a.size // 4, a.ctypes.data)


def render(p, tgt, uni):
    fn('glUseProgram', None, GLuint)(p)
    base = {'cellRow': [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0], 'cellInterior': 1, 'cellHasDalc': 0, 'cellHasDir': 0,
            'cellDalc': ROWS, 'cellDirTo': LDIRS[0], 'cellDirColor': DIRC}
    base.update(uni)
    set_uni(p, base)
    gl.glViewport(0, 0, W, H)
    fn('glClearColor', None, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)(0.0, 0.0, 0.0, -1.0)
    gl.glClear(0x4000)
    gl.glDrawArrays(5, 0, 4)
    gl.glFinish()
    px = tgt.read()
    out = np.full((W * H, 3), np.nan)
    idx = px[:, 3]
    ok = idx >= 0
    out[idx[ok].astype(int)] = px[ok, :3]
    return out ** 2


def inputs():
    """the probe's N, V, gloss per pixel index (float32 as the shader, then float64)"""
    k = np.arange(W * H)
    f32 = np.float32
    u = ((k % W).astype(f32) + f32(0.5)) / f32(32.0)
    v = ((k // W).astype(f32) + f32(0.5)) / f32(32.0)
    N = np.stack([f32(0.3) * np.sin(f32(7.0) * v), f32(0.3) * np.cos(f32(5.0) * u), np.ones_like(u)], 1).astype(np.float64)
    N /= np.linalg.norm(N, axis=1, keepdims=True)
    tv, ph = (u * f32(1.5)).astype(np.float64), (v * f32(6.2831853)).astype(np.float64)
    V = np.stack([np.sin(tv) * np.cos(ph), np.sin(tv) * np.sin(ph), np.cos(tv)], 1)
    g = ((((k % W) * 7 + (k // W) * 3) % 64).astype(f32) + f32(0.5)) / f32(64.0)
    g =(f32(1.0) - np.sqrt((f32(1.0) - g) * (f32(1.0) - g))).astype(np.float64)
    return N, V, g


def dot(a, b):
    return np.sum(a * b, 1)


def twin_env(N, V, g, exp_off=3.0):
    """bsdf_light_sun.hlsl: reflV = 2 NdotV N - V; envFres = exp2(log2(1 - sat(NdotV)) x (3 - gloss)) x 0.25;
    envCube = (rows . (reflV, 1))^2.2 x envFres; envSpec = spec mask x envCube"""
    NV = dot(N, V)
    R = 2.0 * NV[:, None] * N - V
    R1 = np.concatenate([R, np.ones((len(R), 1))], 1)
    raw = R1 @ ROWS.T
    fres = (1.0 - np.clip(NV, 0.0, 1.0)) ** (exp_off - g) * 0.25
    return fres[:, None] * np.maximum(raw, 0.0) ** 2.2


def twin_dir(N, V, g, Ldir, f0=0.2):
    """bsdf_light_sun.hlsl legacy branch: n = 2^(gloss x 10 + 1); D = NH^n x (n + 2) x 0.159155; vis select; F =
    min((1 - fc^5) x F0 + fc^5, 1) (the game's 0.2); min(D vis F / 4, 15) x mask x pi x light x sat(NdotL)"""
    L = Ldir / np.linalg.norm(Ldir)
    NL = dot(N, np.broadcast_to(L, N.shape))
    sNL = np.clip(NL, 0.0, 1.0)
    NV = dot(N, V)
    sNV = np.clip(NV, 0.0, 1.0)
    Hh = V + L
    Hh /= np.linalg.norm(Hh, axis=1, keepdims=True)
    VH = np.clip(dot(V, Hh), 0.0, 1.0)
    NH = np.clip(dot(Hh, N), 0.0, 1.0)
    n = 2.0 ** (g * 10.0 + 1.0)
    D = NH ** n * (n + 2.0) * 0.159155
    mn = np.minimum(sNV, sNL)
    with np.errstate(divide='ignore', invalid='ignore'):
        ratio = np.where(sNV == mn, 1.0, sNL / sNV)
        vis = np.where(VH >= 2.0 * NH * mn, 2.0 * NH * ratio / VH, 1.0 / sNV)
    fc = 1.0 - VH
    F = np.minimum((1.0 - fc ** 5) * f0 + fc ** 5, 1.0)
    s = np.minimum(D * vis * F * 0.25, 15.0) * np.pi * sNL
    return s[:, None] * DIRC[None, :], NV > 0.02


def judge(name, got, want, mask=None):
    m = np.isfinite(want).all(1) & np.isfinite(got).all(1)
    if mask is not None:
        m &= mask
    cov = np.mean(want[:, 0] > 1e-4)
    err = np.abs(got - want)[m] / np.maximum(np.abs(want[m]), 0.01)
    worst = float(err.max()) if err.size else float('inf')
    if cov < 0.30:
        verdict = 'NOCOV'
    else:
        verdict = 'PASS' if worst <= 2e-3 else 'FAIL'
    print('%-6s %s worst %.3g (bar 2e-3) over %d px, %d left out, coverage %.0f %%' % (name, verdict, worst, int(m.sum()),
          int((~m).sum()), 100 * cov))
    if os.environ.get('FO4MATCH_DEBUG') and err.size:
        N, V, g = inputs()
        ids = np.nonzero(m)[0][np.argsort(-err.max(1))[:5]]
        for i in ids:
            print('   px %4d NV %.4f gloss %.4f got %s want %s' % (i, dot(N, V)[i], g[i], np.round(got[i], 6), np.round(want[i], 6)))
    return verdict


def main():
    hdc = context()
    tgt = Target()
    N, V, g = inputs()
    p = program(os.path.join(HL, 'lights', 'cell_lights.hlsli'), 'now')
    res = {}
    e = render(p, tgt, {'cellHasDalc': 1})
    res['E'] = judge('E', e, twin_env(N, V, g))
    res['r1'] = judge('r1', e, twin_env(N, V, g, exp_off=2.0))
    dv = []
    for i, Ld in enumerate(LDIRS):
        d = render(p, tgt, {'cellHasDir': 1, 'cellDirTo': Ld})
        tw, m = twin_dir(N, V, g, Ld)
        dv.append(judge('D%d' % i, d, tw, m))
        res['r2_%d' % i] = judge('r2_%d' % i, d, twin_dir(N, V, g, Ld, f0=0.04)[0], m)
    res['D'] = 'PASS' if all(x == 'PASS' for x in dv) else 'FAIL'
    ed = render(p, tgt, {'cellHasDalc': 1, 'cellHasDir': 1, 'cellDirTo': LDIRS[1]})
    tw, m = twin_dir(N, V, g, LDIRS[1])
    res['ED'] = judge('ED', ed, twin_env(N, V, g) + tw, m)
    # r3: the probe on the file before the change
    old = os.path.join(SCR, 'cell_lights_before.hlsli')
    rev = BASE or '62c7bdb2'   # the lane's tree before the change (hlslsrc1 a69426cb merged in)
    r = subprocess.run([GIT, '-C', TREE, 'show', '%s:res/shaders/hlsl/lights/cell_lights.hlsli' % rev], capture_output=True)
    if r.returncode != 0:
        raise SystemExit('git show failed: %s' % r.stderr[-300:])
    open(old, 'wb').write(r.stdout)
    po = program(old, 'before')
    res['r3E'] = judge('r3E', render(po, tgt, {'cellHasDalc': 1}), twin_env(N, V, g))
    d = render(po, tgt, {'cellHasDir': 1, 'cellDirTo': LDIRS[0]})
    tw, m = twin_dir(N, V, g, LDIRS[0])
    res['r3D'] = judge('r3D', d, tw, m)
    green = all(res[k] == 'PASS' for k in ('E', 'D', 'ED'))
    reds = [k for k in res if k.startswith('r')]
    caught = all(res[k] == 'FAIL' for k in reds)
    print('GREENS %s; REDS %s (%s)' % ('PASS' if green else 'FAIL', 'CAUGHT' if caught else 'MISSED',
          ', '.join('%s %s' % (k, res[k]) for k in reds)))
    return 0 if green and caught else 1


if __name__ == '__main__':
    sys.exit(main())
