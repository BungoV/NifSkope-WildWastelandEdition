"""Lane SHOWCASE1 picture 5 -- the baked ambient occlusion, in greyscale.

Three different AO numbers live in this bake and they are NOT the same thing:

  * the TERRAIN AO at TEXEL resolution -- the R channel of the chunk's
    `_data.DDS` mask sheet (docs/LODGEN_TERRAIN_VT.md line 51: "R AO, G wetness,
    B shore, A cover").  This is what `--terrain-object-ao` writes.
  * the TERRAIN AO at VERTEX resolution -- the .BTR's vertex colour B
    (src/lodgen.cpp:945-957 writes R material class, G wetness, B AO, A shore).
  * the OBJECT AO -- the .BTO's vertex colour B, one value a vertex, ray-cast
    per placement (src/lodgen.h:604 `bakeAO`).

The two vertex ones are drawn through the viewport's own channel preview,
WW_LOD_CHANNEL=3, which is `v = C.bbb` in res/shaders/fo4_default.frag:279 --
the B channel flat, no texture and no lighting.  So the grey in those panels is
the byte in the file, not a shade of a render.
"""
import os
import numpy as np
from PIL import Image
from common import L, dds_rgba, dds_kind, grid, save, read_cam, crop_square, crop_content, clear_colour

S = L + '/shots'


def g(path):
    a = np.asarray(Image.open(path).convert('RGB')).astype(np.int16)
    return a


def stat(x):
    return 'mean %.1f  min %d  max %d' % (x.mean(), x.min(), x.max())


cells = []

# (a) terrain AO, texel resolution, straight out of the mask sheet
p = L + '/out/on/tex/Commonwealth.4.-20.24_data.DDS'
a = dds_rgba(p)
r = a[:, :, 0]
cells.append((np.dstack([r] * 3),
              ['a  TERRAIN AO, texel resolution', '_data.DDS R channel, %dx%d %s'
               % (a.shape[1], a.shape[0], dds_kind(p)), stat(r)]))

# (b) the same AO at vertex resolution, through the viewport
p = S + '/p5_btr_ao_top.png'
if os.path.exists(p):
    x = g(p)
    bg = clear_colour(x)
    m = np.abs(x - bg).sum(2) > 10
    cells.append((crop_square(Image.fromarray(x.astype(np.uint8)), read_cam(p)),
                  ['b  TERRAIN AO, vertex resolution', '.BTR vertex colour B, WW_LOD_CHANNEL=3, top down',
                   stat(x[m][:, 0]) if m.any() else 'empty']))

# (c) the object AO
p = S + '/p5_bto_ao_top.png'
if os.path.exists(p):
    y = g(p)
    bgy = clear_colour(y)
    my = np.abs(y - bgy).sum(2) > 10
    cells.append((crop_square(Image.fromarray(y.astype(np.uint8)), read_cam(p)),
                  ['c  OBJECT AO, per placement', '.BTO vertex colour B, WW_LOD_CHANNEL=3, top down',
                   (stat(y[my][:, 0]) + '  over %.1f%% of frame' % (100 * my.mean())) if my.any() else 'empty']))

# (d) the two together, top down -- the picture bungo asked for
if os.path.exists(S + '/p5_btr_ao_top.png') and os.path.exists(S + '/p5_bto_ao_top.png'):
    out = x.copy()
    out[my] = y[my]
    cells.append((crop_square(Image.fromarray(out.astype(np.uint8)),
                              read_cam(S + '/p5_btr_ao_top.png')),
                  ['d  BOTH, top down', 'objects keyed over terrain, both greyscale AO',
                   'objects %.1f%% of frame, terrain the rest' % (100 * my.mean())]))

# (e) the two together, oblique
pa, pb = S + '/p5_btr_ao_obl.png', S + '/p5_bto_ao_obl.png'
if os.path.exists(pa) and os.path.exists(pb):
    xa, yb = g(pa), g(pb)
    mb = np.abs(yb - clear_colour(yb)).sum(2) > 10
    o2 = xa.copy()
    o2[mb] = yb[mb]
    cells.append((crop_content(Image.fromarray(o2.astype(np.uint8))),
                  ['e  BOTH, oblique (VIEW=8)', 'same key, same AO channel',
                   'objects %.1f%% of frame' % (100 * mb.mean())]))

# (f) the AO switched OFF, same sheet, for the difference
p = L + '/out/one_noao/tex/Commonwealth.4.-20.24_data.DDS'
if os.path.exists(p):
    b = dds_rgba(p)[:, :, 0]
    d = np.abs(b.astype(np.int16) - r.astype(np.int16))
    cells.append((np.dstack([b] * 3),
                  ['f  the same sheet with --terrain-object-ao OFF', stat(b),
                   'differs from (a) on %.1f%% of texels, worst %d, mean |diff| %.2f'
                   % (100 * (d > 0).mean(), d.max(), d.mean())]))


# (g) and (h) the buildings' own AO, close up -- the panel bungo named: the
# baked AO of the buildings and the terrain together, greyscale, at a size where
# a wall is more than a pixel.  Top down first, because a LOD building is a flat
# slab and an oblique view of a slab is a line.
for tag, sfx, lab in (('g', '_top', 'top down'), ('h', '', 'oblique (VIEW=8)')):
    pa, pb = '%s/p5_bld_btr_ao%s.png' % (S, sfx), '%s/p5_bld_bto_ao%s.png' % (S, sfx)
    if not (os.path.exists(pa) and os.path.exists(pb)):
        continue
    xg, yg = g(pa), g(pb)
    mg = np.abs(yg - clear_colour(yg)).sum(2) > 10
    og = xg.copy()
    og[mg] = yg[mg]
    c = read_cam(pb)
    im = crop_square(Image.fromarray(og.astype(np.uint8)), read_cam(pa), half=520.0)         if sfx == '_top' else crop_content(Image.fromarray(og.astype(np.uint8)))
    cells.append((im,
                  ['%s  the 11 BUILDINGS + terrain AO, %s' % (tag, lab),
                   'ortho half-width %s units, same WW_LOD_CHANNEL=3'
                   % c.get('halfW', '?').split('.')[0],
                   (stat(yg[mg][:, 0]) + '  objects %.1f%% of frame' % (100 * mg.mean()))
                   if mg.any() else 'empty']))

save(grid(cells, 3, cell=520, capt=58,
          title='5  The baked ambient occlusion, greyscale: buildings, trees, rocks and the terrain together',
          sub='Three different AO numbers. (a) and (f) are the mask sheet\'s R channel read straight off disk. (b), (c), (d), (e) are the '
              'viewport drawing vertex colour B flat -- WW_LOD_CHANNEL=3 is `v = C.bbb` in fo4_default.frag:279, no texture and no lighting, '
              'so the grey IS the byte in the file. (d) and (e) are composites: two files, one camera, objects keyed over terrain.'),
     '5_ao_greyscale.png')
