"""Lane VKLIGHT1 round 2 (2026-11-12): the jobs tests/vklight/frag_gate.py runs (GL .frag vs HLSL ps on Vulkan).

Inputs are synthetic but shaped like the frame's (no flight dump of the packed air volume exists yet; the extended
WW_CELL_AIR_DUMP writes one): a camera 731 u over the ground pitched 6 deg down, so the frame holds rays above, at and
under the horizon; the packed volume (near grid 40x24x64 over 64..20000 u, far grid 20x12x32 to 200000 u) from the
air's own height law along each column; the aerial-perspective LUTs smooth and monotone; a terrain grid and a cloud map.
"""
import numpy as np

Tex = None  # frag_gate.Tex, handed in by jobs()

W, H = 320, 180
KM = 1.42875e-5  # km per game unit


def camera(pitch_deg=-6.0, yaw_deg=30.0, eye=(22873.0, -9481.0, 731.0), fovy=60.0, near=15.0, far=400000.0):
    p, y = np.radians(pitch_deg), np.radians(yaw_deg)
    fwd = np.array([np.cos(p) * np.cos(y), np.cos(p) * np.sin(y), np.sin(p)])
    right = np.cross(fwd, [0.0, 0.0, 1.0]); right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    R = np.stack([right, up, -fwd])  # world -> view rows
    f = 1.0 / np.tan(np.radians(fovy) / 2)
    P = np.zeros((4, 4))  # [row][col]
    P[0, 0] = f * H / W; P[1, 1] = f
    P[2, 2] = (far + near) / (near - far); P[2, 3] = 2 * far * near / (near - far); P[3, 2] = -1
    return R, np.array(eye), P


def gl_cols(M):
    """a [row][col] matrix as GL's column-major list"""
    return np.asarray(M, np.float64).T.reshape(-1)


def view_dirs(P, w, h):
    """view-space unit rays through texel centres of a w x h grid (row 0 = the bottom, GL's)"""
    x = (np.arange(w) + 0.5) / w * 2 - 1
    y = (np.arange(h) + 0.5) / h * 2 - 1
    X, Y = np.meshgrid(x, y)
    d = np.stack([X / P[0, 0], Y / P[1, 1], -np.ones_like(X)], -1)
    return d / np.linalg.norm(d, axis=-1, keepdims=True)


def air_column(z0, s, d0, d1, sigma0=2.2e-5, Hh=2600.0):
    """optical depth of rho = exp(-z / Hh) from distance d0 to d1 along a ray from height z0 at elevation sine s"""
    s = np.where(np.abs(s) < 1e-5, 1e-5, s)
    a = np.exp(-(z0 + s * d0) / Hh) - np.exp(-(z0 + s * d1) / Hh)
    return sigma0 * Hh / s * a


def volume(R, eye, P):
    cols, rows, nN, fc, fr, nF = 40, 24, 64, 20, 12, 32
    near, nfar, ffar = 64.0, 20000.0, 200000.0
    up = R[:, 2]  # world up in view space
    z0 = eye[2]
    vol = np.zeros((nN + nF, rows, cols, 4), np.float32)
    Lc = np.array([0.55, 0.68, 0.92])
    for (c, r, n, d0, d1, base) in ((cols, rows, nN, near, nfar, 0), (fc, fr, nF, nfar, ffar, nN)):
        dirs = view_dirs(P, c, r)
        s = dirs @ up
        edges = d0 * np.exp(np.log(d1 / d0) * (np.arange(n) + 1) / n)
        for k in range(n):
            tau = air_column(z0 + (s * d0 if base else 0.0), s, 0.0 if not base else 0.0, edges[k] - (d0 if base else 0.0))
            T = np.exp(-tau)
            tint = 0.85 + 0.15 * (np.arange(c)[None, :] / c)
            L = (1 - T)[..., None] * Lc[None, None, :] * tint[..., None]
            vol[base + k, :r, :c, :3] = L
            vol[base + k, :r, :c, 3] = T
    return vol, dict(cols=cols, rows=rows, nN=nN, fc=fc, fr=fr, nF=nF, near=near, nfar=nfar, ffar=ffar)


def smooth(h, w, ch, seed, lo=0.0, hi=1.0):
    rng = np.random.default_rng(seed)
    a = rng.random((h // 8 + 2, w // 8 + 2, ch))
    yi = np.linspace(0, a.shape[0] - 1.001, h); xi = np.linspace(0, a.shape[1] - 1.001, w)
    y0, x0 = yi.astype(int), xi.astype(int); fy, fx = (yi - y0)[:, None, None], (xi - x0)[None, :, None]
    v = (a[y0][:, x0] * (1 - fy) * (1 - fx) + a[y0 + 1][:, x0] * fy * (1 - fx) + a[y0][:, x0 + 1] * (1 - fy) * fx
         + a[y0 + 1][:, x0 + 1] * fy * fx)
    return (lo + (hi - lo) * v).astype(np.float32)


def volsky(name, hlsl, defines, far_on=True, flags=None, rule=4, rain=True, night=True, reds=()):
    R, eye, P = camera()
    vol, g = volume(R, eye, P)
    up = R[:, 2]
    sun = np.array([0.4, 0.3, 0.35]); sun /= np.linalg.norm(sun)
    # terrain: 2 layers, world z 0..1800 u, the coarse over 400000 u, the fine over 40000 u around the eye
    terr = np.stack([smooth(64, 64, 1, 3, 0.0, 1800.0), smooth(64, 64, 1, 4, 100.0, 1400.0)])
    rowx, rowy = R[:, 0], R[:, 1]  # world x / y of a view point = dot(R[:, k], p) + eye[k]

    def uvrow(axis, x0, span):
        r = R[:, axis] / span
        return [r[0], r[1], r[2], (eye[axis] - x0) / span]
    rain_xf = np.zeros((4, 4))  # view -> (u, v, km below the Low base 70000 u)
    rain_xf[0, :3] = R[:, 0] / 400000.0; rain_xf[0, 3] = (eye[0] - 22873 + 200000) / 400000.0
    rain_xf[1, :3] = R[:, 1] / 400000.0; rain_xf[1, 3] = (eye[1] + 9481 + 200000) / 400000.0
    rain_xf[2, :3] = -up * KM; rain_xf[2, 3] = (70000.0 - eye[2]) * KM
    rain_xf[3, 3] = 1.0
    if flags is None:
        flags = 2 | 4 | 8 | 64 | 16384 | 65536 | 131072
    trans = np.zeros((64, 256, 4), np.float32)  # atTrans LUT: rows rho, columns mu
    u = np.linspace(0, 1, 256)[None, :]; v = np.linspace(0, 1, 64)[:, None]
    for i, k in enumerate((0.25, 0.6, 1.4)):
        trans[..., i] = np.exp(-k * (1.05 - u) * (0.3 + v))
    trans[..., 3] = 1.0
    ap = np.zeros((32, 32, 32, 4), np.float32)
    sl = ((np.arange(32) + 0.5) / 32)[:, None, None]
    ap[..., 0] = 0.30 * sl; ap[..., 1] = 0.38 * sl; ap[..., 2] = 0.52 * sl
    ap[..., 3] = np.exp(-1.6 * sl) * (0.95 + 0.05 * smooth(32, 32, 1, 7)[None, ..., 0])
    tile = np.zeros((24, 40, 4), np.float32)
    tile[..., 0] = smooth(24, 40, 1, 9, 8000.0, 30000.0)[..., 0]
    tile[..., 1] = tile[..., 0] * 0.5
    U = {
        'viewMatrix': gl_cols(R), 'projectionMatrix': gl_cols(P), 'envMapRotation': np.eye(3).reshape(-1),
        'viewportDimensions': [0, 0, W, H],
        'fogOn': 1, 'volOn': 1, 'volSkyOn': 1, 'volSkyMode': 0, 'volSkySea': 0, 'volProbe': 0, 'volFogBlend': 0,
        'volSkyDome': 0.4 if night else 0.0, 'fogNight': [0.02, 0.03, 0.05, 0.35 if night else 0.0], 'fogNightHue': 1.0,
        'volRect': [0, 0, 1.0 / W, 1.0 / H],
        'volSlices': [g['near'], np.log(g['nfar'] / g['near']), g['nN'], 1.0],
        'volFar': [g['nfar'], np.log(g['ffar'] / g['nfar']), g['nF'], 1.0 if far_on else 0.0],
        'volPack': [1.0, 1.0, g['fc'] / g['cols'], g['fr'] / g['rows']],
        'volPackDims': [g['cols'], g['rows'], g['nN'] + g['nF'], g['nN']],
        'volFarClamp': 1, 'volAirFlags': flags,
        'volAirMed': [2.2e-5, 0.0, 1.0 / 2600.0, 9000.0], 'volAirLayer': [0.3, 0.0, 1.0 / 600.0, 0.4],
        'volAirGround': [0.0, 1.0 / 250.0, 1.0 / 1700.0, 0.0],
        'volAirTerrU': uvrow(0, eye[0] - 200000, 400000.0), 'volAirTerrV': uvrow(1, eye[1] - 200000, 400000.0),
        'volAirTerrFU': uvrow(0, eye[0] - 20000, 40000.0), 'volAirTerrFV': uvrow(1, eye[1] - 20000, 40000.0),
        'volAirTerrRef': 250.0,
        'volAirRain': [3e-5, 0.6, 2.0, 1.0] if rain else [0.0, 0.0, 1.0, 1.0],
        'volAirRainFar': [0.45, 0.1, 6.0 if rain else 0.0, 0.0], 'volAirRainXf': gl_cols(rain_xf),
        'volAirDrift': [0.0, 0.0, 1.0, 0.5], 'volAirDriftRed': 0,
        'fogK': [[1.0 / 60000, 0, 0.05, 0], [0.42, 0.5, 0.62, 1.2], [0.6, 0.66, 0.78, 0.92], [0.5, 0.55, 0.7, 0.6],
                 [0.62, 0.7, 0.85, 0.0], [1.0 / 3000, 1.0 / 6000, 0.1, 0.05]],
        'fogView': list(up) + [eye[2]], 'fogDistScale': 1.0, 'fogSun': list(R @ sun) + [1.0],
        'fogSunColour': [1.0, 0.92, 0.8, 8.0], 'fogUnit': 1.0, 'fogRed': 0, 'fogProbe': [0, 0, 0, 0],
        'atAp': [1.0, 16.0, KM, 32.0], 'atApProj': [P[0, 0], P[1, 1], P[0, 2], P[1, 2]], 'atSkyCamR': 6360.0 + eye[2] * KM,
        'atApSkyRule': rule, 'atApDouble': 0.0,
        'atRay': [5.802e-3, 13.558e-3, 33.1e-3], 'atMieS': 3.996e-3, 'atMieE': 4.40e-3, 'atHaze': [0.02, 1.2, 0.6],
        'atOzone': [0.65e-3, 1.881e-3, 0.085e-3], 'atG': 0.8, 'atHazeAlbedo': [0.9, 0.9, 0.9], 'atHazeSpec': [1, 1, 1],
        'atMsOld': 0.0,
    }
    T = {
        'volTex': Tex('3d', 'rgba16f', vol, 'linear', 'clamp'),
        'volTile': Tex('2d', 'rgba32f', tile, 'nearest', 'clamp'),
        'volAirTerr': Tex('2darray', 'r32f', terr, 'linear', 'clamp'),
        'volAirRainMap': Tex('2d', 'rgba16f', smooth(64, 64, 4, 11), 'linear', 'repeat'),
        'atSkyTransTex': Tex('2d', 'rgba16f', trans, 'linear', 'clamp'),
        'atApTex': Tex('3d', 'rgba16f', ap, 'linear', 'clamp'),
    }
    return dict(name=name, size=(W, H), hlsl=hlsl, glsl='air/cell_volsky.frag', defines=defines, uniforms=U,
                textures=T, rule=('ulp', 1), reds=list(reds))


VOLSKY_REDS = [
    ('return float4(v.xyz + ((J * v.w) * (1.0f - Tt)), v.w * Tt);', 'return float4(v.xyz + ((J * v.w) * (1.0f - Tt)), v.w * Tt * 0.98f);'),
    ('((base + min(f, n)) - 0.5f) / volPackDims.z', '((base + min(f, n)) - 0.4f) / volPackDims.z'),  # froxel slice
    ('float tSky = dot(ts, 0.3333333432674407958984375f.xxx);', 'float tSky = dot(ts, 0.33f.xxx);'),  # the sky's T
    ('far = volSlices.x * exp(volSlices.y);', 'far = volSlices.x * exp(volSlices.y) * 0.99f;'),  # near grid's end (far off)
]
# (an AP-depth red, atApTex at s * 0.9, read EXACT: this camera's sky band weight is 0, so the AP read is not covered)
# GL (NV) fuses atTransUv's rho = sqrt(r*r - Rb*Rb) into an fma; at eye height r*r - Rb*Rb is ~132 of 4e7, so the
# unfused Vulkan rho differs by up to 741 ulp (rel 5.8e-5; probe_rhofma: 5 ulp with mad). The rest is the sky ray's
# last bit (wwSkyRay vr 1-2 ulp -> mu up to 21 ulp). Stated bound: no value past rel 1e-4.
VOLSKY_RULE = ('rel', 1e-4, 0.0)


def jobs(want, tex):
    global Tex
    Tex = tex
    all_ = [
        volsky('volsky_leak', 'air/cell_volsky_ps__WW_CELL_FOGLEAK.hlsl', ['WW_CELL_FOGLEAK'], reds=VOLSKY_REDS[:3]),
        volsky('volsky_leak_near', 'air/cell_volsky_ps__WW_CELL_FOGLEAK.hlsl', ['WW_CELL_FOGLEAK'], far_on=False,
               flags=2 | 8 | 32 | 64 | 65536, rule=1, night=False, reds=VOLSKY_REDS[3:]),  # far off, AP rule 1: only the near grid's tail moves it
        volsky('volsky_base', 'air/cell_volsky_ps.hlsl', [], reds=[VOLSKY_REDS[0], VOLSKY_REDS[2]]),  # froxel reads in lookdev_fog.hlsli
    ]
    for j in all_:
        j['rule'] = VOLSKY_RULE
    # probes (only when named): one term off at a time, to find which one carries a difference
    full = 2 | 4 | 8 | 64 | 16384 | 65536 | 131072
    leak = ('air/cell_volsky_ps__WW_CELL_FOGLEAK.hlsl', ['WW_CELL_FOGLEAK'])
    probes = [volsky('probe_nocurve', *leak, flags=full & ~131072), volsky('probe_noterr', *leak, flags=full & ~64),
              volsky('probe_norain', *leak, rain=False), volsky('probe_norec', *leak, flags=full & ~4),
              volsky('probe_rule0', *leak, rule=0), volsky('probe_notail', *leak, flags=8, rule=0, night=False, rain=False)]
    st = volsky('probe_resid', *leak, flags=8, rule=0, night=False, rain=False)
    st['gl_edit'] = [('float tr = wwApFogResidualT( tSky, a.a, atApDouble > 0.5 );',
                      'float tr = wwApFogResidualT( tSky, a.a, atApDouble > 0.5 ); fragColor = vec4( tSky, a.a, tr, mu ); return;')]
    st['vk_edit'] = [('float tr = wwApFogResidualT(param_37, param_38, param_39);',
                      'float tr = wwApFogResidualT(param_37, param_38, param_39); fragColor = float4(tSky, a.w, tr, mu); return;')]
    probes.append(st)
    VARS = {
        'rhofma': [('sqrt(max(0.0f, (r * r) - 40449600.0f));', 'sqrt(max(0.0f, mad(r, r, -40449600.0f)));')],
        'topfma': [('((r * r) * ((mu * mu) - 1.0f)) + 41731600.0f', 'mad(r * r, mad(mu, mu, -1.0f), 41731600.0f)')],
        'topfma2': [('((r * r) * ((mu * mu) - 1.0f)) + 41731600.0f', 'mad(r * r, (mu * mu) - 1.0f, 41731600.0f)')],
        'topmr': [('return max(0.0f, ((-r) * mu) + sqrt(max(0.0f, ((r * r) * ((mu * mu) - 1.0f)) + 41731600.0f)));',
                   'return max(0.0f, mad(-r, mu, sqrt(max(0.0f, ((r * r) * ((mu * mu) - 1.0f)) + 41731600.0f))));')],
    }
    for k, v in VARS.items():
        j = dict(st, name='probe_resid_' + k, vk_edit=st['vk_edit'] + v)
        probes.append(j)
    for base in all_[:2]:  # the base port reads atTransUv from sky/atmo_common.hlsli; a probe edits the main file only
        probes.append(dict(base, name='probe_rhofma_' + base['name'], vk_edit=VARS['rhofma'], reds=[]))
    mu = dict(st, name='probe_mu')
    mu['gl_edit'] = [('float mu = dot( vr, viewMatrix[2] );', 'float mu = dot( vr, viewMatrix[2] ); fragColor = vec4( vr, mu ); return;')]
    mu['vk_edit'] = [('float mu = dot(vr, viewMatrix[2]);', 'float mu = dot(vr, viewMatrix[2]); fragColor = float4(vr, mu); return;')]
    probes.append(mu)
    MUV = {
        'dotxyz': [('float mu = dot(vr, viewMatrix[2]);',
                    'float mu = mad(vr.z, viewMatrix[2].z, mad(vr.y, viewMatrix[2].y, vr.x * viewMatrix[2].x));')],
        'dotyxz': [('float mu = dot(vr, viewMatrix[2]);',
                    'float mu = mad(vr.z, viewMatrix[2].z, mad(vr.x, viewMatrix[2].x, vr.y * viewMatrix[2].y));')],
        'dotzyx': [('float mu = dot(vr, viewMatrix[2]);',
                    'float mu = mad(vr.x, viewMatrix[2].x, mad(vr.y, viewMatrix[2].y, vr.z * viewMatrix[2].z));')],
        'dotplain': [('float mu = dot(vr, viewMatrix[2]);',
                      'precise float mu = vr.x * viewMatrix[2].x + vr.y * viewMatrix[2].y + vr.z * viewMatrix[2].z;')],
    }
    RAY = 'return normalize(float3((ndc.x + projectionMatrix[2].x) / projectionMatrix[0].x, (ndc.y + projectionMatrix[2].y) / projectionMatrix[1].y, -1.0f));'
    for k, new in {
        'rsq': 'float3 q = float3((ndc.x + projectionMatrix[2].x) / projectionMatrix[0].x, (ndc.y + projectionMatrix[2].y) / projectionMatrix[1].y, -1.0f); return q * rsqrt(dot(q, q));',
        'rcp': 'return normalize(float3((ndc.x + projectionMatrix[2].x) * (1.0f / projectionMatrix[0].x), (ndc.y + projectionMatrix[2].y) * (1.0f / projectionMatrix[1].y), -1.0f));',
        'rcprsq': 'float3 q = float3((ndc.x + projectionMatrix[2].x) * (1.0f / projectionMatrix[0].x), (ndc.y + projectionMatrix[2].y) * (1.0f / projectionMatrix[1].y), -1.0f); return q * rsqrt(dot(q, q));',
    }.items():
        probes.append(dict(mu, name='probe_ray_' + k, vk_edit=mu['vk_edit'] + [(RAY, new)]))
    for k, v in MUV.items():
        probes.append(dict(mu, name='probe_mu_' + k, vk_edit=v + [(v[0][1], v[0][1] + ' fragColor = float4(vr, mu); return;')]))
    return [j for j in all_ if not want or j['name'] in want] + [j for j in probes if j['name'] in want]
