"""Lane SHOWCASE1 pictures 2 and 4 -- terrain and objects, three views.

The render hook opens ONE file, and a .BTR and a .BTO are two files, so these
are COMPOSITES: the object render is keyed over the terrain render.  The two
renders are from the same pinned camera -- same WW_RENDER_VIEW, same
WW_RENDER_ORTHO, same viewport -- and the only thing that differs is the
look-at, by exactly the chunk origin (a .BTR is chunk-local, a .BTO worldspace).
Each cell's caption carries the `upp` and look-at read back out of
$WW_CAMERA_CENSUS at the moment of the grab, so the claim is checkable.

The key is the viewport's own clear colour.  That is honest for the top view,
where nothing in the chunk can be in front of an object.  For the front and the
oblique view it draws every object OVER the terrain whatever the depth says, so
a tree behind a ridge is still drawn: see the caption.
"""
import os
import numpy as np
from PIL import Image
from common import L, grid, save, font, read_cam, crop_square, crop_content, clear_colour
from PIL import ImageDraw

S = L + '/shots'


def cam(p):
    """(upp, lookat, view, halfW, vp) read back out of the .cam sidecar."""
    try:
        t = open(p + '.cam').read().strip().split('\n')[-1]
    except OSError:
        return {}
    d = {}
    for kv in t.split():
        if '=' in kv:
            k, v = kv.split('=', 1)
            d[k] = v
    return d


def load(p):
    return np.asarray(Image.open(p).convert('RGB')).astype(np.int16)


def over(btr, bto, thr=10):
    """Object render keyed over the terrain render; returns (image, cover%)."""
    a, b = load(btr), load(bto)
    bg = clear_colour(b)
    m = (np.abs(b - bg).sum(2) > thr)
    out = a.copy()
    out[m] = b[m]
    return Image.fromarray(out.astype(np.uint8)), 100.0 * m.mean()


VIEWS = [('top', 'top down (WW_RENDER_VIEW=1)'),
         ('front', 'front (VIEW=5)'),
         ('obl', 'oblique (VIEW=8, ViewUser)')]
BAKES = [('on', 'identity ON -- the default'),
         ('noid', '--no-terrain-identity'),
         ('look', '--no-terrain-identity --no-identity')]

cells = []
for bk, blab in BAKES:
    for vw, vlab in VIEWS:
        p1 = '%s/p2_%s_btr_%s.png' % (S, bk, vw)
        p2 = '%s/p2_%s_bto_%s.png' % (S, bk, vw)
        if not (os.path.exists(p1) and os.path.exists(p2)):
            cells.append((None, ['%s  %s' % (vlab, blab), 'missing render', '']))
            continue
        im, cov = over(p1, p2)
        c1, c2 = cam(p1), cam(p2)
        im = crop_square(im, c1) if vw == 'top' else crop_content(im)
        cells.append((im, [
            '%s   %s' % (vlab, blab),
            'objects %.1f%% of frame   upp %s = %s   halfW %s'
            % (cov, c1.get('upp', '?')[:7], c2.get('upp', '?')[:7], c1.get('halfW', '?')[:7]),
            'look-at %s | %s' % (c1.get('lookat', '?').replace('.0000', ''),
                                 c2.get('lookat', '?').replace('.0000', ''))]))

save(grid(cells, 3, cell=560, capt=58,
          title='2  The near chunk (-20,24): TERRAIN + OBJECTS, three views, three identity states',
          sub='COMPOSITE, not one framebuffer: the .BTO render is keyed over the .BTR render off the viewport clear colour. '
              'Same pinned camera for both; the look-at differs by exactly the chunk origin (-81920, 98304) because a .BTR is '
              'chunk-local and a .BTO worldspace. Top down the key is exact; front and oblique draw every object over the '
              'terrain whatever the depth, so a tree behind a ridge is still drawn.'),
     '2_terrain_and_objects.png')

# ----------------------------------------------------------- 4  the .BTO alone
cells = []
for vw, vlab in VIEWS:
    p = '%s/p2_look_bto_%s.png' % (S, vw)
    if not os.path.exists(p):
        cells.append((None, ['objects only, %s' % vlab, 'missing', '']))
        continue
    a = load(p)
    cov = 100.0 * (np.abs(a - clear_colour(a)).sum(2) > 10).mean()
    c = cam(p)
    _im = Image.fromarray(a.astype(np.uint8))
    _im = crop_square(_im, c) if vw == 'top' else crop_content(_im)
    cells.append((_im,
                  ['objects only, %s' % vlab,
                   '678 objects: 654 trees, 13 rocks, 11 buildings',
                   'covers %.1f%% of frame   upp %s   halfW %s'
                   % (cov, c.get('upp', '?')[:7], c.get('halfW', '?')[:7])]))
p = '%s/p2_vanilla_bto_top.png' % S
if os.path.exists(p):
    a = load(p)
    cov = 100.0 * (np.abs(a - clear_colour(a)).sum(2) > 10).mean()
    cells.append((crop_square(Image.fromarray(a.astype(np.uint8)), cam(p)),
                  ['VANILLA .BTO, same chunk, same camera',
                   "Bethesda's shipped file, for scale and for red 1",
                   'covers %.1f%% of frame' % cov]))

# --- the 11 buildings, close up (ORTHO half-width 520 units on the cluster) ---
# The cluster's own manifest rows put it at world x -77836..-77310,
# y 102213..102378, z 8103..8231 -- 530 x 165 x 130 units.  Terrain and objects
# are composited the same way as picture 2.
for vw, vlab in (('top', 'top down'), ('obl', 'oblique (VIEW=8)')):
    pr = '%s/p4_bld_btr_%s.png' % (S, vw)
    po = '%s/p4_bld_bto_%s.png' % (S, vw)
    if not (os.path.exists(pr) and os.path.exists(po)):
        cells.append((None, ['buildings close up, %s' % vlab, 'missing render', '']))
        continue
    im, cov = over(pr, po)
    c1, c2 = cam(pr), cam(po)
    im = crop_content(im)
    cells.append((im, [
        'the 11 BUILDINGS close up, %s' % vlab,
        'ortho half-width %s units   upp %s   objects %.1f%% of frame'
        % (c1.get('halfW', '?').split('.')[0], c1.get('upp', '?')[:7], cov),
        'look-at %s | %s' % (c1.get('lookat', '?').replace('.0000', ''),
                             c2.get('lookat', '?').replace('.0000', ''))]))

save(grid(cells, 3, cell=560, capt=58,
          title='4  The near chunk\'s object file alone (Commonwealth.4.-20.24.BTO, 1,276,441 bytes)',
          sub='From the LOOK bake (both identity payloads off) -- see red 1. Lit, textured, no grid, no axes (WW_RENDER_CLEAN=1). '
              'The fourth cell is vanilla\'s own .BTO for the same chunk on the same camera.'),
     '4_objects_only.png')
