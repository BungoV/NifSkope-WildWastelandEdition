# IMPOSTORFIN1 -- one before/after picture per subject, SAME sheets, SAME views:
#   mesh (new exe) | card BEFORE (rung exe) | card AFTER (new exe)
# The views are the subject's own bake directions nearest the horizon (the
# way a far tree is seen), four of them, distinct azimuths. Each card carries
# its silhouette IoU against the mesh grab and ink = card px / mesh px,
# measured off the grab pixels against the viewport background.
#   python compose_pairs.py SETROOT BEFOREGRABS AFTERGRABS OUTDIR LABEL
import sys, os, glob, json, math
import numpy as np
from PIL import Image, ImageDraw, ImageFont

FX, GB, GA, OUT, LABEL = sys.argv[1:6]
BG = np.array([43, 45, 49], float)
TAGS = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')


def font(sz):
    for p in ('C:/Windows/Fonts/consola.ttf', 'C:/Windows/Fonts/arial.ttf'):
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()


def views(N):
    out = []
    for j in range(N):
        for i in range(N):
            u = i / float(N - 1) * 2.0 - 1.0; v = j / float(N - 1) * 2.0 - 1.0
            x = (u + v) * 0.5; y = (u - v) * 0.5; z = 1.0 - abs(x) - abs(y)
            n = math.sqrt(x * x + y * y + z * z); x, y, z = x / n, y / n, z / n
            el = math.degrees(math.asin(max(-1.0, min(1.0, z))))
            az = math.degrees(math.atan2(y, x)) % 360.0
            out.append((i, j, az, el))
    return out


def grab(folder, az, el, kind):
    p = '%s/v_az%03d_el%02d_%s.png' % (folder, int(az), int(abs(el)), kind)
    return p if os.path.exists(p) else None


def mask(p):
    a = np.asarray(Image.open(p).convert('RGB')).astype(float)
    return np.abs(a - BG).sum(-1) > 12


def main():
    os.makedirs(OUT, exist_ok=True)
    f1, f2 = font(20), font(15)
    for t in TAGS:
        L = glob.glob('%s/%s/cards/*_oct.lodm' % (FX, t))
        if not L:
            continue
        raw = open(L[0], 'rb').read()
        card = json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
        N = int(card['oct']); fw, fh = card['frame']
        ga, gb = GA + '/' + t, GB + '/' + t
        vs = views(N)
        # nearest the horizon, then spread in azimuth
        vs = sorted(vs, key=lambda v: (abs(v[3]), v[2]))
        pick, seen = [], set()
        for v in vs:
            k = int(v[2]) // 45
            if k in seen:
                continue
            if all(grab(g, v[2], v[3], kd) for g, kd in ((ga, 'mesh'), (gb, 'card'), (ga, 'card'))):
                pick.append(v); seen.add(k)
            if len(pick) == 4:
                break
        if not pick:
            print('%-9s no complete view triple' % t); continue
        paths = []
        for v in pick:
            paths += [grab(ga, v[2], v[3], 'mesh'), grab(gb, v[2], v[3], 'card'), grab(ga, v[2], v[3], 'card')]
        m = np.zeros_like(mask(paths[0]))
        for p in paths:
            m |= mask(p)
        ys, xs = np.nonzero(m)
        x0, x1, y0, y1 = max(0, xs.min() - 6), xs.max() + 7, max(0, ys.min() - 6), ys.max() + 7
        cw, ch = x1 - x0, y1 - y0
        gap, top, cap = 10, 64, 40
        W = 16 + 3 * cw + 2 * gap + 16
        H = top + len(pick) * (ch + cap + gap)
        sheet = Image.new('RGB', (W, H), (24, 25, 28))
        dr = ImageDraw.Draw(sheet)
        dr.text((16, 10), '%s -- before / after, same sheets (%dx%d texel frames, N=%d), same views' % (t, fw, fh, N),
                font=f1, fill=(235, 235, 235))
        dr.text((16, 36), 'left: original mesh   middle: BEFORE (rung exe)   right: AFTER (new exe)   %s' % LABEL,
                font=f2, fill=(170, 172, 178))
        rows = []
        for r, v in enumerate(pick):
            y = top + r * (ch + cap + gap)
            pm, pb, pa = grab(ga, v[2], v[3], 'mesh'), grab(gb, v[2], v[3], 'card'), grab(ga, v[2], v[3], 'card')
            mm = mask(pm)
            txt = ['mesh  az %.0f el %.0f' % (v[2], v[3])]
            for p in (pb, pa):
                cm = mask(p)
                iou = (cm & mm).sum() / float(max(1, (cm | mm).sum()))
                ink = cm.sum() / float(max(1, mm.sum()))
                txt.append('IoU %.3f  ink %.2f' % (iou, ink))
            rows.append((v, txt[1], txt[2]))
            for k, p in enumerate((pm, pb, pa)):
                im = Image.open(p).convert('RGB').crop((x0, y0, x1, y1))
                sheet.paste(im, (16 + k * (cw + gap), y))
                dr.text((16 + k * (cw + gap) + 2, y + ch + 4), ('', 'before ', 'after ')[k] + txt[k],
                        font=f2, fill=((150, 200, 150), (200, 160, 140), (140, 200, 230))[k])
        s = 2400.0 / max(sheet.size)
        if s < 1.0:
            sheet = sheet.resize((int(sheet.size[0] * s), int(sheet.size[1] * s)), Image.LANCZOS)
        p = '%s/%s_before_after.png' % (OUT, t)
        sheet.save(p)
        print('%-9s %s  %dx%d  ' % (t, os.path.basename(p), sheet.size[0], sheet.size[1])
              + ' | '.join('az%.0f el%.0f %s -> %s' % (v[2], v[3], b, a) for v, b, a in rows))


if __name__ == '__main__':
    main()
