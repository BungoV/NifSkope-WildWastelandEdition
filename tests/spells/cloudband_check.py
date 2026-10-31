"""Lane CLOUDBAND1 (bungo 10-30, notes/cloudband1/bungo_cloud_band.png: "the clouds have banding", a regular DIAGONAL line
pattern over a cloud edge): offline gates of the volume clouds' temporal accumulation (WW_CLOUDVOL_ACCUM) through the real
driver, no NifSkope. The live view is played frame by frame as cloudvol.cpp drives it (stride 4, the 4x4 Bayer phase
order, cvFrame = frame % 64, the blue noise slice = frame / 16), through the REAL cloud_march*.prog and cloud_resolve*.prog.
CLOUD2's helpers come from cloudvol_check, CLOUD5's variety helpers from cloudvar_check (imported, none of their gates run).

    python -I tests/spells/cloudband_check.py <scratch dir> [--red noclamp|ign|noaccum|off|bayerdither] [--only DLMGQF]

Arms: today  = cloud_march.prog + cloud_resolve.prog (Off: IGN step offsets, one march a history pixel)
      accum  = cloud_march_a.prog + cloud_resolve_a.prog (WW_CLOUDVOL_ACCUM=1: blue noise offsets, one slice a refresh
               cycle, the fresh march blended into the clamped history, weight 1/4)
      today_v / accum_v = the same with Cloud Variety (cloud_march_v.prog; its offsets are already blue noise)
Reds: noaccum (the accum arm with weight 1: no accumulation), noclamp (the clamp off), ign (the accum arm's march keeps
CLOUD2's IGN offsets).

Measures (in 8-bit display levels: the light x an exposure that puts the reference's median cloud at 0.18, sRGB):
  E = the live history after 80 frames of a still camera minus the reference = the mean of 64 full marches of every
  history pixel at the same step count, the jitter of frames 0..63 (the value the accumulation must converge to).
  lattice = the largest FFT bin of E (Hann, 64 x 64 history px windows at least 60 % inside cloud) over the median of
  its frequency ring (0.04 cycles a px wide), 0.04 .. 0.7 cycles a px; floor = the same of 16 Gaussian white fields in
  the same windows (their max). Amended after the first run, bars unchanged: 128 px windows 85 % inside cloud found only
  2-4 windows at Medium (1/4 res history, 320 x 180), and a fixed weight 1/4 from the first frame left 0.75^5 = 0.24 of
  the first frame's upsampled march in the history after 5 refreshes (a 4-periodic startup pattern read as a period-2
  peak); the weight now runs 1, 1/2, 1/3, 1/4 over the refresh cycles after a history reset (cloudvol.cpp).

Gates (bars written before the first green run, CLOUDBAND1 10-30):
  D diag      INFO: which pattern makes the diagonal. The today arm's lattice and the peak's period and angle (history px
              and screen px), FOGGLOW1's dither hash (wwDitherHash TPDF, a numpy twin of the uint32 integer hash) over a
              512 x 512 field, IGN alone, IGN + the 4x4 Bayer slot's frame offset (the jitter a still history holds).
  L lattice   Medium (1/4 res) and Ultra (1/2 res) at 1280 x 720, Off and Variety skies: the accum arms' lattice
              <= 1.5 x floor; the today arm must FAIL the same bar (> 1.5 x floor), else the measure does not separate.
              Amended after the first run: the Variety sky's today arm has no lattice (CLOUD5 round 2 moved its offsets
              to blue noise; its own red is cloudvar_check B red ign), so the separation is asked of the Off sky only.
  M converge  the accum arms' rms(E) <= 0.6 x the today arms' rms(E) (an exponential mean of weight 1/4 holds sqrt(1/7)
              = 0.38 of one march's noise; 0.6 leaves room for the clamp's bias). Red noaccum fails.
  G ghost     a camera turning 0.4 deg and moving 0.5 m a frame; at frame 40 the cloud field jumps (a clearing: the map
              coverage 0.6 -> 0 in the Low band and 0.3 -> 0 in the Mid band, the history no longer matches anything). 16 frames later (one refresh of every
              history pixel) the error vs the NEW reference, mean |E| over the pixels the jump changed by > 8 levels:
              accum <= today + 2 levels (today shows each pixel's fresh march, no history kept). Red noclamp fails.
              Amended after the first red runs: the first case (coverage 0.6 -> 0.35 plus a 3 km wind step, bar accum
              <= 1.5 x today) let the red noclamp pass (15.03 vs bar 15.31; green 13.96), and a clearing to 0.05 still
              held thin cloud (mean opacity 0.5 on the changed px) in most 3x3 boxes: both kept as INFO. The box was
              tightened (variance clipping) after the same runs.
              Lane CLOUD6g: Move Clamp clamps every reprojected pixel under the moving camera to this frame's march
              box, so the moving clearing reads 0.00 with or without the history clamp and the red noclamp passed it
              (0.00). The same clearing is judged with the camera STILL at the last frame's view too (same bar):
              there only the fresh pixels' clamp (and Change Clamp inside it) removes the old field.
  F flicker   CLOUD2's gate D on the accum arm (its path, its metric, its bar 0.08): the history's frame-to-frame change
              minus the clouds' own, frames 17..47.
  Q dither    (lane CLOUDBAND2, LATE ITEM 2's first suspect) FOGGLOW1's TPDF dither added at the 8-bit write over the
              default arm, at screen px: lattice <= 1.5 x the screen floor; red bayerdither (an ordered 4x4 dither) fails.
Lane CLOUDBAND2 (Cloud Accumulation is the DEFAULT): the gates judge the "default" arms = the programs cloudvol.cpp
draws for this env (cloudband_dump's "default accum" line, cloudvolmath.h defaultOnSwitch); the "today" arms stay the
old path. Red off: the dump gets WW_CLOUDVOL_ACCUM=0, the default arm becomes the old path and L + M must FAIL.
Exit 0 only when every gate run passes.
"""
import ctypes, math, os, runpy, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudband_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('DLMGQF')
os.makedirs(scratch, exist_ok=True)

# CLOUD5's helpers (and through them CLOUD2's): cloudvar_check run with a gate letter it does not have runs none
sys.argv = [os.path.join(HERE, 'cloudvar_check.py'), scratch, '--only', 'Z']
V = runpy.run_path(os.path.join(HERE, 'cloudvar_check.py'), run_name='cloudband_helpers')
np, K = V['np'], V['K']
S, C, g, gl = K.S, K.C, K.g, K.gl
SDIR = os.path.join(TREE, 'res', 'shaders')
BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]   # cloudvol.cpp's phase order
TIERS = {'medium': (4, 48, 3, 1.0, 60.0), 'ultra': (2, 96, 6, 1.2, 100.0)}   # cloudvol.cpp preset(): div, steps, taps, reach, max
SW, SH = 1280, 720


import subprocess   # noqa: E402
DUMPB = os.path.join(scratch, 'cloudband_dump.exe')
_r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                     os.path.join(TREE, 'tests', 'cloudband', 'cloudband_dump.cpp'), '-o', DUMPB], env=V['ENV'],
                    capture_output=True, text=True)
if _r.returncode:
    raise SystemExit('BUILD FAIL ' + _r.stderr[-2000:])
# lane CLOUDBAND2: the dump also prints what cloudvol.cpp turns on for this env (cloudvolmath.h defaultOnSwitch); the red
# off hands it WW_CLOUDVOL_ACCUM=0, the old path
DENV = dict(V['ENV'], WW_CLOUDVOL_ACCUM='0') if red == 'off' else V['ENV']
# lane CLOUD6g: its reds hand the dump the old path of Move Clamp / Change Clamp (tests/spells/cloudstale_check.py)
if red == 'staleread':
    DENV = dict(DENV, WW_CLOUDVOL_MOVECLAMP='0')
if red == 'looseclamp':
    DENV = dict(DENV, WW_CLOUDVOL_CHANGECLAMP='0')
# lane CLOUD6h: its reds hand the dump the old path of History Age / Lit Clamp (tests/spells/cloudage_check.py)
if red == 'noage':
    DENV = dict(DENV, WW_CLOUDVOL_HISTAGE='0')
if red == 'litblind':
    DENV = dict(DENV, WW_CLOUDVOL_LITCLAMP='0')
_out = subprocess.run([DUMPB], env=DENV, capture_output=True, text=True, check=True).stdout.split('\n')
ACC = {int(l.split()[1]): float(l.split()[2]) for l in _out if l.startswith('acc ')}
_dl = [l.split() for l in _out if l.startswith('default ')][0]
DEF_ACCUM = int(_dl[2]) == 1
print('DEFAULT      INFO  cloudvol.cpp draws Cloud Accumulation %s for this env (dump: %s)'
      % ('ON' if DEF_ACCUM else 'OFF (the old path)', ' '.join(_dl)), flush=True)
# lane CLOUD6g: Move Clamp + Change Clamp as cloudvol.cpp sets them for this env; only the "default" arms draw them (the
# "today" / "accum" arms stay the paths before CLOUD6g: an unset uniform is 0 = the old read)
_gl = [l.split() for l in _out if l.startswith('cloud6g ')][0]
DEF_MOVE, DEF_RESET, DEF_RESETSD = float(int(_gl[2])), float(_gl[6]) if int(_gl[4]) else 0.0, float(_gl[8])
print('DEFAULT      INFO  Move Clamp %s, Change Clamp %s (dump: %s)' % ('ON' if DEF_MOVE else 'OFF', DEF_RESET or 'OFF',
                                                                     ' '.join(_gl)), flush=True)
# lane CLOUD6h: History Age + Lit Clamp as cloudvol.cpp sets them for this env (only the "default" arms draw them)
_hl = [l.split() for l in _out if l.startswith('cloud6h ')][0]
DEF_AGE = float(_hl[4]) if int(_hl[2]) else 0.0
DEF_AGEK, DEF_LIT, DEF_LITF = float(_hl[6]), float(_hl[10]) if int(_hl[8]) else 0.0, float(_hl[12])
print('DEFAULT      INFO  History Age %s, Lit Clamp %s (dump: %s)' % (DEF_AGE or 'OFF', DEF_LIT or 'OFF', ' '.join(_hl)),
      flush=True)


def new_uni(arm, accum):
    """lane CLOUD6g: the resolve uniforms of the shipped arms (cloudvol.cpp resolvePass)"""
    on = arm.startswith('default')   # the other arms set 0 explicitly: a program keeps its uniforms between draws
    u = {'cvMoveClamp': (DEF_MOVE if on else 0.0,)}
    if accum:
        u['cvAccReset'] = (DEF_RESET if on else 0.0,)
        u['cvAccResetSd'] = (DEF_RESETSD,)
        # lane CLOUD6h: History Age + Lit Clamp (0 = CLOUD6g's resolve in every other arm)
        u['cvHistAge'] = (DEF_AGE if on else 0.0,)
        u['cvAgeK'] = (DEF_AGEK,)
        u['cvLitReset'] = (DEF_LIT if on else 0.0,)
        u['cvLitFloor'] = (DEF_LITF,)
    return u


def alpha(fr):
    """cloudvolmath.h accumAlpha through the dump (the weight cloudvol.cpp hands the resolve) in refresh cycle fr / 16
    since the history began: 1, 1/2, 1/3, then 1/4; the red noaccum 1"""
    return 1.0 if red == 'noaccum' else ACC[min(fr // 16, max(ACC))]

f_linkst = S.fn('glGetProgramiv', None, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_int))


def prog(name):
    if not os.path.exists(os.path.join(SDIR, name)):
        return None
    p = C.build(SDIR, name)
    ok = ctypes.c_int()
    f_linkst(p, 0x8B82, ctypes.byref(ok))
    if not ok.value:
        raise SystemExit('LINK FAIL ' + name)
    return p


P = {n: prog(n) for n in ('cloud_march.prog', 'cloud_resolve.prog', 'cloud_march_v.prog', 'cloud_march_a.prog',
                           'cloud_resolve_a.prog', 'cloud_fly_a.prog')}
ARMS = {'today': ('cloud_march.prog', 'cloud_resolve.prog', False), 'accum': ('cloud_march_a.prog', 'cloud_resolve_a.prog', False),
        'today_v': ('cloud_march_v.prog', 'cloud_resolve.prog', True), 'accum_v': ('cloud_march_v.prog', 'cloud_resolve_a.prog', True)}
# lane CLOUDBAND2: the gates' "default" arms are the programs cloudvol.cpp draws by default (the dump's line): the accum
# twins since CLOUDBAND2; the red off (WW_CLOUDVOL_ACCUM=0) gives the old programs, which must FAIL L and M. The "today"
# arms stay the old path (the comparison base: today = main before CLOUDBAND2's default).
ARMS['default'] = ARMS['accum'] if DEF_ACCUM else ARMS['today']
ARMS['default_v'] = ARMS['accum_v'] if DEF_ACCUM else ARMS['today_v']
results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


SV = K.sky.view_lut(192, 108, 6360.05, K.R.sundir(30, 30))
VSKY = V['sky'](0.6, 0.2, cov=0.6)


def sky_uni(var, cov, wind):
    if var:
        s = dict(VSKY, cov=cov)
        u = V['var_uni'](s)
        u['cvBands.windKm'] = wind
        u['cvAmbKm'] = (5.0,)
        return u, s['calT']
    # the Mid band at 0.3 beside the Low band's cov; a Low coverage of 0 clears the Mid band too (gate G's clearing)
    return K.bands_uni(K.LOW, K.MID, K.cal, cov=(cov, 0.3 if cov > 0 else 0.0, 0.0), wind=wind), None


def run(pr, tgt, uni, texs, calT=None):
    g.bindfb(0x8D40, tgt.fb)
    gl.glViewport(0, 0, tgt.w, tgt.h)
    g.use(pr)
    K.setuni(pr, uni)
    if calT is not None:
        V['set_calT'](pr, calT)
    K.bind(pr, texs)
    gl.glDrawArrays(5, 0, 4)


def march_uni(tier, cam, pos, hw, hh, stride, ph, frame, slice_, var, accum_march, cov=0.6, wind=(0.0, 0.0, 0.0)):
    div, steps, taps, reach, mx = TIERS[tier]
    u, calT = sky_uni(var, cov, wind)
    u.update(K.march_uni(cam, pos, (hw, hh), stride, ph, 30.0, sun_az=30, steps=steps, taps=taps, drop=K.drop, amb=0.05,
                         ap=1.0, frame=float(frame), lightKm=V['light_km'](0.8, reach) if var else reach, maxKm=mx))
    if var or accum_march:
        u['cvBlueSlice'] = -1 if (red == 'ign' and accum_march) or slice_ < 0 else slice_
    return u, calT


def mtexs(var_or_blue):
    return V['vtexs'](K.trans, SV) if var_or_blue else K.march_texs(K.trans, SV)


def live(arm, tier, nframes, path, field=None, keep=None):
    """the live view: frame fr marches the history pixels of Bayer slot fr % 16 and resolves (cloudvol.cpp's order).
    path(fr) -> (cam, pos); field(fr) -> (cov, wind). keep(fr, hist) is called after each resolve."""
    mname, rname, var = ARMS[arm]
    pm, pr = P[mname], P[rname]
    accum = rname.endswith('_a.prog')
    div = TIERS[tier][0]
    hw, hh = (SW + div - 1) // div, (SH + div - 1) // div
    mw, mh = (hw + 3) // 4, (hh + 3) // 4
    march, hist = K.Mrt(mw, mh), [K.Mrt(hw, hh), K.Mrt(hw, hh)]
    prev, cur = None, 0
    for fr in range(nframes):
        cam, pos = path(fr)
        cov, wind = field(fr) if field else (0.6, (0.0, 0.0, 0.0))
        slot = BAYER[fr % 16]
        ph = (float(slot % 4), float(slot // 4))
        u, calT = march_uni(tier, cam, pos, hw, hh, 4, ph, fr % 64, (fr // 16) % 64, var, mname.endswith('_a.prog'), cov, wind)
        run(pm, march, u, mtexs(var or mname.endswith('_a.prog')), calT)
        ru = {'cvHistSize': (float(hw), float(hh)), 'cvStride': (4.0,), 'cvPhase': ph, 'cvCam': cam, 'cvProj': u['cvProj'],
              'cvCamKm': pos, 'cvHistOn': (1.0 if prev else 0.0,), 'cvReproj': (1.0,)}
        p0 = prev or (cam, u['cvProj'], pos)
        ru.update({'cvPrevCam': p0[0], 'cvPrevProj': p0[1], 'cvPrevCamKm': p0[2]})
        if accum:
            ru['cvAccAlpha'] = (alpha(fr),)
            ru['cvAccClamp'] = (0.0 if red == 'noclamp' else 1.0,)
        ru.update(new_uni(arm, accum))   # lane CLOUD6g
        run(pr, hist[cur], ru, [('cvMarch', 0x0DE1, march.col), ('cvMarchDepth', 0x0DE1, march.dep),
                                ('cvPrev', 0x0DE1, hist[1 - cur].col), ('cvPrevDepth', 0x0DE1, hist[1 - cur].dep)])
        if keep:
            keep(fr, hist[cur])
        prev = (cam, u['cvProj'], pos)
        cur = 1 - cur
    return hist[1 - cur].read()


def reference(arm, tier, cam, pos, n=64, cov=0.6, wind=(0.0, 0.0, 0.0)):
    """every history pixel marched (stride 1) at the live step count, the mean over the jitter of frames 0..n-1 (IGN with
    the golden-ratio frame offset: an even spread of offsets per pixel); the variety twin through its red ign path"""
    mname, _, var = ARMS[arm]
    div = TIERS[tier][0]
    hw, hh = (SW + div - 1) // div, (SH + div - 1) // div
    full = K.Mrt(hw, hh)
    acc = 0.0
    pm = P['cloud_march_v.prog' if var else 'cloud_march.prog']
    for f in range(n):
        u, calT = march_uni(tier, cam, pos, hw, hh, 1, (0.0, 0.0), f, -1, var, False, cov, wind)
        run(pm, full, u, mtexs(var), calT)
        acc = acc + full.read()
    return acc / n


LUMW = np.array([0.2126, 0.7152, 0.0722])


def srgb(v):
    v = np.clip(v, 0.0, 1.0)
    return np.where(v <= 0.0031308, 12.92 * v, 1.055 * np.power(np.maximum(v, 0.0031308), 1 / 2.4) - 0.055)


def display(img, expo):
    """the composite over a mid sky (premultiplied light + T x sky), exposed, sRGB 8-bit levels of the luminance"""
    return 255.0 * srgb((img[..., :3] @ LUMW + img[..., 3] * 0.05) * expo)


def exposure(ref):
    a = 1.0 - ref[..., 3]
    m = a > 0.3
    return 0.18 / max(float(np.median((ref[..., :3] @ LUMW)[m] / a[m])), 1e-9)


def windows(mask, size=64, need=0.6):
    h, w = mask.shape
    out = []
    for y in range(0, h - size + 1, size // 4):
        for x in range(0, w - size + 1, size // 2):
            if mask[y:y + size, x:x + size].mean() >= need:
                out.append((y, x))
    return out


def ring_peak(E, mask, wins, size=64):
    """the largest FFT bin over its ring's median, 0.04 .. 0.7 cycles a px, over the windows; returns (ratio, fx, fy)"""
    hann = np.outer(np.hanning(size), np.hanning(size))
    f = np.fft.fftshift(np.fft.fftfreq(size))
    FX, FY = np.meshgrid(f, f)
    R = np.hypot(FX, FY)
    ring = np.minimum((R / 0.04).astype(int), 18)   # rings 2.5 bins wide (a 64 px window's bins are 1/64 apart)
    band = (R >= 0.04) & (R <= 0.7)
    best = (0.0, 0.0, 0.0)
    for (y, x) in wins:
        e = E[y:y + size, x:x + size] * mask[y:y + size, x:x + size]
        m = mask[y:y + size, x:x + size]
        e = np.where(m, e - e[m].mean(), 0.0)
        Pw = np.abs(np.fft.fftshift(np.fft.fft2(e * hann))) ** 2
        med = np.zeros(19)
        for r in range(19):
            sel = (ring == r) & band
            if sel.any():
                med[r] = np.median(Pw[sel])
        Q = np.where(band, Pw / np.maximum(med[ring], 1e-30), 0.0)
        i = int(np.argmax(Q))
        if Q.flat[i] > best[0]:
            best = (float(Q.flat[i]), float(FX.flat[i]), float(FY.flat[i]))
    return best


def floor_of(mask, wins, n=16):
    rng = np.random.default_rng(1030)
    return max(ring_peak(rng.standard_normal(mask.shape), mask, wins)[0] for _ in range(n))


def describe(fx, fy, div):
    r = math.hypot(fx, fy)
    if r <= 0:
        return 'no peak'
    return 'period %.2f history px (%.2f screen px) normal %.0f deg' % (1 / r, div / r, math.degrees(math.atan2(fy, fx)))


def still(fr):
    return K.camera(0, 18), (0.0, 0.0, 0.05)


CACHE = {}
MEMO = {}


def measure(arm, tier):
    """memoised by (programs, tier) (lane CLOUDBAND2: gate Q reuses L's runs; a run is deterministic)"""
    mk = (ARMS[arm], tier, arm.startswith('default'))   # lane CLOUD6g: the default arms' uniforms differ, same programs
    if mk not in MEMO:
        MEMO[mk] = measure_(arm, tier)
    return MEMO[mk]


def measure_(arm, tier):
    var = ARMS[arm][2]
    key = (var, tier)
    if key not in CACHE:
        cam, pos = still(0)
        ref = reference(arm, tier, cam, pos)
        mask = (1.0 - ref[..., 3]) > 0.15
        er = mask.copy()
        for ax in (0, 1):
            for sh in (-2, 2):
                er &= np.roll(mask, sh, axis=ax)
        wins = windows(er)
        CACHE[key] = (ref, exposure(ref), er, wins, floor_of(er, wins) if wins else 0.0)
    ref, expo, er, wins, flo = CACHE[key]
    L = live(arm, tier, 80, still)
    E = display(L, expo) - display(ref, expo)
    pk = ring_peak(E, er, wins) if wins else (0.0, 0.0, 0.0)
    rms = float(np.sqrt(np.mean(E[er] ** 2)))
    return pk, flo, rms, len(wins), int(er.sum()), E, L, ref, expo


def png(path, v):
    import zlib, struct
    a = np.clip(np.round(v), 0, 255).astype(np.uint8)[::-1]
    if a.ndim == 2:
        a = np.repeat(a[..., None], 3, axis=2)
    raw = b''.join(b'\x00' + row.tobytes() for row in a)

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', a.shape[1], a.shape[0], 8, 2, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


# ---- D: which pattern makes the diagonal ---------------------------------------------------------------------------------
if 'D' in only:
    n = 512
    yy, xx = np.mgrid[0:n, 0:n].astype(np.uint32)
    M = np.uint32(0xFFFFFFFF)

    def dhash(px, py, s):
        """wwDitherHash (ww_dither.glsl), uint32 arithmetic"""
        with np.errstate(over='ignore'):
            h = (px * np.uint32(1664525) + py * np.uint32(1013904223) + np.uint32(s) * np.uint32(2654435769)) & M
            h ^= h >> np.uint32(16)
            h = (h * np.uint32(2246822519)) & M
            h ^= h >> np.uint32(13)
            h = (h * np.uint32(3266489917)) & M
            h ^= h >> np.uint32(16)
        return (h >> np.uint32(8)).astype(np.float64) / 16777216.0
    full = np.ones((n, n), bool)
    wins = [(y, x) for y in range(0, n - 63, 64) for x in range(0, n - 63, 64)]
    flo = floor_of(full, wins)
    tp = dhash(xx, yy, 2 * 7) + dhash(xx, yy, 2 * 7 + 1) - 1.0
    qx, qy = xx.astype(np.float64), yy.astype(np.float64)
    ign = np.mod(52.9829189 * np.mod(qx * 0.06711056 + qy * 0.00583715, 1.0), 1.0)
    inv = {s: f for f, s in enumerate(BAYER)}
    kslot = np.vectorize(lambda s: inv[s])((xx % 4) + 4 * (yy % 4)).astype(np.float64)
    held = np.mod(ign + kslot * 0.618034, 1.0)   # frame k of the slot: cvFrame k (k < 16)
    for name, fld in (('dither TPDF (FOGGLOW1)', tp), ('IGN alone', ign), ('IGN + Bayer slot frame offset', held)):
        pk = ring_peak(fld, full, wins)
        print('D diag       INFO  %-30s lattice %.1f (floor %.1f) %s' % (name, pk[0], flo, describe(pk[1], pk[2], 1)), flush=True)
    for tier in ('medium', 'ultra'):
        pk, fl, rms, nw, npx, E, L, ref, expo = measure('today', tier)
        print('D diag       INFO  today %-6s live history lattice %.1f (floor %.1f, %d windows) %s; rms(E) %.2f levels'
              % (tier, pk[0], fl, nw, describe(pk[1], pk[2], TIERS[tier][0]), rms), flush=True)
        png(os.path.join(scratch, 'today_%s_E.png' % tier), 128 + 16 * E)
        png(os.path.join(scratch, 'today_%s_live.png' % tier), display(L, expo))
    print('D diag       INFO  bungo crop (screen px): period 4.58 px, normal -22 deg (y down), x488 over the median bin '
          '(notes/code1030/cloudband1/tools/crop_fft.py)')

# ---- L + M: the lattice and the convergence ----------------------------------------------------------------------------
if ('L' in only or 'M' in only) and P['cloud_resolve_a.prog'] is not None:
    for tier in ('medium', 'ultra'):
        for base, acc in (('today', 'default'), ('today_v', 'default_v')):
            t = measure(base, tier)
            a = measure(acc, tier)
            div = TIERS[tier][0]
            if 'L' in only:
                sep = base == 'today_v' or t[0][0] > 1.5 * t[1]   # the variety sky's offsets are blue noise already
                gate('L %s %s' % (tier[:3], acc), a[0][0] <= 1.5 * a[1] and sep,
                     'lattice %s %.1f (%s) vs %s %.1f (%s); bar <= 1.5 x floor %.1f, %s (%d windows)'
                     % (acc, a[0][0], describe(a[0][1], a[0][2], div), base, t[0][0], describe(t[0][1], t[0][2], div),
                        a[1], 'INFO: today_v has no lattice (CLOUD5 round 2 blue noise)' if base == 'today_v'
                        else 'the today arm must exceed it', a[3]))
            if 'M' in only:
                gate('M %s %s' % (tier[:3], acc), a[2] <= 0.6 * t[2],
                     'rms(E) %s %.2f vs %s %.2f levels: ratio %.3f (bar 0.6)%s' % (acc, a[2], base, t[2], a[2] / max(t[2], 1e-9),
                                                                          ' (red %s)' % red if red else ''))
            png(os.path.join(scratch, '%s_%s_E.png' % (acc, tier)), 128 + 16 * a[5])
            png(os.path.join(scratch, '%s_%s_live.png' % (acc, tier)), display(a[6], a[8]))

# ---- G: the clamp rejects a history that no longer matches --------------------------------------------------------------
if 'G' in only and P['cloud_resolve_a.prog'] is not None:
    JUMP, AFTER = 40, 16

    def path(fr):
        return K.camera(20 + 0.4 * fr, 18), (0.0005 * fr, 0.0, 0.05)

    tier = 'medium'
    cam, pos = path(JUMP + AFTER - 1)
    expo = exposure(reference('today', tier, cam, pos, n=16))
    for case, cov1, wind1 in (('clearing', 0.0, (0.0, 0.0, 0.0)), ('thinning', 0.05, (0.0, 0.0, 0.0)),
                              ('shift', 0.35, (3.0, 0.0, 0.0))):
        def field(fr):
            return (0.6, (0.0, 0.0, 0.0)) if fr < JUMP else (cov1, wind1)
        old = reference('today', tier, cam, pos, n=16)
        new = reference('today', tier, cam, pos, n=64, cov=cov1, wind=wind1)
        changed = np.abs(display(new, expo) - display(old, expo)) > 8.0
        err = {}
        for arm in ('today', 'default'):
            L = live(arm, tier, JUMP + AFTER, path, field)
            err[arm] = float(np.abs(display(L, expo) - display(new, expo))[changed].mean())
            png(os.path.join(scratch, 'ghost_%s_%s.png' % (case, arm)), display(L, expo))
        msg = ('%s: %d px changed by the jump (mean %.1f levels); %d frames after it mean |E| vs the new field: accum %.2f, '
               'today %.2f levels' % (case, int(changed.sum()), float(np.abs(display(new, expo) - display(old, expo))[changed].mean()),
                                      AFTER, err['default'], err['today']))
        if case == 'clearing':
            gate('G ghost', err['default'] <= err['today'] + 2.0, msg + ' (bar accum <= today + 2 levels)%s'
                 % (' (red %s)' % red if red else ''))
            # lane CLOUD6g: the same clearing with the camera still (Move Clamp does not act; the history clamp must)
            def spath(fr):
                return path(JUMP + AFTER - 1)
            es = {}
            for arm in ('today', 'default'):
                L = live(arm, tier, JUMP + AFTER, spath, field)
                es[arm] = float(np.abs(display(L, expo) - display(new, expo))[changed].mean())
            gate('G ghost', es['default'] <= es['today'] + 2.0, 'clearing, camera still: %d frames after it mean |E| vs '
                 'the new field: accum %.2f, today %.2f levels (bar accum <= today + 2 levels)%s'
                 % (AFTER, es['default'], es['today'], ' (red %s)' % red if red else ''))
        else:
            print('G ghost      INFO  ' + msg + ' (a 3x3 box of march texels 4 history px apart holds part of a textured old '
                  'field: the clamp rejects what no neighbour has)', flush=True)

# ---- Q: FOGGLOW1's dither on the default (accumulated) clouds, at screen res (lane CLOUDBAND2) ---------------------------
def dither_hash(px, py, s):
    """wwDitherHash (res/shaders/ww_dither.glsl), uint32 arithmetic (gate D's twin, at module level)"""
    M32 = np.uint32(0xFFFFFFFF)
    with np.errstate(over='ignore'):
        h = (px * np.uint32(1664525) + py * np.uint32(1013904223) + np.uint32(s) * np.uint32(2654435769)) & M32
        h ^= h >> np.uint32(16)
        h = (h * np.uint32(2246822519)) & M32
        h ^= h >> np.uint32(13)
        h = (h * np.uint32(3266489917)) & M32
        h ^= h >> np.uint32(16)
    return (h >> np.uint32(8)).astype(np.float64) / 16777216.0


def upsample(a, w, h):
    """the composite's bilinear read of the history (cloud_composite.frag texture( cvHist, uv )) at w x h screen px"""
    ah, aw = a.shape
    x = np.clip((np.arange(w) + 0.5) * aw / w - 0.5, 0.0, aw - 1.0)
    y = np.clip((np.arange(h) + 0.5) * ah / h - 0.5, 0.0, ah - 1.0)
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    x1, y1 = np.minimum(x0 + 1, aw - 1), np.minimum(y0 + 1, ah - 1)
    fx, fy = (x - x0)[None, :], (y - y0)[:, None]
    top = a[y0][:, x0] * (1 - fx) + a[y0][:, x1] * fx
    bot = a[y1][:, x0] * (1 - fx) + a[y1][:, x1] * fx
    return top * (1 - fy) + bot * fy


if 'Q' in only:
    """LATE ITEM 2's first banding suspect: FOGGLOW1's TPDF dither (ww_dither.glsl wwDither8, added at the 8-bit write in
    cell_hdr.frag / taa2.frag / game_taa.frag). Bars written before the first run (CLOUDBAND2 10-30): the default arm's
    live history (gate L's run) composited at screen res (bilinear), the dither added (+-1 level TPDF, faded at 0 and 255
    as wwDither8, seed 79 = the frame index) and rounded to 8 bits; E = that minus the reference (float, no dither).
    PASS iff its lattice <= 1.5 x the screen floor. Red bayerdither: a 4x4 ordered dither of the same amplitude (a
    lattice) in its place must FAIL. INFO: the same without the dither, the today arm through the dither (the measure
    still sees the old lattice), the dither field alone.
    Amended after the first run (default arm only, no red run yet): the floor was 16 white HISTORY fields through the
    same bilinear upsample; the upsample's falling spectrum put it at 28454.8 (Medium) / 4084.4 (Ultra), a bar nothing
    could fail. The floor is now 16 white fields at SCREEN px (the dither's domain, as gate D); the bar (1.5 x) unchanged.
    The upsampled floor stays an INFO number."""
    SEED = 79
    for tier in ('medium', 'ultra'):
        div = TIERS[tier][0]
        res = {}
        for arm in ('default', 'today'):
            pk, fl, rms, nw, npx, E, L, ref, expo = measure(arm, tier)
            dL, dR = display(L, expo), display(ref, expo)
            uL, uR = upsample(dL, SW, SH), upsample(dR, SW, SH)
            er = CACHE[(ARMS[arm][2], tier)][2]
            mS = upsample(er.astype(np.float64), SW, SH) > 0.999
            wins = windows(mS)
            if len(wins) > 48:   # evenly thinned: the ring measure costs ~10 ms a window
                wins = [wins[i] for i in np.linspace(0, len(wins) - 1, 48).astype(int)]
            yy, xx = np.mgrid[0:SH, 0:SW].astype(np.uint32)
            if red == 'bayerdither':
                B4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float64)
                n = ((B4[yy % 4, xx % 4] + 0.5) / 16.0 - 0.5) * 2.0
            else:
                n = dither_hash(xx, yy, 2 * SEED) + dither_hash(xx, yy, 2 * SEED + 1) - 1.0
            wfade = np.clip(np.minimum(uL, 255.0 - uL), 0.0, 1.0)
            Ed = np.round(uL + n * wfade) - uR
            Eq = np.round(uL) - uR
            if 'flo' not in res:   # white fields at SCREEN px (the dither's own domain; see the amendment above)
                rng = np.random.default_rng(2030)
                res['flo'] = max(ring_peak(rng.standard_normal(mS.shape), mS, wins)[0] for _ in range(16))
                res['floup'] = max(ring_peak(upsample(rng.standard_normal(dL.shape), SW, SH), mS, wins)[0] for _ in range(4))
            res[arm] = (ring_peak(Ed, mS, wins), ring_peak(Eq, mS, wins), len(wins))
            if arm == 'default':
                res['field'] = ring_peak(n, mS, wins)
                png(os.path.join(scratch, 'q_%s_%s_Ed.png' % (arm, tier)), 128 + 16 * Ed)
        d, t, flo = res['default'], res['today'], res['flo']
        gate('Q %s dither' % tier[:3], d[0][0] <= 1.5 * flo,
             'default arm + %s, 8-bit, screen px: lattice %.1f (%s); bar <= 1.5 x screen floor %.1f (%d windows)%s'
             % ('ordered 4x4 dither (red)' if red == 'bayerdither' else 'FOGGLOW1 TPDF dither', d[0][0],
                describe(d[0][1], d[0][2], 1), flo, d[2], ' (red %s)' % red if red else ''))
        print('Q %s dither  INFO  default arm without the dither %.1f (%s); dither field alone %.1f (%s); today arm + dither '
              '%.1f (%s), without %.1f; floor of upsampled white history fields %.1f; bungo crop: period 4.58 screen px, '
              'normal -22 deg'
              % (tier[:3], d[1][0], describe(d[1][1], d[1][2], 1), res['field'][0], describe(res['field'][1], res['field'][2], 1),
                 t[0][0], describe(t[0][1], t[0][2], 1), t[1][0], res['floup']), flush=True)

# ---- F: CLOUD2's flicker gate D on the accum arm -------------------------------------------------------------------------
if 'F' in only and P['cloud_resolve_a.prog'] is not None:
    W, H, ST = 256, 144, 4
    mw, mh = (W + ST - 1) // ST, (H + ST - 1) // ST
    march, hist, full = K.Mrt(mw, mh), [K.Mrt(W, H), K.Mrt(W, H)], K.Mrt(W, H)
    sv = K.sky.view_lut(192, 108, 6360.05, K.R.sundir(30, 35))
    prev, cur, flick, flickref = None, 0, [], []
    for fr in range(48):
        cam, pos = K.camera(20 + 0.4 * fr, 12), (0.0005 * fr, 0.0, 0.3)
        slot = BAYER[fr % 16]
        ph = (float(slot % 4), float(slot // 4))
        base = dict(K.bands_uni(K.LOW, K.MID, K.cal, cov=(0.55, 0.3, 0.0)),
                    **K.march_uni(cam, pos, (W, H), ST, ph, 35.0, sun_az=30, steps=64, taps=6, frame=float(fr % 64),
                                  drop=K.drop, amb=0.05))
        base['cvBlueSlice'] = -1 if red == 'ign' else (fr // 16) % 64
        run(P[ARMS['default'][0]], march, base, V['vtexs'](K.trans, sv) if DEF_ACCUM else K.march_texs(K.trans, sv))
        ru = {'cvHistSize': (float(W), float(H)), 'cvStride': (float(ST),), 'cvPhase': ph, 'cvCam': cam, 'cvProj': base['cvProj'],
              'cvCamKm': pos, 'cvHistOn': (1.0 if prev else 0.0,), 'cvReproj': (1.0,),
              'cvAccAlpha': (alpha(fr),), 'cvAccClamp': (0.0 if red == 'noclamp' else 1.0,)}
        p0 = prev or (cam, base['cvProj'], pos)
        ru.update({'cvPrevCam': p0[0], 'cvPrevProj': p0[1], 'cvPrevCamKm': p0[2]})
        ru.update(new_uni('default', DEF_ACCUM))   # lane CLOUD6g
        run(P[ARMS['default'][1]], hist[cur], ru, [('cvMarch', 0x0DE1, march.col), ('cvMarchDepth', 0x0DE1, march.dep),
                                                      ('cvPrev', 0x0DE1, hist[1 - cur].col), ('cvPrevDepth', 0x0DE1, hist[1 - cur].dep)])
        if fr >= 16:
            r = hist[cur].read()
            lf = 0.0
            for jf in range(8):
                run(P['cloud_march.prog'], full, dict(base, cvStride=(1.0,), cvPhase=(0.0, 0.0), cvFrame=(float(jf),)),
                    K.march_texs(K.trans, sv))
                lf = lf + full.read()[..., :3] @ LUMW / 8.0
            lr = r[..., :3] @ LUMW
            if fr > 16:
                flick.append(float(np.abs((lr - lprev) - (lf - lfprev)).mean() / max(lf.mean(), 1e-9)))
                flickref.append(float(np.abs(lf - lfprev).mean() / max(lf.mean(), 1e-9)))
            lprev, lfprev = lr, lf
        prev = (cam, base['cvProj'], pos)
        cur = 1 - cur
    e = float(np.mean(flick))
    gate('F flicker', e <= 0.08, 'default arm (%s):' % ARMS['default'][1] + ' excess frame-to-frame change %.4f over 31 frame pairs (CLOUD2 gate D bar 0.08; '
         'max %.4f; the clouds themselves change %.4f a frame)%s' % (e, max(flick), float(np.mean(flickref)),
                                                                   ' (red %s)' % red if red else ''))

print('CLOUDBAND-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
