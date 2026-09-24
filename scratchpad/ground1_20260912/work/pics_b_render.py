"""Lane GROUND1 Part B, the viewport picture.

The SAME terrain LOD mesh -- byte-identical in the two bakes, because the pass
writes sheets and never geometry -- drawn by NifSkope's own renderer with the
OFF sheets and then the ON sheets, from one pinned orthographic camera
(WW_RENDER_CENTER / _ORTHO / _DIST / _VIEW, see b_render2.sh).

WHAT THIS PICTURE IS AND IS NOT. It is evidence that the change survives the
whole path -- lattice, delta, DDS codec, resource stack, GL -- and reaches a
real framebuffer. It is NOT what the ground looks like in the game: NifSkope
draws FO4 terrain LOD in colours that are not the ground colours (greens and
magentas over a diffuse sheet whose mean is a dark brown, 68/61/53 of 255).
That is a pre-existing renderer matter, not this pass, and it was measured, not
chased. The picture that answers "what does it look like" is
b_cmp_erosion_lit.png, whose lighting is stated arithmetic.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), 'images')
AMP = 16


def main():
    o = np.asarray(Image.open(os.path.join(HERE, 'b_ortho_off.png')).convert('RGB'))
    n = np.asarray(Image.open(os.path.join(HERE, 'b_ortho_on.png')).convert('RGB'))
    d = np.abs(n.astype(np.int32) - o.astype(np.int32))
    print('|on - off| mean %.3f of 255, p99 %.1f, max %d'
          % (d.mean(), np.percentile(d, 99), d.max()))
    amp = np.clip(d * AMP, 0, 255).astype(np.uint8)

    h, w = o.shape[:2]
    pad, gap = 64, 18
    out = Image.new('RGB', (gap + 3 * (w + gap), pad + h + gap), (16, 16, 16))
    for k, p in enumerate((o, n, amp)):
        out.paste(Image.fromarray(p), (gap + k * (w + gap), pad))
    dr = ImageDraw.Draw(out)
    dr.text((6, 5), 'NifSkope own viewport, the same byte-identical terrain LOD mesh '
                    '(Commonwealth dim 4 chunk -32,-20, --road-detail 1), one pinned '
                    'orthographic camera, only the sheets change', fill=(235, 235, 235))
    dr.text((6, 20), 'the palette is the renderer, not the ground: it draws FO4 terrain '
                     'LOD in greens and magentas over a diffuse sheet whose mean is '
                     '68/61/53 of 255. Measured, not chased -- see B5.',
            fill=(170, 170, 170))
    dr.text((6, 32), 'what it does prove: the mesh bytes are identical in the two runs, '
                     'so every pixel that differs came out of the two sheets.',
            fill=(170, 170, 170))
    for k, t in enumerate(('the OFF sheets  (--erosion 0)',
                           'the ON sheets   (--erosion 1 --erosion-iterations 4 --erosion-seed 7)',
                           'the difference, x%d  (mean %.2f of 255, max %d)'
                           % (AMP, d.mean(), d.max()))):
        dr.text((gap + k * (w + gap) + 2, pad - 14), t, fill=(235, 235, 235))
    p = os.path.join(OUT, 'b_render_viewport.png')
    out.save(p)
    print('%s  %dx%d  %d bytes' % (p, out.width, out.height, os.path.getsize(p)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
