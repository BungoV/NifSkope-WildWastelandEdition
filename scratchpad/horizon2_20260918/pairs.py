# Lane HORIZON2 step 5 -- the BEFORE|AFTER pages.
#
#   python pairs.py main     -> before_after_<framing>_e<el>_a<az>.png   (12)
#   python pairs.py control  -> control_beside_real.png
#   python pairs.py raycast  -> before_after_raycast_<framing>_e<el>_a<az>.png
#
# CONSTITUTION 5: the same framing, before and after, in one page, with the
# number the report quotes printed on the picture it came from. The BEFORE
# panels are lane HORIZON1's own renders of the 21:59:46 exe's bake, copied
# unmodified into images/before/ -- not re-rendered, because a re-render of the
# old bytes by the new exe would be a different picture in a second way.
#
# Every caption number is read out of the render's OWN log (the viewer's note
# line for the role-7 sheet), not typed in, so a picture cannot disagree with
# its caption.
import os
import re
import sys

from PIL import Image, ImageDraw

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
IMG = LANE + '/images'
BEF = IMG + '/before'
PAD, BAR, TOP = 8, 54, 30
INK = (240, 240, 245)
DIM = (170, 170, 178)
BG = (18, 18, 21)

NOTE = re.compile(r'TERRAIN HORIZON SHEET[^\n]*?horizon ([0-9.]+)\.\.([0-9.]+) deg'
                  r'[^\n]*?mean ([0-9.]+)[^\n]*?([0-9.]+)%+ lit')


def note(logpath):
    """The viewer's own sentence about the role-7 sheet: (lo, hi, mean, lit)."""
    if not os.path.exists(logpath):
        return None
    m = NOTE.search(open(logpath, 'rb').read().decode('utf-8', 'replace'))
    if not m:
        return None
    return tuple(float(g) for g in m.groups())


def fmt(n):
    if n is None:
        return 'no note line in the log'
    return 'terrain horizon %.2f..%.2f deg, mean %.2f, %.1f%% lit' % n


def page(panels, title, out, sub=''):
    """panels: list of (png path, heading, caption). One fixed cell, captions at
    the cell's top-left, nothing clipped."""
    ims = [Image.open(p).convert('RGB') for p, _h, _c in panels]
    cw = max(i.width for i in ims)
    ch = max(i.height for i in ims)
    W = len(ims) * cw + (len(ims) + 1) * PAD
    H = TOP + (1 if sub else 0) * 18 + ch + TOP + BAR + 2 * PAD
    out_im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(out_im)
    d.text((PAD, 8), title, fill=INK)
    if sub:
        d.text((PAD, 24), sub, fill=DIM)
    y0 = TOP + (18 if sub else 0)
    for k, (im, (_p, h, c)) in enumerate(zip(ims, panels)):
        x = PAD + k * (cw + PAD)
        d.text((x, y0), h, fill=INK)
        out_im.paste(im, (x + (cw - im.width) // 2, y0 + 20))
        yy = y0 + 20 + ch + 6
        for line in c.split('\n'):
            d.text((x, yy), line, fill=DIM)
            yy += 14
    out_im.save(out)
    print('%-52s %d panel(s)  %dx%d' % (os.path.basename(out), len(ims), W, H))


def main():
    made = 0
    for framing in ('close', 'full'):
        for az in (120, 240):
            for el in (5, 15, 30):
                stem = 'chunk_horizon_%s_e%02d_a%d' % (framing, el, az)
                b, a = '%s/%s' % (BEF, stem), '%s/%s' % (IMG, stem)
                if not (os.path.exists(b + '.png') and os.path.exists(a + '.png')):
                    print('  SKIP %s -- missing a panel' % stem)
                    continue
                nb, na = note(b + '.log'), note(a + '.log')
                page([(b + '.png', 'BEFORE -- exe 21:59:46, the sector-max footprint',
                       fmt(nb) + '\nthe stored byte is the MAXIMUM over the bin\'s whole 22.5 deg sector'),
                      (a + '.png', 'AFTER -- exe 23:47:33, one square a tap',
                       fmt(na) + '\neach stored byte is the skyline in its own direction, which is what the viewer blends')],
                     'Chunk 4.4.-12, %s framing, sun azimuth %d deg elevation %d deg -- terrain horizon channel'
                     % (framing, az, el),
                     '%s/before_after_%s_e%02d_a%d.png' % (IMG, framing, el, az),
                     'WW_LODL_CHANNEL=horizon. Same camera, same scene, same channel; the only difference is '
                     'the bytes in Commonwealth.VT.4.lodt.')
                made += 1
    print('%d before/after pages' % made)


def control():
    real = '%s/chunk_horizon_close_e15_a120' % IMG
    ctrl = '%s/chunk_horizon_close_e15_a120_CONTROL' % IMG
    if not os.path.exists(ctrl + '.png'):
        print('  SKIP control -- no CONTROL render')
        return
    page([(real + '.png', 'REAL -- the sun\'s own azimuth (120 deg)', fmt(note(real + '.log'))),
          (ctrl + '.png', 'CONTROL -- the same bytes read a quarter turn away (WW_HORIZON_BIN_ROT=4)',
           fmt(note(ctrl + '.log')) + '\nif this were hard to tell from its neighbour, the azimuth is not '
           'being read and every picture here is decoration')],
         'The red control, beside the real picture -- chunk 4.4.-12, close framing, sun 120,15, AFTER exe',
         '%s/control_beside_real.png' % IMG)


def raycast():
    made = 0
    for framing in ('close', 'full'):
        for az in (120, 240):
            for el in (5, 15, 30):
                stem = 'horizon_vs_raycast_%s_e%02d_a%d.png' % (framing, el, az)
                b, a = '%s/%s' % (BEF, stem), '%s/%s' % (IMG, stem)
                if not (os.path.exists(b) and os.path.exists(a)):
                    print('  SKIP %s -- missing a panel' % stem)
                    continue
                page([(b, 'BEFORE -- exe 21:59:46', 'every dot is a sampled receiver where the stored bins and the '
                       'in-bake reference disagree about THIS sun'),
                      (a, 'AFTER -- exe 23:47:33', 'the reference moved too (it calls the same maxAlong), so this '
                       'page shows agreement between two instruments, not correctness')],
                     'Where the bins and the in-bake reference disagree -- %s framing, sun %d,%d'
                     % (framing, az, el),
                     '%s/before_after_raycast_%s_e%02d_a%d.png' % (IMG, framing, el, az),
                     'RED = terrain texel, ORANGE = LOD vertex. Camera measured from this lane\'s own '
                     'calibration renders, not assumed.')
                made += 1
    print('%d raycast pages' % made)


if __name__ == '__main__':
    mode = sys.argv[1] if len(sys.argv) > 1 else 'main'
    {'main': main, 'control': control, 'raycast': raycast}[mode]()
