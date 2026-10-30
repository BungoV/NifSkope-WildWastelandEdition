"""Lane CLOUDBAND1 item (b) (RDR2 look, bungo 10-29/10-30): the sky's ambient light inside thick cloud. Offline gates through
the real cloud_march(_v).prog, no NifSkope; CLOUD2's helpers and Monte Carlo from cloudvol_check (imported, none of its gates
run), CLOUD5's variety helpers from cloudvar_check.

    python -I tests/spells/cloudamb_check.py <scratch dir> [--red flatamb|off] [--only AE]
(lane CLOUDBAND2: On is the default; the gate judges the arm cloudvol.cpp draws for this env, red off = the old ramp.)

Off (today): the march lights each sample with the sky's mean radiance x a height ramp 0.5 .. 1 over the band, so a deck's
base gets half the open sky's light however thick the deck is (a dense deck's underside stays sky-blue). On
(WW_CLOUDVOL_AMBDARK=1, uniform cvAmbDark 1): the ambient reaching a sample is the open sky's x the slab above it's diffuse
transmission, exp(-tau / 0.6) + Tdif(tau, mu 0.6) (two-stream, as the sun's floor; 0.6 = the diffusivity cosine), tau
from three taps straight up to the top of the sample's band. Bauer 2019 (RDR2) darkens the ambient by a sun-ward sample;
straight up is the column the sky light crosses (a low sun's sample would see the deck's side).

Gates (bars written before the first run, CLOUDBAND1 10-30):
  A ambient   a uniform slab (cvSlab) of tau 2.5, 10 and 40, the sun off (key 0), the sky's ambient 1, seen straight up from
              under it: the underside radiance (the march's light + the sky x its transmittance, the composite) / the
              ambient within 0.67 .. 1.5 x a Monte Carlo of the same slab lit by a
              uniform sky (the droplet phase, albedo 0.99: CLOUD2's mc_underside over 8 Gauss-Legendre beam cosines).
              Off (the red flatamb, and today's programs) must FAIL at tau 40. Both cloud_march.prog and cloud_march_v.prog.
              Amended after the first run (today's programs only, before any On run): the march's light alone left out
              the sky seen through the slab, which the Monte Carlo counts; the bar is unchanged.
  E cost      INFO: GPU ms of the march at 1080p Ultra (1/2 res, 1/16 of the pixels), overcast, Off vs On.
Exit 0 only when every gate run passes.
"""
import ctypes, math, os, runpy, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudamb_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('AE')
os.makedirs(scratch, exist_ok=True)
sys.argv = [os.path.join(HERE, 'cloudvar_check.py'), scratch, '--only', 'Z']
V = runpy.run_path(os.path.join(HERE, 'cloudvar_check.py'), run_name='cloudamb_helpers')
np, K = V['np'], V['K']
S, C, g, gl = K.S, K.C, K.g, K.gl
# lane CLOUDBAND2: the column ambient is the DEFAULT. The arm judged is what cloudvol.cpp turns on for this env
# (tests/cloudband/cloudband_dump.cpp's "default ... ambdark" line, cloudvolmath.h defaultOnSwitch); the red off hands
# the dump WW_CLOUDVOL_AMBDARK=0 (the old ramp), the red flatamb forces the ramp in the shader. Both must FAIL A.
import subprocess   # noqa: E402
DUMPB = os.path.join(scratch, 'cloudband_dump.exe')
_r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                     os.path.join(TREE, 'tests', 'cloudband', 'cloudband_dump.cpp'), '-o', DUMPB], env=V['ENV'],
                    capture_output=True, text=True)
if _r.returncode:
    raise SystemExit('BUILD FAIL ' + _r.stderr[-2000:])
_dl = [l.split() for l in subprocess.run([DUMPB], env=dict(V['ENV'], WW_CLOUDVOL_AMBDARK='0') if red == 'off' else V['ENV'],
                                         capture_output=True, text=True, check=True).stdout.split('\n')
       if l.startswith('default ')][0]
DEF_AMB = int(_dl[4]) == 1
print('DEFAULT      INFO  cloudvol.cpp draws the column ambient %s for this env (dump: %s)'
      % ('ON' if DEF_AMB else 'OFF (the old ramp)', ' '.join(_dl)), flush=True)
ON = 0.0 if red == 'flatamb' or not DEF_AMB else 1.0
results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


P_M = K.P_MARCH
P_MV = V['P_MV']


def mc_sky(tau, n=60000):
    """the underside radiance straight down per unit sky radiance, the sky uniform over the upper hemisphere:
    2 pi x the integral over mu of the beam's (CLOUD2's mc_underside: radiance per unit normal irradiance)"""
    x, w = np.polynomial.legendre.leggauss(8)
    mu = 0.5 * (x + 1.0)
    return float(sum(2 * math.pi * 0.5 * wi * K.mc_underside(tau, float(m), K.drop, n=n, seed=11 + i)
                     for i, (m, wi) in enumerate(zip(mu, w))))


def under(prog, var, tau, on):
    W, H = 32, 32
    tgt = K.Mrt(W, H)
    slab = (1.5, 1.5 + tau / K.SIGMA, 1.0)
    if var:
        s = V['sky'](0.0, 0.0, cov=1.0, base0=slab[0], depth=slab[1] - slab[0])
        u = V['var_uni'](s)
        u['cvBands.topKm'] = (slab[1], s['top1'], 9.2)   # the slab's top (a type-0 sky's own top is lower)
        u['cvAmbKm'] = (slab[1] - slab[0],)
        u['cvBlueSlice'] = -1
        calT = s['calT']
        texs = V['vtexs'](K.trans, K.trans)
    else:
        u = K.bands_uni(slab, (9.0, 9.1, 0.0), K.cal, cov=(1.0, 0.0, 0.0))
        calT = None
        texs = K.march_texs(K.trans, K.trans)
    u.update(K.march_uni(K.camera(180, 89.5), (0, 0, 0.5), (W, H), 1, (0, 0), 60.0, steps=192, taps=6, slab=1.0,
                         drop=K.drop, vfov=10.0, amb=1.0))
    u['cvKeyK'] = (0.0,)
    u['cvSunK'] = (0.0,)
    u['cvAmbDark'] = (on,)
    g.bindfb(0x8D40, tgt.fb)
    gl.glViewport(0, 0, W, H)
    g.use(prog)
    K.setuni(prog, u)
    if calT is not None:
        V['set_calT'](prog, calT)
    K.bind(prog, texs)
    gl.glDrawArrays(5, 0, 4)
    o = tgt.read()
    # the march's light plus the sky seen through it (the composite: sky x T, the sky's radiance = the ambient, 1): the
    # Monte Carlo's photons straight down include the ones that crossed unscattered
    c = o[12:20, 12:20].reshape(-1, 4).mean(0)
    return float(c[:3] @ np.array([0.2126, 0.7152, 0.0722]) + c[3])


if 'A' in only:
    rows, ok = [], True
    for tau in (2.5, 10.0, 40.0):
        ref = mc_sky(tau)
        for name, prog, var in (('march', P_M, False), ('march_v', P_MV, True)):
            got, off = under(prog, var, tau, ON), under(prog, var, tau, 0.0)
            r, ro = got / max(ref, 1e-12), off / max(ref, 1e-12)
            ok &= 0.67 <= r <= 1.5
            if tau == 40.0 and red != 'flatamb':
                ok &= not (0.67 <= ro <= 1.5)   # the Off ramp must fail the thick deck, else the gate cannot tell
            rows.append('tau %g %s: on %.4g off %.4g vs MC %.4g (ratio on %.3f, off %.3f)' % (tau, name, got, off, ref, r, ro))
    gate('A ambient', ok, '; '.join(rows) + ' (bar on 0.67 .. 1.5 x MC at every tau; off must fail at tau 40)%s'
         % (' (red %s)' % red if red else ''))

if 'E' in only:
    q = ctypes.c_uint()
    S.fn('glGenQueries', None, ctypes.c_int, ctypes.POINTER(ctypes.c_uint))(1, ctypes.byref(q))
    begq = S.fn('glBeginQuery', None, ctypes.c_uint, ctypes.c_uint)
    endq = S.fn('glEndQuery', None, ctypes.c_uint)
    getq = S.fn('glGetQueryObjectui64v', None, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint64))
    sv = K.sky.view_lut(192, 108, 6360.05, K.R.sundir(30, 35))
    W, H = 960, 540
    march = K.Mrt((W + 3) // 4, (H + 3) // 4)
    lines = []
    for name, prog, var in (('march', P_M, False), ('march_v', P_MV, True)):
        ms = []
        for on in (0.0, 1.0):
            if var:
                s = V['sky'](0.9, 0.25, cov=0.6, dens1=0.6)
                u = V['var_uni'](s)
                u['cvAmbKm'] = (5.0,)
                u['cvBlueSlice'] = -1
                texs, calT = V['vtexs'](K.trans, sv), s['calT']
            else:
                u = K.bands_uni(K.LOW, K.MID, K.cal, cov=(0.8, 0.3, 0.0))
                texs, calT = K.march_texs(K.trans, sv), None
            u.update(K.march_uni(K.camera(10, 8), (0, 0, 0.05), (W, H), 4, (1.0, 2.0), 35.0, sun_az=30, steps=96, taps=6,
                                 drop=K.drop, amb=0.05, ap=1.0, lightKm=1.2, maxKm=100.0))
            u['cvAmbDark'] = (on,)
            g.bindfb(0x8D40, march.fb)
            gl.glViewport(0, 0, march.w, march.h)
            g.use(prog)
            K.setuni(prog, u)
            if calT is not None:
                V['set_calT'](prog, calT)
            K.bind(prog, texs)
            gl.glDrawArrays(5, 0, 4)
            gl.glFinish()
            begq(0x88BF, q)
            for _ in range(20):
                gl.glDrawArrays(5, 0, 4)
            endq(0x88BF)
            ns = ctypes.c_uint64()
            getq(q, 0x8866, ctypes.byref(ns))
            ms.append(ns.value / 1e6 / 20)
        lines.append('%s off %.3f ms, on %.3f ms' % (name, ms[0], ms[1]))
    print('E cost       INFO  Ultra march at 1080p (this machine\'s GPU, offline): ' + '; '.join(lines), flush=True)

print('CLOUDAMB-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
