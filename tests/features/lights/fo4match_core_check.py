"""Lane FO4MATCH1 terms 3-6: the vanilla path (editor BRDF off) of the readable core core/fo4_default_ps.hlsl (fo4_cell
compiles the same text) against the game's sun pass, offline. No NifSkope window: the probe pixel shader takes the
core's own functions out of the file text (ambSpecGame, VisibDiv, TorranceSparrow, OrenNayar, OrenNayarFull,
sunTailGame), calls one per run on a 32 x 32 grid of normals, views, glosses and wrap amounts; DXC -> SPIR-V
(tests/hlsllive/build_spv + spvgl) -> the real driver in a hidden GL context.

    python -I -X utf8 tests/features/lights/fo4match_core_check.py <scratch dir>

Twins (numpy, transcribed from Fallout 4 Community Shaders, branch bungo, res/Lighting/bsdf_light_sun.hlsl, legacy branch):
  T3 ambient specular lobe   (1 - sat(NV))^(3 - gloss) x 0.25 (x the ambient at R, x the spec mask: the callers)
  T4 sun / NIF view specular min(D vis F / 4, 15) x mask x pi x sat(NL), D = NH^n (n + 2) / 2pi, n = 2^(10 gloss + 1),
                             F = min((1 - fc^5) 0.2 + fc^5, 1), the Torrance-Sparrow vis select (wetness 0)
  T5 NIF view diffuse        the game's Oren-Nayar: (A + B max(cosPhi, 0) sqrt(sat((1-NV^2)(1-NL^2))) / max(NV, NL)) x sat(NL)
  T6 sun tail                albedo x (a x sat(-NL) + max(sat((NL + w)/(w + 1)) - sat(NL), 0)), a = the translucency mask
Bars (written before the first run): |probe - twin| <= 2e-3 x max(|twin|, 0.01) per pixel; T4 / T5 leave out NdotV <= 0.02
(the game divides by it); at least 30 % of the pixels with twin > 1e-4, else NOCOV.
Reds, each must FAIL: r3 T3 with (2 - gloss); r4a T4 with F0 0.04; r4b T4 with Schlick ^4; r4c T4 without the 15 cap
(a high-gloss run where the cap bites); r4d the TorranceSparrow of the file before this lane (FO4MATCH_BASE, default
62c7bdb2, its Fresnel Power exponent, here 3); r5 the file's OrenNayarFull; r6 T6's wrap without - sat(NL).
Sites: the vanilla call sites in the file use these functions (a text check; the A4 rows show the rest).
Exit 0 only when every green passes, every red fails and every site is found.
"""
import ctypes, os, re, subprocess, sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
HL = os.path.join(TREE, 'res', 'shaders', 'hlsl')
CORE = 'res/shaders/hlsl/core/fo4_default_ps.hlsl'
sys.path.insert(0, os.path.join(TREE, 'tests', 'hlsllive'))
from glctx import *          # noqa: E402,F401
import build_spv, spvgl      # noqa: E402
import spirv_probe as SP     # noqa: E402

SCR = os.path.abspath(sys.argv[1])
os.makedirs(SCR, exist_ok=True)
W = H = 32
GIT = 'E:/Tools/Git/cmd/git.exe'
BASE = os.environ.get('FO4MATCH_BASE', '') or '62c7bdb2'
FPOW = 3.0          # the material's Fresnel Power in the probe: the vanilla law must not read it
ALB = np.array([0.8, 0.5, 0.3])
LDIRS = [np.array([0.3, 0.2, 1.0]), np.array([-0.6, 0.4, 0.7]), np.array([0.1, -0.9, 0.35])]

HEAD = r'''
#define M_PI 3.1415926535897932384626433832795
#define FLT_EPSILON 1.192092896e-07F
static float fresnelPower = %r;
'''

BODY = r'''
struct WwProbeIn { float4 pos : SV_Position; };
float4 main( WwProbeIn i ) : SV_Target0
{
	float2 q = floor( i.pos.xy );
	float u = ( q.x + 0.5 ) / 32.0, v = ( q.y + 0.5 ) / 32.0;
	float3 N = normalize( float3( 0.3 * sin( 7.0 * v ), 0.3 * cos( 5.0 * u ), 1.0 ) );
	float tv = u * 1.5, ph = v * 6.2831853;
	float3 V = float3( sin( tv ) * cos( ph ), sin( tv ) * sin( ph ), cos( tv ) );
	float gl = ( fmod( q.x * 7.0 + q.y * 3.0, 64.0 ) + 0.5 ) / 64.0;
	gl = GLOSS_MAP;
	float w = ( fmod( q.x * 5.0 + q.y * 11.0, 32.0 ) + 0.5 ) / 32.0;	// the wrap amount (rolloff) and translucency mask
#ifdef MIRROR_L
	float3 L = normalize( 2.0 * dot( N, V ) * N - V );	// H on N: the lobe's peak, where the 15 cap bites
#else
	float3 L = normalize( LDIR );
#endif
	float3 Hh = normalize( L + V );
	float NdotL = dot( N, L );
	float NdotL0 = max( NdotL, FLT_EPSILON );	// as the core's main sets them
	float NdotH = max( dot( N, Hh ), FLT_EPSILON );
	float NdotV = max( dot( N, V ), FLT_EPSILON );
	float VdotH = max( dot( V, Hh ), FLT_EPSILON );
	float3 c = (float3)( 0.0 );
#if MODE == 3
	c = (float3)( ambSpecGame( NdotV, gl ) );
#elif MODE == 4
	c = TorranceSparrow( NdotL0, NdotH, NdotV, VdotH, (float3)( 1.0 ), exp2( gl * 10.0 + 1.0 ), 0.2 ) * NdotL0;
#elif MODE == 5
	c = (float3)( OrenNayar( L, V, N, 1.0 - gl, NdotL ) );
#elif MODE == 50
	c = (float3)( OrenNayarFull( NdotL, dot( N, V ), dot( L, V ), 1.0 - gl ) );
#elif MODE == 6
	c = ALBEDO * sunTailGame( NdotL, 1.0 - w, w );
#endif
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


def function_text(src, name):
    """one function of the file, by name: its first line through its closing brace"""
    m = re.search(r'^[A-Za-z0-9_]+\s+%s\s*\(' % name, src, re.M)
    if not m:
        return None
    i = src.index('{', m.end())
    depth = 0
    for j in range(i, len(src)):
        depth += {'{': 1, '}': -1}.get(src[j], 0)
        if depth == 0:
            return src[m.start():j + 1] + '\n'
    return None


def probe_source(src, mode, ldir, gloss_map='gl', mirror=False):
    fns = ['#define MIRROR_L 1\n'] if mirror else []
    for name in ('ambSpecGame', 'VisibDiv', 'fresnelSchlick', 'TorranceSparrow', 'OrenNayar', 'OrenNayarFull',
                 'sunTailGame'):
        t = function_text(src, name)
        if t:
            fns.append(t)
    defs = '#define MODE %d\n#define LDIR float3( %r, %r, %r )\n#define ALBEDO float3( %r, %r, %r )\n#define GLOSS_MAP %s\n' % (
        (mode,) + tuple(float(x) for x in ldir) + tuple(float(x) for x in ALB) + (gloss_map,))
    return HEAD % FPOW + defs + ''.join(fns) + BODY


def compile_stage(name, src, prof):
    path = os.path.join(SCR, name + '.hlsl')
    open(path, 'w', encoding='utf-8', newline='\n').write(src)
    raw, out = path + '.raw', os.path.join(SCR, name + '.spv')
    cmd = [build_spv.DXC, '-spirv', '-fspv-target-env=vulkan1.0', '-O0', '-T', prof, '-E', 'main', '-Fo', raw, path]
    r = subprocess.run(cmd, capture_output=True, text=True, stdin=subprocess.DEVNULL)
    if r.returncode != 0:
        raise SystemExit('DXC FAIL %s: %s' % (name, r.stderr[-1500:]))
    data, _ = spvgl.patch(open(raw, 'rb').read(), build_spv.BLOCKS, {}, {}, set())
    os.remove(raw)
    open(out, 'wb').write(data)
    return out


class Target:
    def __init__(self):
        gen, bindt = fn('glGenTextures', None, GLsizei, ctypes.POINTER(GLuint)), gl.glBindTexture
        t = GLuint(); gen(1, ctypes.byref(t))
        bindt(0x0DE1, t.value)
        gl.glTexImage2D(0x0DE1, 0, 0x8814, W, H, 0, 0x1908, 0x1406, None)
        gl.glTexParameteri(0x0DE1, 0x2801, 0x2600); gl.glTexParameteri(0x0DE1, 0x2800, 0x2600)
        fb = GLuint(); fn('glGenFramebuffers', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(fb))
        fn('glBindFramebuffer', None, GLenum, GLuint)(0x8D40, fb.value)
        fn('glFramebufferTexture2D', None, GLenum, GLenum, GLenum, GLuint, GLint)(0x8D40, 0x8CE0, 0x0DE1, t.value, 0)
        va = GLuint(); fn('glGenVertexArrays', None, GLsizei, ctypes.POINTER(GLuint))(1, ctypes.byref(va))
        fn('glBindVertexArray', None, GLuint)(va.value)

    def read(self):
        buf = (ctypes.c_float * (W * H * 4))()
        gl.glReadPixels(0, 0, W, H, 0x1908, 0x1406, buf)
        return np.frombuffer(buf, np.float32).reshape(H * W, 4).astype(np.float64)


VSPV = None


def run(src, mode, ldir, tag, tgt, gloss_map='gl', mirror=False):
    global VSPV
    if VSPV is None:
        VSPV = compile_stage('core_probe_vs', VS, 'vs_6_0')
    ps = compile_stage('core_probe_%s' % tag, probe_source(src, mode, ldir, gloss_map, mirror), 'ps_6_0')
    p, log = SP.build([('vs', VSPV), ('ps', ps)])
    if p is None:
        raise SystemExit('LINK FAIL %s: %s' % (tag, log[:1500]))
    fn('glUseProgram', None, GLuint)(p)
    gl.glViewport(0, 0, W, H)
    fn('glClearColor', None, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)(0.0, 0.0, 0.0, -1.0)
    gl.glClear(0x4000)
    gl.glDrawArrays(5, 0, 4)
    gl.glFinish()
    px = tgt.read()
    out = np.full((W * H, 3), np.nan)
    ok = px[:, 3] >= 0
    out[px[ok, 3].astype(int)] = px[ok, :3]
    return out


def inputs(gloss_hi=False):
    k = np.arange(W * H)
    f32 = np.float32
    u = ((k % W).astype(f32) + f32(0.5)) / f32(32.0)
    v = ((k // W).astype(f32) + f32(0.5)) / f32(32.0)
    N = np.stack([f32(0.3) * np.sin(f32(7.0) * v), f32(0.3) * np.cos(f32(5.0) * u), np.ones_like(u)], 1).astype(np.float64)
    N /= np.linalg.norm(N, axis=1, keepdims=True)
    tv, ph = (u * f32(1.5)).astype(np.float64), (v * f32(6.2831853)).astype(np.float64)
    V = np.stack([np.sin(tv) * np.cos(ph), np.sin(tv) * np.sin(ph), np.cos(tv)], 1)
    g = ((((k % W) * 7 + (k // W) * 3) % 64).astype(f32) + f32(0.5)) / f32(64.0)
    if gloss_hi:
        g = (f32(0.85) + f32(0.15) * g).astype(f32)
    w = ((((k % W) * 5 + (k // W) * 11) % 32).astype(f32) + f32(0.5)) / f32(32.0)
    return N, V, g.astype(np.float64), w.astype(np.float64)


def dot(a, b):
    return np.sum(a * b, -1)


# ---- twins (notes/fo4match1/core_twins.py) ----
def twin_amb(N, V, g, exp_off=3.0):
    return (1.0 - np.clip(dot(N, V), 0, 1)) ** (exp_off - g) * 0.25


def twin_sun(N, V, L, g, f0=0.2, cap=15.0, fpow=5):
    NL, NV = dot(N, L), dot(N, V)
    sNL, sNV = np.clip(NL, 0, 1), np.clip(NV, 0, 1)
    Hh = V + L
    Hh = Hh / np.linalg.norm(Hh, axis=-1, keepdims=True)
    VH, NH = np.clip(dot(V, Hh), 0, 1), np.clip(dot(N, Hh), 0, 1)
    n = 2.0 ** (g * 10.0 + 1.0)
    D = NH ** n * (n + 2.0) * 0.159155
    mn = np.minimum(sNV, sNL)
    with np.errstate(divide='ignore', invalid='ignore'):
        ratio = np.where(sNV == mn, 1.0, sNL / sNV)
        vis = np.where(VH >= 2.0 * NH * mn, 2.0 * NH * ratio / VH, 1.0 / sNV)
    fc = 1.0 - VH
    F = np.minimum((1.0 - fc ** fpow) * f0 + fc ** fpow, 1.0)
    return np.minimum(D * vis * F * 0.25, cap) * np.pi * sNL


def twin_oren(N, V, L, rough):
    NL, NV = dot(N, L), dot(N, V)
    Vt = V - N * NV[..., None]
    Lt = L - N * NL[..., None]
    r2 = rough * rough
    A = 1.0 - 0.5 * r2 / (r2 + 0.57)
    B = 0.45 * r2 / (r2 + 0.09)
    s = np.sqrt(np.clip((1 - NV * NV) * (1 - NL * NL), 0, 1))
    with np.errstate(divide='ignore', invalid='ignore'):
        on = np.maximum(dot(Vt, Lt), 0.0) * B * s / np.maximum(NV, NL) + A
    return on * np.clip(NL, 0, 1)


def twin_tail(N, L, a, w, minus_sat=True):
    NL = dot(N, L)
    wr = np.clip((NL + w) / (w + 1.0), 0, 1) - (np.clip(NL, 0, 1) if minus_sat else 0.0)
    return ALB[None, :] * (a * np.clip(-NL, 0, 1) + np.maximum(wr, 0.0))[:, None]


def judge(name, got, want, mask=None):
    if want.ndim == 1:
        want = np.repeat(want[:, None], 3, 1)
    m = np.isfinite(want).all(1) & np.isfinite(got).all(1)
    if mask is not None:
        m &= mask
    cov = np.mean(want[:, 0] > 1e-4)
    err = np.abs(got - want)[m] / np.maximum(np.abs(want[m]), 0.01)
    worst = float(err.max()) if err.size else float('inf')
    verdict = 'NOCOV' if cov < 0.30 else ('PASS' if worst <= 2e-3 else 'FAIL')
    print('%-6s %s worst %.3g (bar 2e-3) over %d px, %d left out, coverage %.0f %%' % (name, verdict, worst, int(m.sum()),
          int((~m).sum()), 100 * cov))
    return verdict


SITES = [
    ('T3 viewport', 'A.rgb * sqrt( ambMask * ambSpecGame( NdotV, smoothness ) )'),
    ('T3 exterior', 'ambSpecGame( NdotV, smoothness ) * ambR'),
    ('T4 exterior', 'cellExtSpec = specSunK * cellSpecTint * Dx * Dx'),
    ('T5 vanilla', ': OrenNayar( L, V, normal, 1.0 - smoothness, NdotL );'),
    ('T6 viewport', 'diffuse += sunTailGame( NdotL, 0.0, subsurfaceRolloff );'),
    ('T6 cell', 'cellBackK = sunTailGame( dot( normal, L ), clamp( backlightPower, 0.0, 1.0 ), subsurfaceRolloff )'),
]


def main():
    context()
    tgt = Target()
    src = open(os.path.join(TREE, CORE), encoding='utf-8').read()
    N, V, g, w = inputs()
    res = {}
    # T3
    got = run(src, 3, LDIRS[0], 't3', tgt)
    res['T3'] = judge('T3', got, twin_amb(N, V, g))
    res['r3'] = judge('r3', got, twin_amb(N, V, g, exp_off=2.0))
    # T4, three lights, and a high-gloss run where the 15 cap bites
    t4 = []
    for i, Ld in enumerate(LDIRS):
        L = np.broadcast_to(Ld / np.linalg.norm(Ld), N.shape)
        m = dot(N, V) > 0.02
        got = run(src, 4, Ld, 't4_%d' % i, tgt)
        t4.append(judge('T4_%d' % i, got, twin_sun(N, V, L, g), m))
        res['r4a_%d' % i] = judge('r4a_%d' % i, got, twin_sun(N, V, L, g, f0=0.04), m)
        res['r4b_%d' % i] = judge('r4b_%d' % i, got, twin_sun(N, V, L, g, fpow=4), m)
    Nh, Vh, gh, _ = inputs(gloss_hi=True)
    L = 2.0 * dot(Nh, Vh)[:, None] * Nh - Vh
    L = L / np.linalg.norm(L, axis=1, keepdims=True)
    m = dot(Nh, Vh) > 0.02
    got = run(src, 4, LDIRS[0], 't4_hi', tgt, gloss_map='0.85 + 0.15 * gl', mirror=True)
    t4.append(judge('T4_hi', got, twin_sun(Nh, Vh, L, gh), m))
    capped = int(np.sum(twin_sun(Nh, Vh, L, gh, cap=np.inf) > 15.0 * np.pi * np.clip(dot(Nh, L), 0, 1) + 1e-6))
    print('   T4_hi: %d px where the cap bites' % capped)
    res['r4c'] = judge('r4c', got, twin_sun(Nh, Vh, L, gh, cap=np.inf), m) if capped else 'NOCOV'
    res['T4'] = 'PASS' if all(x == 'PASS' for x in t4) else 'FAIL'
    r = subprocess.run([GIT, '-C', TREE, 'show', '%s:%s' % (BASE, CORE)], capture_output=True)
    if r.returncode != 0:
        raise SystemExit('git show failed: %s' % r.stderr[-300:])
    old = r.stdout.decode('utf-8')
    L = np.broadcast_to(LDIRS[1] / np.linalg.norm(LDIRS[1]), N.shape)
    res['r4d'] = judge('r4d', run(old, 4, LDIRS[1], 't4_before', tgt), twin_sun(N, V, L, g), dot(N, V) > 0.02)
    # T5
    t5 = []
    for i, Ld in enumerate(LDIRS):
        L = np.broadcast_to(Ld / np.linalg.norm(Ld), N.shape)
        m = dot(N, V) > 0.02
        t5.append(judge('T5_%d' % i, run(src, 5, Ld, 't5_%d' % i, tgt), twin_oren(N, V, L, 1.0 - g), m))
    res['T5'] = 'PASS' if all(x == 'PASS' for x in t5) else 'FAIL'
    L = np.broadcast_to(LDIRS[1] / np.linalg.norm(LDIRS[1]), N.shape)
    res['r5'] = judge('r5', run(src, 50, LDIRS[1], 't5_full', tgt), twin_oren(N, V, L, 1.0 - g), dot(N, V) > 0.02)
    # T6, a light from behind half the grid
    t6 = []
    for i, Ld in enumerate([np.array([0.2, 0.1, -0.4]), LDIRS[2]]):
        L = np.broadcast_to(Ld / np.linalg.norm(Ld), N.shape)
        got = run(src, 6, Ld, 't6_%d' % i, tgt)
        t6.append(judge('T6_%d' % i, got, twin_tail(N, L, 1.0 - w, w)))
        if i == 1:      # the front light: from behind sat(NL) is 0, so the red equals the green there by construction
            res['r6'] = judge('r6', got, twin_tail(N, L, 1.0 - w, w, minus_sat=False))
    res['T6'] = 'PASS' if all(x == 'PASS' for x in t6) else 'FAIL'
    sites = [(n, s in src) for n, s in SITES]
    for n, ok in sites:
        print('site   %-12s %s' % (n, 'FOUND' if ok else 'MISSING'))
    greens = ('T3', 'T4', 'T5', 'T6')
    green = all(res[k] == 'PASS' for k in greens)
    reds = [k for k in res if k.startswith('r')]
    caught = all(res[k] == 'FAIL' for k in reds)
    sites_ok = all(ok for _, ok in sites)
    print('GREENS %s; REDS %s; SITES %s (%s)' % ('PASS' if green else 'FAIL', 'CAUGHT' if caught else 'MISSED',
          'FOUND' if sites_ok else 'MISSING', ', '.join('%s %s' % (k, res[k]) for k in reds)))
    return 0 if green and caught and sites_ok else 1


if __name__ == '__main__':
    sys.exit(main())
