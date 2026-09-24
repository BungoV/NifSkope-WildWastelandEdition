# impostor_light_check.py -- the octahedral card's LIGHTING rows
# (tests/spells/impostor_draw.sh step 16). Lane IMPOSTORLIGHT1, 2026-09-22.
# bungo: "why is the lighting on the imposter wrong? Normals issue?" and then
# "Just fix it".
#
#   python impostor_light_check.py LITDIR NRMDIR NVIEWS [ALBDIR]
#
# Both folders hold one WW_IMPOSTOR_PREVIEW=orbit run at the SAME views (the
# bake directions, so the card is a photograph of the mesh there and any
# difference is the drawing's): <stem>_azAAA_elEE_mesh.png / _card.png.
#   LITDIR  the lit grabs, both halves as a user sees them;
#   NRMDIR  the NORMAL grabs: the mesh through LOD channel 8
#           (WW_IMPOSTOR_MESH_CHANNEL=8, the view-space geometric normal --
#           the picture the bake stores) and the card through its debug
#           channel 13 (WW_IMPOSTOR_CHANNEL=13, the normal its LIGHT is dotted
#           with, in view space). Both packed n*0.5+0.5.
#   ALBDIR  the ALBEDO grabs (optional; the TRANSFER row needs it): the mesh
#           through LOD channel 12 (raw base colour, unlit) and the card
#           through debug channel 1 (its colour sheet, unlit). This is the
#           rung the lit transfer is scored against: no lighting can make the
#           card follow the mesh better than its own colour sheet does.
#
# THE MASK is the intersection of the two NORMAL silhouettes, never of the lit
# ones. A normal colour is never the clear colour; a DARK lit pixel is -- the
# harness's own background rule (within 12 of (43,45,49) on every channel)
# read 6.06 % of the old card's covered pixels as background on blast_n4 and
# 44.05 % on the dead tree, which is exactly the population this row is about.
#
# Every bar below was set BEFORE the fix and is written with the numbers both
# sides of it (cardRes 512, bake directions, blend on):
#
#   NORMALS. Sign agreement per axis, counted where |mesh component| > 0.15 so
#   an honestly-zero component cannot vote; mean angle.
#     bf6aa749 (the model-space normal its lighting used): x 47.8..50.8 %,
#       y 40.6..50.4 %, z 66.9..83.8 %, angle 68.5..87.8 deg on the five
#       subjects -- a coin toss on x and y;
#     fixed: blast_n4 96.7 / 94.2 / 95.9 %, 11.1 deg; the worst subject, the
#       leafy maple, 86.1 / 80.3 / 96.5 %, 21.9 deg.
#     BAR: >= 80 % on every axis and mean angle <= 25 deg.
#
#   BRIGHTNESS. Mean Rec.709 luma card / mesh over the mask, tonemapped.
#     bf6aa749 0.475 / 0.481 / 0.541 / 0.513 / 0.877;
#     fixed    0.953 / 0.954 / 0.966 / 0.984 / 0.967.
#     BAR: 0.90 .. 1.10.
#
#   TRANSFER (the card must get brighter where the mesh does; the old one was
#   flat on blast, 57 -> 58 luma across the mesh's 57 -> 190, and fell on the
#   maple). Spearman rho over the mask pixels, and the card's mean over the
#   mesh's upper five luma deciles minus its mean over the lower five.
#   The bar is RELATIVE to the colour sheet, as registered before the fix
#   ("lit rho >= the albedo rung's rho minus 0.10"): the same rho measured on
#   the ALBDIR pair is the most the lighting can be asked to reach, because a
#   sheet whose colour does not follow the mesh cannot be lit into following
#   it. An absolute bar was tried and withdrawn: the impostor_draw fixture
#   (an older bake, 000531b3) has an albedo rho of 0.071, deciles 102..105 --
#   its colour sheet barely follows the mesh at all.
#     res512 bake, blast_n4: albedo rho 0.47; bf6aa749 lit 0.145, rise 2.1;
#       fixed 0.520, rise 19.3.
#     impostor_draw fixture: albedo rho 0.071, rise 1.7; fixed lit 0.194, 6.4.
#     BAR: rho >= albedo rho - 0.10, and rho > 0 and rise > 0 (it rises and
#     does not invert).
#   What this does NOT gate, said: the card's TOP decile is flat on blast
#   (129 -> 129) because its ALBEDO is -- the mesh's brightest decile is its
#   palest bark (albedo 156) and the card's albedo there is 110, as in the
#   decile below. No lighting reaches that; it is the colour sheet's.
#
# Floors on the instrument itself, so a broken run cannot pass:
#   * the view count equals NVIEWS in both folders;
#   * the mesh NORMAL grab is not the mesh LIT grab (an exe that ignores
#     WW_IMPOSTOR_MESH_CHANNEL photographs the lit mesh twice): >= 50 % of the
#     mesh's covered pixels differ by more than 12 levels;
#   * the mesh normals face the camera (mean view-space z >= 0.30), which a
#     lit picture misread as a normal does not reliably do.
import glob, os, sys
import numpy as np
from PIL import Image

BG = np.array([43, 45, 49], float)
W = np.array([0.2126, 0.7152, 0.0722])
DEAD = 0.15

SIGN_BAR, ANGLE_BAR = 0.80, 25.0
RATIO_LO, RATIO_HI = 0.90, 1.10
RHO_SLACK = 0.10

fails = 0


def say(ok, text):
    global fails
    if not ok:
        fails += 1
    print(('  ok   ' if ok else '  FAIL ') + text)


def img(p):
    return np.asarray(Image.open(p).convert('RGB')).astype(float)


def covered(a):
    return np.abs(a - BG).sum(-1) > 12


def bgrule(a):
    return (np.abs(a - BG) <= 12).all(-1)


def unpack(a):
    n = a / 255.0 * 2.0 - 1.0
    return n / np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-6)


def rank(x):
    r = np.empty(len(x))
    r[np.argsort(x, kind='stable')] = np.arange(len(x))
    return r


def views(d):
    return sorted(os.path.basename(p)[:-len('_mesh.png')] for p in glob.glob(os.path.join(d, '*_mesh.png'))
                  if os.path.exists(p[:-len('_mesh.png')] + '_card.png'))


def transfer(lm, lc):
    q = np.percentile(lm, np.linspace(0, 100, 11))
    idx = np.clip(np.searchsorted(q, lm, side='right') - 1, 0, 9)
    dec = [float(lc[idx == i].mean()) if (idx == i).any() else float('nan') for i in range(10)]
    rise = float(np.nanmean(dec[5:]) - np.nanmean(dec[:5]))
    rho = float(np.corrcoef(rank(lm), rank(lc))[0, 1]) if lm.std() > 0 and lc.std() > 0 else float('nan')
    return rho, rise, dec


def main(lit, nrm, nviews, alb=None):
    vl, vn = views(lit), views(nrm)
    say(len(vl) == nviews and len(vn) == nviews,
        'the view count: %d lit pairs and %d normal pairs, %d asked for' % (len(vl), len(vn), nviews))
    common = [v for v in vn if v in vl]
    if not common:
        say(False, 'no view is in both folders -- nothing below can be measured')
        return
    ag, cnt = np.zeros(3), np.zeros(3)
    ang, lm, lc, am_, ac_ = [], [], [], [], []
    differ = mcov = 0
    zsum = zpx = 0.0
    darkc = covc = 0
    for v in common:
        mn, cn = img(os.path.join(nrm, v + '_mesh.png')), img(os.path.join(nrm, v + '_card.png'))
        ml, cl = img(os.path.join(lit, v + '_mesh.png')), img(os.path.join(lit, v + '_card.png'))
        km, kc = covered(mn), covered(cn)
        mcov += int(km.sum())
        differ += int((np.abs(mn - ml).max(-1) > 12)[km].sum())
        k = km & kc
        if k.sum() < 50:
            continue
        a, b = unpack(mn[k]), unpack(cn[k])
        zsum += float(a[:, 2].sum()); zpx += len(a)
        for i in range(3):
            s = np.abs(a[:, i]) > DEAD
            ag[i] += int((np.sign(a[s, i]) == np.sign(b[s, i])).sum()); cnt[i] += int(s.sum())
        ang.append(np.degrees(np.arccos(np.clip((a * b).sum(-1), -1.0, 1.0))))
        lm.append(ml[k] @ W); lc.append(cl[k] @ W)
        if alb:
            pa, pc = os.path.join(alb, v + '_mesh.png'), os.path.join(alb, v + '_card.png')
            if os.path.exists(pa) and os.path.exists(pc):
                am_.append(img(pa)[k] @ W); ac_.append(img(pc)[k] @ W)
        darkc += int((kc & bgrule(cl)).sum()); covc += int(kc.sum())

    share = differ / max(1, mcov)
    say(share >= 0.50, 'the mesh NORMAL grab is not its lit grab: %.1f %% of %d covered pixels differ'
        ' (floor 50 %%; an exe that ignores WW_IMPOSTOR_MESH_CHANNEL reads ~0)' % (100 * share, mcov))
    zm = zsum / max(1.0, zpx)
    say(zm >= 0.30, 'the mesh normals face the camera: mean view-space z %.3f (floor 0.30)' % zm)
    if not ang:
        say(False, 'no view had 50 pixels in both normal silhouettes')
        return

    sx, sy, sz = ag / np.maximum(cnt, 1)
    am = float(np.concatenate(ang).mean())
    say(min(sx, sy, sz) >= SIGN_BAR and am <= ANGLE_BAR,
        'NORMAL sign agreement card vs mesh: x %.1f %% y %.1f %% z %.1f %%, mean angle %.1f deg'
        ' (bar >= %.0f %% every axis, <= %.0f deg; bf6aa749 read 40.6..50.8 %% on x/y)'
        % (100 * sx, 100 * sy, 100 * sz, am, 100 * SIGN_BAR, ANGLE_BAR))

    lm, lc = np.concatenate(lm), np.concatenate(lc)
    ratio = float(lc.mean() / max(lm.mean(), 1e-6))
    say(RATIO_LO <= ratio <= RATIO_HI,
        'BRIGHTNESS card/mesh %.3f (mesh %.1f, card %.1f luma over %d px; bar %.2f..%.2f; bf6aa749 read 0.475)'
        % (ratio, lm.mean(), lc.mean(), len(lm), RATIO_LO, RATIO_HI))

    rho, rise, dec = transfer(lm, lc)
    if not alb:
        say(False, 'TRANSFER rises: no ALBDIR given, so the albedo rung this row is scored against is missing'
            ' (lit rho %.3f, rise %.1f)' % (rho, rise))
    elif not am_:
        say(False, 'TRANSFER rises: the ALBDIR holds none of the views -- the albedo rung is missing')
    else:
        arho, arise, adec = transfer(np.concatenate(am_), np.concatenate(ac_))
        bar = arho - RHO_SLACK
        say(rho >= bar and rho > 0 and rise > 0,
            'TRANSFER rises: Spearman rho %.3f, upper-half minus lower-half %.1f luma'
            ' (bar rho >= the colour sheet rho %.3f - %.2f = %.3f, and rho, rise > 0; bf6aa749 read 0.145 and 2.1'
            ' on blast_n4)' % (rho, rise, arho, RHO_SLACK, bar))
        print('  info colour sheet (unlit) card luma by mesh decile: ' + ' '.join('%.0f' % d for d in adec)
              + ' (rise %.1f)' % arise)
    print('  info card luma by mesh decile: ' + ' '.join('%.0f' % d for d in dec))
    print('  info dark card pixels the harness background rule drops: %.2f %% of %d' % (100.0 * darkc / max(1, covc), covc))


if __name__ == '__main__':
    if len(sys.argv) not in (4, 5):
        print('usage: impostor_light_check.py LITDIR NRMDIR NVIEWS [ALBDIR]')
        sys.exit(2)
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4] if len(sys.argv) == 5 else None)
    print('light check: %d failure(s)' % fails)
    sys.exit(1 if fails else 0)
