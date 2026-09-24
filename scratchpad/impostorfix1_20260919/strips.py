# The pictures. Every one is made from grabs the harness wrote itself, never a
# screen capture, and every row is labelled in the image so a strip cannot be
# read as the wrong thing later.
import os, glob
from PIL import Image, ImageDraw

R = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix1_20260919'
C = R + '/control'
OUT = R + '/images'
os.makedirs(OUT, exist_ok=True)
AZ = list(range(0, 360, 30))
BG = (24, 25, 28)
FG = (208, 208, 200)


def grab(tag, az, el, kind):
    p = '%s/%s/v_az%03d_el%02d_%s.png' % (C, tag, az, el, kind)
    return Image.open(p).convert('RGB') if os.path.exists(p) else None


def strip(rows, az, el, path, title, scale=0.42):
    """rows = [(label, tag, kind)]"""
    probe = grab(rows[0][1], az[0], el, rows[0][2])
    w, h = probe.size
    tw, th = int(w * scale), int(h * scale)
    lab = 84
    hdr = 22
    img = Image.new('RGB', (lab + tw * len(az), hdr + th * len(rows)), BG)
    d = ImageDraw.Draw(img)
    d.text((6, 6), title, fill=FG)
    for k, a in enumerate(az):
        d.text((lab + k * tw + 4, 6), 'az %d' % a, fill=FG)
    for r, (label, tag, kind) in enumerate(rows):
        d.text((6, hdr + r * th + th // 2 - 4), label, fill=FG)
        for k, a in enumerate(az):
            g = grab(tag, a, el, kind)
            if g is not None:
                img.paste(g.resize((tw, th)), (lab + k * tw, hdr + r * th))
    img.save(path)
    print('  wrote', os.path.basename(path), img.size)


def gif(tag, kind, el, path, scale=0.5):
    frames = []
    for a in AZ:
        g = grab(tag, a, el, kind)
        if g is None:
            continue
        w, h = g.size
        frames.append(g.resize((int(w * scale), int(h * scale))))
    if frames:
        frames[0].save(path, save_all=True, append_images=frames[1:], duration=200, loop=0)
        print('  wrote', os.path.basename(path), len(frames), 'frames')


print('00_before_after')
strip([('OLD card', 'orbB1', 'card'),
       ('NEW card', 'blast_n4_after_b1', 'card'),
       ('mesh', 'orbB1', 'mesh')],
      AZ, 15, OUT + '/00_before_after.png',
      'blast_n4, elev 15, 12 azimuths -- exe 88d6abb3 card / repaired card / the mesh itself')

for tag, name in (('blast_n4', '10_orbit_blast_n4'), ('blast_n8', '11_orbit_blast_n8'),
                  ('maple_n4', '12_orbit_maple_n4'), ('dead_n4', '13_orbit_dead_n4'),
                  ('rock_n4', '14_orbit_rock_n4')):
    print(name)
    for el in (15, 45):
        strip([('card BEFORE', tag + '_before_b1', 'card'),
               ('card AFTER', tag + '_after_b1', 'card'),
               ('mesh', tag + '_after_b1', 'mesh')],
              AZ, el, '%s/%s_el%02d.png' % (OUT, name, el),
              '%s, elev %d -- card on the shipped sheets / on the repaired sheets / the mesh' % (tag, el))
    gif(tag + '_after_b1', 'card', 15, '%s/%s_card.gif' % (OUT, name))
    gif(tag + '_after_b1', 'mesh', 15, '%s/%s_mesh.gif' % (OUT, name))

print('20_parallax_is_the_flake_maker')
strip([('parallax ON  (old sheet)', 'orbB1', 'card'),
       ('parallax OFF (old sheet)', 'orbB0', 'card'),
       ('parallax ON  (repaired)', 'blast_n4_after_b1', 'card'),
       ('mesh', 'orbB1', 'mesh')],
      AZ, 15, OUT + '/20_parallax_is_the_flake_maker.png',
      'blast_n4 elev 15: the flakes are the parallax step reading a broken height channel')
