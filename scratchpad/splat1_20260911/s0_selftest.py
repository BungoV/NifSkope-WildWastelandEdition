"""SPLAT1 section 0 -- the instruments, tested on inputs whose answer is known.

Nothing in sections 1-4 may be believed before every line here reads PASS.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
import splatlib as S                                      # noqa: E402
from lodgen_terrain_model import Dds as RefDds            # noqa: E402

ok = 0
bad = 0


def chk(name, cond, detail=''):
    global ok, bad
    if cond:
        ok += 1
        print('PASS  %-52s %s' % (name, detail))
    else:
        bad += 1
        print('FAIL  %-52s %s' % (name, detail))


# --- D1: the decoder against the INDEPENDENT one already in the tree --------
for path, tag in ((S.van_sheet(-20, 24), 'vanilla DXT5'),
                  (S.OURS[(-20, 24)], 'ours DXT1'),
                  (S.van_sheet(-20, 24, '_msn'), 'vanilla msn')):
    a = S.Dds(path)
    b = RefDds(path)
    px, w, h = b._level(0)
    ref = (np.array(px, dtype=np.float32).reshape(h, w, 4) * 255.0)
    mine = a.level(0)
    d = np.abs(mine - ref).max()
    chk('D1 decoder == tests/spells decoder, %s' % tag, d <= 1.01,
        'maxabs %.4f of 255, %dx%d %s, mips %d' % (d, w, h, a.fourcc.decode(),
                                                   a.maxMip + 1))

# --- D2: the metrics on known answers --------------------------------------
h = w = 256
const = np.full((h, w), 90.0)
lv = S.local_var(const).mean()
chk('D2a local_var(constant) == 0', lv < 1e-9, 'mean local var %.3e' % lv)

rng = np.random.default_rng(7)
wn = rng.standard_normal((h, w)) * 10.0 + 90.0
lv = S.local_var(wn).mean()
# a 3x3 window of iid samples has E[sample var] = sigma^2 * (n-1)/n = 100*8/9
chk('D2b local_var(white noise sigma=10) == 88.9', abs(lv - 88.9) < 3.0,
    'mean local var %.2f, theory 88.89' % lv)

sm = S.smooth_field(h, w, cells=32)
lv_sm = S.local_var(sm).mean()
sp = sm + S.checker(h, w, period=6, amp=12.0)
lv_sp = S.local_var(sp).mean()
chk('D2c metric separates smooth from 6-texel checker',
    lv_sp / max(lv_sm, 1e-9) > 20.0,
    'smooth %.3f -> speckled %.3f = %.1fx' % (lv_sm, lv_sp, lv_sp / max(lv_sm, 1e-9)))

# --- D3: Parseval -- the radial power sums to the windowed variance ---------
ctr, pw = S.radial_power(sm)
wind = np.outer(np.hanning(h), np.hanning(w))
f = (sm - sm.mean()) * wind
chk('D3 sum(radial power) == var(windowed field)',
    abs(pw.sum() - f.var()) / max(f.var(), 1e-12) < 0.02,
    'sum %.5f vs var %.5f' % (pw.sum(), f.var()))

# --- D4: the band table finds a period it was given ------------------------
ctr, pw = S.radial_power(sp)
p, med, ratio = S.peak_at(ctr, pw, S.checker_radius(6))
chk('D4 spectrum finds the planted 6-texel period', ratio > 5.0,
    'bin power %.4f vs local median %.4f = %.1fx' % (p, med, ratio))

# --- D5: the phase twin keeps the energy and loses the structure ------------
tw = S.phase_twin(sp, seed=11)
chk('D5a phase twin keeps the variance', abs(tw.var() / sp.var() - 1.0) < 0.05,
    'subject var %.2f, twin var %.2f' % (sp.var(), tw.var()))
chk('D5b phase twin keeps the local variance too',
    abs(S.local_var(tw).mean() / S.local_var(sp).mean() - 1.0) < 0.10,
    'subject %.2f, twin %.2f -- SO local variance alone cannot tell '
    'structure from noise; the band table must' %
    (S.local_var(sp).mean(), S.local_var(tw).mean()))

# --- D6: the BC1 emulation, checked on VANILLA'S OWN BYTES -----------------
van = S.Dds(S.van_sheet(-20, 24)).level(0)
rt = S.bc1_roundtrip(van[:, :, :3])
rms = float(np.sqrt(((rt - van[:, :, :3]) ** 2).mean()))
chk('D6a BC1 emulation reproduces an already-BC field', rms < 4.0,
    're-encode of vanilla rms %.2f/255 -- vanilla is ALREADY block-coded, so '
    'this says the emulation agrees with the shipped encoder, not what a '
    'codec costs' % rms)

# THE CODEC FLOOR: how much local variance a BC1 block codec ADDS to a field
# that has none of its own. This is the number the verdict needs, and it can
# only be measured on a field that was never block-coded.
smooth3 = np.dstack([S.smooth_field(256, 256, cells=32, seed=k, amp=20.0,
                                    mean=90.0) for k in (3, 4, 5)])
lv_pre = S.local_var(S.lum(np.dstack([smooth3, np.full((256, 256), 255.0)]))).mean()
cod = S.bc1_roundtrip(smooth3)
lv_post = S.local_var(S.lum(np.dstack([cod, np.full((256, 256), 255.0)]))).mean()
chk('D6b codec floor measured on a never-coded smooth field', lv_post > lv_pre,
    'smooth %.3f -> after BC1 %.3f  (the codec adds %.3f)'
    % (lv_pre, lv_post, lv_post - lv_pre))
print('      CODEC FLOOR (local var a BC1 codec adds to smooth data) = %.3f'
      % (lv_post - lv_pre))

print('')
print('selftest %d ok, %d fail' % (ok, bad))
sys.exit(1 if bad else 0)
