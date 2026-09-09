#!/usr/bin/env python3
"""THE TRANSITION GATE: does the card stand where the model stood?

bungo, 2026-09-09: "then the tree must be positioned correctly, so that when a
3d tree transitions to an imposter, the tree won't change position".

The SOURCE half is a real render through NifSkope's render hook, orthographic,
camera pointed at the card's own centre (`card.center` -- the offset from the
object's PIVOT).  The CARD half is drawn from the shipped `.lodm` alone --
`center`, `half`, `frame`, `pad`, `oct` -- exactly as a reader would place the
quad: at `pivot + center`, spanning `+-half`.  Both are then measured the same
way and their silhouette bounding boxes must coincide.

CONTROL, which must FAIL: the same card placed at the pivot, i.e. `center`
treated as zero, which is what a reader that ignores the field would do.

Usage:  transition_gate.py <cardsdir> <renderdir> <outdir> <id> [<id> ...]
The renders must already exist as <renderdir>/<id>_src_<tag>.png.
"""
import sys, os, json, struct, math
import numpy as np
from PIL import Image, ImageDraw

COV = 16                 # the spec's coverage floor


def lodm(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'LODM'
    n = struct.unpack('<I', b[8:12])[0]
    return json.loads(b[12:12 + n].decode('utf-8'))


def frame_of(sheet, N, fw, fh, i, j):
    return sheet[j * fh:(j + 1) * fh, i * fw:(i + 1) * fw]


def bbox(mask):
    if not mask.any():
        return None
    ys, xs = np.where(mask)
    return (float(xs.min()), float(xs.max()) + 1.0, float(ys.min()), float(ys.max()) + 1.0)


def source_mask(img):
    """Silhouette of a rendered model against the viewport background.

    The background is modelled PER ROW from the ten outermost columns on each
    side (the model is centred, and a vertical gradient background would defeat
    a single modal colour).  A pixel is the model where it differs from its own
    row's background by more than THRESH on any channel.
    """
    a = np.asarray(img.convert('RGB')).astype(np.int32)
    h, w = a.shape[:2]
    edge = np.concatenate([a[:, :10, :], a[:, -10:, :]], axis=1)
    bg = np.median(edge, axis=1)                     # (h, 3)
    d = np.abs(a - bg[:, None, :]).max(axis=2)
    THRESH = 24
    return d > THRESH, THRESH


def card_layer(sheet, N, fw, fh, i, j, rect, size):
    """The card frame scaled into `rect` of an image of `size`, RGBA."""
    f = frame_of(sheet, N, fw, fh, i, j)
    im = Image.fromarray(f, 'RGBA')
    x0, y0, x1, y1 = rect
    tw, th = max(1, int(round(x1 - x0))), max(1, int(round(y1 - y0)))
    im = im.resize((tw, th), Image.NEAREST)
    out = Image.new('RGBA', size, (0, 0, 0, 0))
    out.paste(im, (int(round(x0)), int(round(y0))))
    return out


def run(cardsdir, renderdir, outdir, ids, dists):
    os.makedirs(outdir, exist_ok=True)
    rows = []
    for cid in ids:
        m = lodm(os.path.join(cardsdir, cid + '_oct.lodm'))
        c = m['card']
        N = c['oct']
        fw, fh = c['frame']
        HW, HH = c['half']
        C = c['center']
        pad = c.get('pad', [max(4, max(fw, fh) // 16)] * 2)
        alb = np.asarray(Image.open(os.path.join(cardsdir, cid + '_oct_albedo.png'))
                         .convert('RGBA'))
        # ViewFront is frame (0, N-1) under the hemi-octahedral mapping:
        # u=-1, v=+1 -> dx=0, dy=-1, dz=0 -> azim -90 -> (rx, rz) = (-90, 180)
        fi, fj = 0, N - 1
        for tag in ('mid', 'ring'):
            dist = dists[cid][tag]
            src_path = os.path.join(renderdir, '%s_src_%s.png' % (cid, tag))
            if not os.path.exists(src_path):
                print('MISSING render %s' % src_path)
                continue
            img = Image.open(src_path)
            W, H = img.size
            smask, thresh = source_mask(img)
            sb = bbox(smask)
            # orthographic: setDistance() IS the half-height
            upp = 2.0 * dist / float(H)
            # the quad, placed at pivot + center: the camera looks AT that point
            rect = (W / 2.0 - HW / upp, H / 2.0 - HH / upp,
                    W / 2.0 + HW / upp, H / 2.0 + HH / upp)
            lay = card_layer(alb, N, fw, fh, fi, fj, rect, (W, H))
            cmask = np.asarray(lay)[:, :, 3] >= COV
            cb = bbox(cmask)
            # THE CONTROL: `center` treated as zero. The camera still looks at
            # C, so the quad lands lower/aside by the projection of -C. In the
            # ViewFront frame the view's right is world +x and the view's up is
            # world +z, so the screen displacement is (-Cx, +Cz) / upp.
            crect = (rect[0] - C[0] / upp, rect[1] + C[2] / upp,
                     rect[2] - C[0] / upp, rect[3] + C[2] / upp)
            lay0 = card_layer(alb, N, fw, fh, fi, fj, crect, (W, H))
            c0b = bbox(np.asarray(lay0)[:, :, 3] >= COV)

            def cmp(a, b):
                if a is None or b is None:
                    return None
                acx, acy = (a[0] + a[1]) / 2, (a[2] + a[3]) / 2
                bcx, bcy = (b[0] + b[1]) / 2, (b[2] + b[3]) / 2
                aw, ah = a[1] - a[0], a[3] - a[2]
                bw, bh = b[1] - b[0], b[3] - b[2]
                return dict(dcx=bcx - acx, dcy=bcy - acy,
                            dw=(bw - aw) / aw * 100.0, dh=(bh - ah) / ah * 100.0,
                            src=[aw, ah], card=[bw, bh])

            # one texel of the CARD's own resolution, in image pixels, here
            texel_px = (2.0 * HW / fw) / upp
            good = cmp(sb, cb)
            bad = cmp(sb, c0b)
            rows.append(dict(id=cid, tag=tag, dist=dist, size=[W, H], thresh=thresh,
                             texel_px=texel_px, frame=[fw, fh], pad=pad,
                             half=[HW, HH], center=C, good=good, control=bad))
            print('%s %-5s D=%-7.0f texel=%.2fpx  card vs source: dcx %+.2fpx dcy %+.2fpx  dw %+.2f%% dh %+.2f%%'
                  % (cid, tag, dist, texel_px, good['dcx'], good['dcy'], good['dw'], good['dh']))
            print('      CONTROL (center zeroed):        dcx %+.2fpx dcy %+.2fpx  dw %+.2f%% dh %+.2f%%'
                  % (bad['dcx'], bad['dcy'], bad['dw'], bad['dh']))

            # the picture: source | card | overlay
            panel = Image.new('RGB', (W * 3, H + 26), (18, 18, 20))
            panel.paste(img.convert('RGB'), (0, 0))
            cardimg = Image.new('RGB', (W, H), (18, 18, 20))
            cardimg.paste(lay.convert('RGB'), (0, 0), lay)
            panel.paste(cardimg, (W, 0))
            ov = img.convert('RGB').copy()
            ov.paste(lay.convert('RGB'), (0, 0), lay)
            panel.paste(ov, (2 * W, 0))
            d = ImageDraw.Draw(panel)
            for k, bx, col in ((0, sb, (80, 220, 120)), (1, cb, (240, 170, 60)),
                               (2, sb, (80, 220, 120)), (2, cb, (240, 170, 60))):
                if bx:
                    d.rectangle([k * W + bx[0], bx[2], k * W + bx[1], bx[3]], outline=col)
            d.text((6, H + 7), 'SOURCE %s  ortho half-height %.0f' % (cid, dist), fill=(200, 220, 200))
            d.text((W + 6, H + 7), 'CARD from the .lodm  frame %dx%d pad %d,%d' % (fw, fh, pad[0], pad[1]),
                   fill=(240, 200, 140))
            d.text((2 * W + 6, H + 7),
                   'OVERLAY  centre %+.2f,%+.2f px (1 card texel = %.2f px)  extents %+.2f%%, %+.2f%%'
                   % (good['dcx'], good['dcy'], texel_px, good['dw'], good['dh']), fill=(220, 220, 220))
            panel.save(os.path.join(outdir, 'transition_%s_%s.png' % (cid, tag)))
    json.dump(rows, open(os.path.join(outdir, 'transition.json'), 'w'), indent=1)
    # the verdict
    print()
    ok = True
    for r in rows:
        g, b = r['good'], r['control']
        pass_c = abs(g['dcx']) <= r['texel_px'] and abs(g['dcy']) <= r['texel_px']
        pass_e = abs(g['dw']) <= 2.0 and abs(g['dh']) <= 2.0
        ctl_fails = (abs(b['dcx']) > r['texel_px'] or abs(b['dcy']) > r['texel_px'])
        print('%s %-5s  centre %s  extents %s  control-fails %s'
              % (r['id'], r['tag'], 'PASS' if pass_c else 'FAIL',
                 'PASS' if pass_e else 'FAIL', 'PASS' if ctl_fails else 'FAIL'))
        ok = ok and pass_c and pass_e and ctl_fails
    print('TRANSITION GATE:', 'PASS' if ok else 'FAIL')


if __name__ == '__main__':
    cards, rend, out = sys.argv[1], sys.argv[2], sys.argv[3]
    ids = sys.argv[4:]
    dists = json.load(open(os.path.join(rend, 'dists.json')))
    run(cards, rend, out, ids, dists)
