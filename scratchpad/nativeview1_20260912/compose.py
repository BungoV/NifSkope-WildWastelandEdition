"""Lane NATIVEVIEW1 -- lay the native pictures out with their legacy controls.

Every frame is a render this lane took; nothing here re-colours or re-scales a
picture.  The only assembly is the LEGACY terrain+objects column, which has to
be composited because the bake keeps the two halves in two files: the .BTR and
the .BTO are photographed with the same orthographic camera and the same window,
so pasting the .BTO's non-background pixels over the .BTR frame puts every
object exactly where the camera already had it.  The label says so.
"""
import os
from PIL import Image, ImageDraw, ImageFont

R = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912'
W = R + '/work/shots'
OUT = R + '/images'
os.makedirs(OUT, exist_ok=True)

PAD = 10
BAR = 30
BG = (24, 24, 26)
FG = (232, 232, 232)


def font(size):
    for p in ('C:/Windows/Fonts/segoeui.ttf', 'C:/Windows/Fonts/arial.ttf'):
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


F = font(19)
FS = font(15)


def load(name):
    return Image.open('%s/%s.png' % (W, name)).convert('RGB')


def corner_bg(im):
    w, h = im.size
    c = [im.getpixel(p) for p in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1))]
    return max(set(c), key=c.count)


def composite(base, over, tol=12):
    """Paste `over`'s non-background pixels on to `base`. Same camera, same size."""
    import numpy as np
    a = np.asarray(base, dtype=int)
    b = np.asarray(over, dtype=int)
    bg = np.array(corner_bg(over), dtype=int)
    m = (np.abs(b - bg).max(axis=2) > tol)
    out = a.copy()
    out[m] = b[m]
    return Image.fromarray(out.astype('uint8')), int(m.sum())


def panel(cells, cols, title, outname):
    """cells: list of (image, caption). Laid out row-major in `cols` columns."""
    cw = max(im.size[0] for im, _ in cells)
    ch = max(im.size[1] for im, _ in cells)
    rows = (len(cells) + cols - 1) // cols
    # the caption bar has to hold the LONGEST caption in the panel, or the last
    # line of a three-line caption is simply cut off (measured: it was)
    lines = max(len(cap.split(chr(10))) for _, cap in cells)
    bar = 8 + 17 * lines
    Wd = PAD + cols * (cw + PAD)
    Ht = BAR + PAD + rows * (ch + bar + PAD)
    out = Image.new('RGB', (Wd, Ht), BG)
    d = ImageDraw.Draw(out)
    d.text((PAD, 6), title, font=F, fill=FG)
    for i, (im, cap) in enumerate(cells):
        r, c = divmod(i, cols)
        x = PAD + c * (cw + PAD)
        y = BAR + PAD + r * (ch + bar + PAD)
        out.paste(im, (x, y))
        for j, line in enumerate(cap.split('\n')):
            d.text((x + 2, y + ch + 4 + j * 17), line, font=FS, fill=FG)
    out.save('%s/%s' % (OUT, outname))
    return outname


made = []

# ---------------------------------------------------------------- (i) both
btr_t, bto_t = load('legacy_btr_top'), load('legacy_bto_top')
btr_o, bto_o = load('legacy_btr_obl'), load('legacy_bto_obl')
leg_t, nt = composite(btr_t, bto_t)
leg_o, no = composite(btr_o, bto_o)
leg_t.save(W + '/legacy_both_top.png')
leg_o.save(W + '/legacy_both_obl.png')
made.append(panel([
    (load('i_native_both_top'),
     'NATIVE  top  Commonwealth.lodl lit by Commonwealth.VT.2.lodt, with\n'
     'Commonwealth.lodi appended: 4 terrain shapes, 1,156 vertices +\n'
     '676 placements in 50 shapes, 36,866 vertices'),
    (leg_t,
     'LEGACY  top  Commonwealth.4.-20.24.BTR with .BTO composited over it\n'
     '(same orthographic camera, %d object pixels pasted); the bake keeps\n'
     'the two halves in two files and two spaces' % nt),
    (load('i_native_both_obl'),
     'NATIVE  oblique  the same document, look-at at z 8500'),
    (leg_o,
     'LEGACY  oblique  the same pair composited, %d object pixels' % no),
], 2, 'lane NATIVEVIEW1 (i)  terrain and objects together, cells [-20,24]..[-17,27], '
   'lod 2, ortho half-width 8192, 1024x1024 asked', 'i_terrain_and_objects.png'))

# --------------------------------------------------------------- (ii) objects
made.append(panel([
    (load('ii_native_obj_top'),
     'NATIVE  top  Commonwealth.lodi, cells [-20,24]..[-17,27], cluster level 0:\n'
     '676 placements read, 676 drawn, 33 bases, 50 shapes, 36,866 vertices'),
    (load('legacy_bto_top'),
     'LEGACY  top  Commonwealth.4.-20.24.BTO, the same 676 placements baked\n'
     'into one merged shape'),
    (load('ii_native_obj_obl'),
     'NATIVE  oblique  the same 676 placements'),
    (load('legacy_bto_obl'),
     'LEGACY  oblique  the same .BTO'),
], 2, 'lane NATIVEVIEW1 (ii)  objects alone, 676 placements in cells '
   '[-20,24]..[-17,27]', 'ii_objects_only.png'))

# ------------------------------------------------------------------ (iii) far
made.append(panel([
    (load('iii_native_far_coarse'),
     'NATIVE  the whole file, cells [-20,20]..[-9,35], CLUSTER LEVEL 7 (the\n'
     'coarsest this library carries): 3,526 placements drawn, 52 bases,\n'
     '54 shapes, 22,358 vertices'),
    (load('iii_native_far_fine'),
     'NATIVE  the same region at CLUSTER LEVEL 0 (the finest): the same 3,526\n'
     'placements, 79 shapes, 178,009 vertices -- eight times the geometry\n'
     'for the same scene'),
], 2, 'lane NATIVEVIEW1 (iii)  the far region at the coarsest .lodi level.  '
   'THIS BAKE CARRIES NO IMPOSTOR CARDS (bake_look.sh passes no --impostors), '
   'so none can be shown; no legacy control, the 12 chunks are 12 separate .BTO files',
   'iii_far_region_levels.png'))

print('written to %s' % OUT)
for n in made:
    p = '%s/%s' % (OUT, n)
    im = Image.open(p)
    print('  %-32s %4dx%-4d  %9d B' % (n, im.size[0], im.size[1], os.path.getsize(p)))
