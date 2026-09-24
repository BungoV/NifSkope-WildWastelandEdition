# Lane BTOFREE1, 2026-09-16 -- patch 3: native_open.sh check (c) gets an
# invariant that CAN be met and a floor that was measured, instead of an IoU
# bar nobody ever measured against the thing it compares.
#
# THE MEASUREMENT (this lane, 2026-09-16, chunk (-20,24) dim 4, one camera):
#   library mnam (the pre-2026-09-16 default)  IoU 0.8179   B-in-A 0.9874  A/B 1.195
#   library near (NATIVE1c default, today)     IoU 0.6190   B-in-A 0.9604  A/B 1.512
# The 0.8179 is to four decimals the number the check has printed on every exe
# since before NATIVEVIEW2, so the check never passed its own bar, not once.
#
# WHERE THE MISSING PIXELS ARE (scratchpad/btofree1_20260916/iou_analyse.py):
# under mnam the .lodi covers 17.4 percent more pixels than the .BTO; 100.0
# percent of that excess lies within 16 px of a pixel the two share, 58.7
# percent within 1 px, and the largest connected blob of it is 143 px with NOT
# ONE blob of 200 px or more. That is one silhouette drawn a hair wider than
# the other, everywhere, not an object drawn by one side and missed by the
# other -- and it is what an instance scene of whole models must look like
# beside a chunk mesh that has been merged, far-ring cut and clipped to the
# chunk. An equal-area bar (IoU) asks those two to have the SAME outline; they
# never will, and 0.95 was never a measurement of anything.
#
# So the check asks the two questions it actually wants answered:
#   COVER = |A and B| / |B|  -- does the .lodi draw EVERYTHING the .BTO draws?
#   FAT   = |A| / |B|        -- and does it do it without drawing the world?
# COVER alone is vacuous (a mask covering the screen scores 1.000), FAT alone
# is vacuous (an empty mask scores 0), and the pair is not. Both refuters are
# in the harness.
ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'


def patch(path, pairs):
    b = open(ROOT + path, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    s = b.decode('utf-8')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, 'anchor count %d (want 1) in %s for: %r' % (n, path, old[:80])
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0, 'CR count moved in %s: %d -> %d' % (path, cr0, nb.count(b'\r'))
    open(ROOT + path, 'wb').write(nb)
    print('%-40s %d -> %d bytes, CR %d -> %d, LF %d -> %d'
          % (path, n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))


AUTH = []

AUTH.append((
'''def job_mad(argv):''',
'''def job_cover(argv):
    """ONE-SIDED coverage of B by A, and how much wider A is (lane BTOFREE1).

    The .lodi scene and the .BTO of the same chunk are not the same geometry:
    the scene places whole library models, the chunk carries a mesh that the
    bake merged, cut at the far rings and clipped to the chunk box. Asking
    them for the same SILHOUETTE (IoU) asks for something neither the bake nor
    the viewer promises. What the pair does promise is that the scene draws
    every object the chunk draws, in the same place, at about the same size.

    COVER is the first half of that and FAT is the second; each is vacuous on
    its own and the two together are not. IOU is still printed because it is
    the number every earlier run of this gate printed, but nothing is gated on
    it any more."""
    if len(argv) < 2:
        die('cover <a.png> <b.png>')
    import numpy as np
    a = load_png(argv[0])
    b = load_png(argv[1])
    if a.shape != b.shape:
        die('sizes differ: %s against %s' % (a.shape, b.shape))
    ma, _ = mask_of(a)
    mb, _ = mask_of(b)
    na, nb = int(ma.sum()), int(mb.sum())
    inter = int((ma & mb).sum())
    union = int((ma | mb).sum())
    flip = int((ma & mb[::-1, :]).sum())
    print('  A covers %d px, B covers %d px, intersection %d, union %d'
          % (na, nb, inter, union))
    print('COVER %.4f' % ((inter / nb) if nb else 0.0))
    print('FAT %.4f' % ((na / nb) if nb else 0.0))
    print('COVERFLIP %.4f' % ((flip / nb) if nb else 0.0))
    print('IOU %.4f' % ((inter / union) if union else 0.0))
    return 0


def job_solid(argv):
    """Write a mask-shaped REFUTER: every pixel of the frame is foreground.

    It exists so the harness can show FAT failing on an input that scores a
    perfect COVER, which is the whole reason COVER is not gated alone."""
    if len(argv) < 2:
        die('solid <like.png> <out.png>')
    import numpy as np
    a = load_png(argv[0])
    h, w = a.shape[0], a.shape[1]
    img = np.full((h, w, 3), 200, dtype=np.uint8)
    img[0, 0] = img[0, w - 1] = img[h - 1, 0] = img[h - 1, w - 1] = (0, 0, 0)
    from PIL import Image
    Image.fromarray(img).save(argv[1])
    print('  wrote a solid %dx%d refuter frame' % (w, h))
    return 0


def job_mad(argv):''',
))

AUTH.append((
'''JOBS = {'chunk': job_chunk, 'compare': job_compare, 'iou': job_iou,
        'mad': job_mad, 'manifest': job_manifest, 'ncc': job_ncc}''',
'''JOBS = {'chunk': job_chunk, 'compare': job_compare, 'cover': job_cover,
        'iou': job_iou, 'mad': job_mad, 'manifest': job_manifest,
        'ncc': job_ncc, 'solid': job_solid}''',
))

AUTH.append((
'''        die('usage: %s {chunk|compare|iou|mad|manifest|ncc} ...' % os.path.basename(sys.argv[0]))''',
'''        die('usage: %s {chunk|compare|cover|iou|mad|manifest|ncc|solid} ...'
            % os.path.basename(sys.argv[0]))''',
))

patch('tests/spells/native_open_authority.py', AUTH)
print('patch3 ok')
