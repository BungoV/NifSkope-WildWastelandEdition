"""Lane SHOWCASE1 picture 1 -- the 2D sheets, ours ON vs ours OFF vs vanilla."""
import os

import numpy as np

from common import L, VAN, dds_rgba, dds_kind, grid, save, mean_rgb, to_img

ON = L + '/out/on/tex'
OFF = L + '/out/off/tex'
CH = ['-20.24', '-20.28']          # the near chunk and one neighbour


def sheet(root, chunk, suffix=''):
    p = '%s/Commonwealth.4.%s%s.DDS' % (root, chunk, suffix)
    return p if os.path.exists(p) else None


def cell(p, head):
    if not p:
        return (None, [head, 'no such file', ''])
    a = dds_rgba(p)
    r, g, b = mean_rgb(a)
    return (to_img(a[:, :, :3]),
            [head,
             '%d x %d  %s' % (a.shape[1], a.shape[0], dds_kind(p)),
             'mean RGB %.1f / %.1f / %.1f   %d B' % (r, g, b, os.path.getsize(p))])


# ---------------------------------------------------------------- 1a  colour
cells = []
for c in CH:
    cells.append(cell(sheet(ON, c), 'chunk (%s)  OURS, every feature ON' % c))
    cells.append(cell(sheet(OFF, c), 'chunk (%s)  OURS, every switch OFF' % c))
    cells.append(cell(sheet(VAN, c), 'chunk (%s)  VANILLA, as shipped' % c))
save(grid(cells, 3, cell=512,
          title='1a  Far-terrain COLOUR sheet, dim 4, Sanctuary',
          sub='ON = road-detail 1 + land-guide aspecthex/hex 256 + object AO + erosion 1 + msn cache.  '
              'OFF = every one of those off.  Vanilla = Bethesda\'s shipped sheet, same chunk.'),
     '1a_sheets_colour.png')

# ---------------------------------------------------------------- 1b  normal
cells = []
for c in CH:
    cells.append(cell(sheet(ON, c, '_msn'), 'chunk (%s)  OURS ON  (--msn-cache)' % c))
    cells.append(cell(sheet(OFF, c, '_msn'), 'chunk (%s)  OURS OFF' % c))
    cells.append(cell(sheet(VAN, c, '_msn'), 'chunk (%s)  VANILLA _msn' % c))
save(grid(cells, 3, cell=512,
          title='1b  Far-terrain NORMAL sheet (_msn), dim 4, Sanctuary',
          sub='The ON column is bungo\'s cleaned 2K cache at E:/Tools/Upscale/esrgan-bat/output, '
              'shown downsampled to 512 for the page; the byte sizes are the files\' own.'),
     '1b_sheets_msn.png')

# ------------------------------------------------------- 1c  mask, 4 channels
# Channel roles measured, not assumed: docs/LODGEN_TERRAIN_VT.md line 51 --
# "R AO, G wetness, B shore, A cover".
ROLES = [('R  ambient occlusion', 0), ('G  wetness', 1),
         ('B  shore proximity', 2), ('A  ground cover', 3)]
cells = []
for tag, root in (('ON', ON), ('OFF', OFF)):
    p = sheet(root, '-20.24', '_data')
    if not p:
        cells += [(None, ['mask %s' % tag, 'no file', ''])] * 5
        continue
    a = dds_rgba(p)
    cells.append((to_img(a[:, :, :3]),
                  ['mask sheet, %s  (RGB shown)' % tag,
                   '%dx%d  %s  %d B' % (a.shape[1], a.shape[0], dds_kind(p), os.path.getsize(p)),
                   'chunk (-20,24)']))
    for name, ci in ROLES:
        ch = a[:, :, ci]
        cells.append((to_img(ch),
                      ['%s  %s' % (name, tag),
                       'mean %.1f  min %d  max %d' % (ch.mean(), ch.min(), ch.max()),
                       '']))
save(grid(cells, 5, cell=400,
          title='1c  The MASK sheet (_data) and each of its four channels, chunk (-20,24)',
          sub='Roles from docs/LODGEN_TERRAIN_VT.md line 51: R ambient occlusion, G wetness, B shore, A ground cover. '
              'BC3 because the cover bit is on. Vanilla ships NO _data sheet at all (same doc, section 7a.1), so there is no third column.'),
     '1c_sheets_mask.png')
