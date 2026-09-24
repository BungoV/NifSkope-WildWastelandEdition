"""Lane SHOWCASE1 picture 3 -- the far rings and the impostor cards.

Two separate things live in this picture and they are NOT the same object.

  * The far-ring .BTO is real geometry: the merged, decimated LOD meshes plus
    the stock engine's crossed quads for trees.  That is what the five render
    panels show.
  * An impostor CARD is an octahedral sprite sheet the bake also produced.  It
    is NOT inside the .BTO -- an ASCII scan of the file finds only the merged
    object atlas (`Commonwealth.LodgenObjects*.DDS`).  A card reaches a consumer
    through the manifest's `C` lines, which say "this placement stands on this
    card", and through the card's own sheets.  So the card panels are the
    sheets themselves, read off disk, not a screenshot of one.

Every render here comes from the LOOK bakes (`--no-terrain-identity
--no-identity`) so the pixels are the baked sheets and not the object-index
colours -- see red 1.  That costs this picture its card metadata, which is red 5:
the whole manifest block, `C` lines included, sits inside `if ( opts.identity )`
at src/lodgen.cpp:3760, so `--no-identity` silently takes the card arrays with
it ("no placement in the chunks stands on an octahedral card", lodgen.cpp:13046).
The card numbers below are therefore read from the identity-ON dim-16 ring.
"""
import os
import numpy as np
from PIL import Image
from common import L, dds_rgba, dds_kind, grid, save, read_cam, crop_content, clear_colour

S = L + '/shots'
CARDS = L + '/cards'
FAR16 = L + '/out/far16/obj'


def load(p):
    return np.asarray(Image.open(p).convert('RGB'))


def panel(name, head, sub):
    p = '%s/%s.png' % (S, name)
    if not os.path.exists(p):
        return (None, [head, 'missing render', ''])
    a = load(p).astype(np.int16)
    cov = 100.0 * (np.abs(a - clear_colour(a)).sum(2) > 10).mean()
    c = read_cam(p)
    return (crop_content(Image.fromarray(a.astype(np.uint8))),
            [head, sub,
             'covers %.1f%% of frame   upp %s   halfW %s'
             % (cov, c.get('upp', '?')[:7], c.get('halfW', '?')[:8])])


cells = [
    panel('p3_far16_top', 'dim 16 ring, chunk (-32,16), top down',
          '65,536 units square -- sixteen cells of the near ring'),
    panel('p3_far16_obl', 'the same chunk, oblique (VIEW=8)',
          '81 source shapes merged to 8; 2 shapes cut by the far-ring rule'),
    panel('p3_far16_near', 'dim 16, the Sanctuary corner, half-width 4,000',
          'merged LOD geometry, not crossed quads: 81 shapes became 8'),
    panel('p3_far32_top', 'dim 32 ring, chunk (-32,0), top down',
          '131,072 units square -- the outermost ring this lane baked'),
    panel('p3_far32_obl', 'the same chunk, oblique (VIEW=8)',
          '39 source shapes merged to 6'),
]

# ------------------------------------------------------- the card, on its own
alb = CARDS + '/0004a075_oct_albedo.png'
if os.path.exists(alb):
    im = Image.open(alb).convert('RGB')
    cells.append((im,
                  ['the card itself: 0004a075, TreeMapleForest3.nif',
                   'octahedral 8 x 8 = 64 frames of 64 x 256, sheet %d x %d'
                   % im.size,
                   'the most-used card in the dim-16 ring: 3,538 of 14,564 `C` lines']))
    # one frame out of the grid, at pixel scale, so a leaf is a leaf
    fw, fh = 64, 256
    crop = im.crop((fw * 4, fh * 4, fw * 5, fh * 5)).resize((fw * 4, fh * 4), Image.NEAREST)
    cells.append((crop,
                  ['one frame of that card, 4x nearest-neighbour',
                   'row 4 column 4 of the octahedron, 64 x 256 texels',
                   'half extents 238.87 x 955.48, depth span 3,072 units']))

sheet = L + '/out/far16/tex/Objects/Commonwealth.LodgenCards.legacy.512x2048_d.DDS'
if os.path.exists(sheet):
    a = dds_rgba(sheet)
    cells.append((a[:, :, :3],
                  ['the packed card sheet that card sits in',
                   'Commonwealth.LodgenCards.legacy.512x2048_d.DDS, %d x %d %s'
                   % (a.shape[1], a.shape[0], dds_kind(sheet)),
                   '%d bytes; _n, _g and _gsaos sit beside it' % os.path.getsize(sheet)]))

save(grid(cells, 3, cell=560, capt=58,
          title='3  The far rings (dim 16 and dim 32) and the impostor cards',
          sub='Renders from the LOOK bakes so the colour is the baked sheets. A card is NOT geometry inside the .BTO -- the .BTO names only '
              'the merged object atlas -- so the last three cells are the card\'s own sheets read off disk, and the card is named out of the '
              'identity-ON ring\'s manifest `C` lines. 23 of 23 candidates baked, all trees.'),
     '3_far_rings_and_cards.png')
