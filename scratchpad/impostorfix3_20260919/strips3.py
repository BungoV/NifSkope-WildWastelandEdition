"""IMPOSTORFIX3's pictures. Every tile is a grab the in-application harness
wrote itself -- never a screen capture -- and every row is labelled inside the
image so a strip cannot be read as the wrong thing six weeks from now.

The `cards_r3` column is the card as exe 161568a5 left it (the IMPOSTORFIX1
height repair: the object's own depth inside the silhouette, the card plane
everywhere outside). This lane re-measured that column on its own exe and
reproduced IMPOSTORFIX1's published IoU to four digits, which is why it can
stand in for that exe's picture.

Azimuth 0..330 in 30s, elevations 15 and 45 -- the SAME views everything else
in this lane is measured on, never a chosen subset.
"""
import os
from PIL import Image, ImageDraw, ImageFont

R = os.path.dirname(os.path.abspath(__file__))
C = R + '/control'
OUT = R + '/images'
os.makedirs(OUT, exist_ok=True)
AZ = list(range(0, 360, 30))
BG, FG, DIM = (24, 25, 28), (216, 216, 210), (150, 150, 155)


def font(sz, bold=False):
    p = 'C:/Windows/Fonts/' + ('arialbd.ttf' if bold else 'arial.ttf')
    return ImageFont.truetype(p, sz) if os.path.exists(p) else ImageFont.load_default()


def grab(tag, az, el, kind):
    p = '%s/%s/v_az%03d_el%02d_%s.png' % (C, tag, az, el, kind)
    return Image.open(p).convert('RGB') if os.path.exists(p) else None


def strip(rows, el, path, title, scale=0.42):
    probe = grab(rows[0][1], AZ[0], el, rows[0][2])
    if probe is None:
        print('  MISSING', rows[0][1]); return
    w, h = probe.size
    tw, th = int(w * scale), int(h * scale)
    lab, hdr, top = 150, 22, 24
    img = Image.new('RGB', (lab + tw * len(AZ), top + hdr + th * len(rows)), BG)
    d = ImageDraw.Draw(img)
    fb, fs = font(14, True), font(12)
    d.text((6, 5), title, font=fb, fill=FG)
    for k, a in enumerate(AZ):
        d.text((lab + k * tw + 4, top + 5), 'az %d' % a, font=fs, fill=DIM)
    for r, (label, tag, kind) in enumerate(rows):
        d.text((6, top + hdr + r * th + th // 2 - 8), label, font=fb, fill=FG)
        for k, a in enumerate(AZ):
            g = grab(tag, a, el, kind)
            if g is not None:
                img.paste(g.resize((tw, th)), (lab + k * tw, top + hdr + r * th))
    img.save(path)
    print('  wrote', os.path.basename(path), img.size)


def gif(tag, kind, el, path, scale=0.5):
    frames = []
    for a in AZ:
        g = grab(tag, a, el, kind)
        if g is None:
            continue
        frames.append(g.resize((int(g.width * scale), int(g.height * scale))))
    if frames:
        frames[0].save(path, save_all=True, append_images=frames[1:],
                       duration=200, loop=0)
        print('  wrote', os.path.basename(path), len(frames), 'frames')


SUBJ = (
    ('blast_n4', '00_before_after_blast_n4', 'TreeMapleblasted05 N=4', '0.5038', '0.5736'),
    ('blast_n8', '00_before_after_blast_n8', 'TreeMapleblasted05 N=8', '0.6754', '0.7483'),
    ('maple_n4', '12_orbit_maple_n4', 'TreeMapleForest2 N=4', '0.3545', '0.3674'),
    ('dead_n4', '13_orbit_dead_n4', 'BlastedForestDestroyedTreeUpright01 N=4', '0.5721', '0.6073'),
    ('rock_n4', '14_orbit_rock_n4', 'RockCliff02_Alt N=4', '0.7724', '0.8305'),
)

for tag, name, who, before, after in SUBJ:
    print(name)
    for el in (15, 45):
        strip([('exe 161568a5', tag + '_cards_r3_b1', 'card'),
               ('8-ring fill', tag + '_cards_b1', 'card'),
               ('the mesh', tag + '_cards_b1', 'mesh')],
              el, '%s/%s_el%02d.png' % (OUT, name, el),
              '%s, elevation %d, 12 azimuths -- card on the plane-outside fill '
              '(orbit IoU %s over 24 views) / on the 8-ring fill (%s) / the mesh itself'
              % (who, el, before, after))
    gif(tag + '_cards_b1', 'card', 15, '%s/%s_card.gif' % (OUT, name))
    gif(tag + '_cards_b1', 'mesh', 15, '%s/%s_mesh.gif' % (OUT, name))
    gif(tag + '_cards_r3_b1', 'card', 15, '%s/%s_card_before.gif' % (OUT, name))
