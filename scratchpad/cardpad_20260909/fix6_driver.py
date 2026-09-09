#!/usr/bin/env python3
"""CARDPAD -- tools/bake_impostor_cards.sh carries the spacing number in a
comment; it now names the GAP, not a per-side margin."""

P = 'tools/bake_impostor_cards.sh'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')

OLD = """# 46.4%. Since 2026-09-09 the padding is rounded UP TO EVEN precisely so a
# halving lands the aux gutter on a whole texel, and the aux mip count comes down
# with it: auxMips = 1 + log2(min(padX,padY)/auxDiv).
"""
NEW = """# 46.4%. Since 2026-09-09 the GAP between two neighbouring silhouettes -- bungo's
# own quantity, gap(side) = max(2, side/16) rounded UP TO EVEN, with the margin on
# each side of a frame half of it -- is rounded even precisely so a halving lands
# those margins on whole texels, and the aux mip count comes down with the gap:
# auxMips = 1 + log2(min(gapX,gapY)/auxDiv), floored at one level on the 16- and
# 32-texel frames where the gap is already at its floor of 2.
"""
assert s.count(OLD) == 1, 'driver comment: %d' % s.count(OLD)
s = s.replace(OLD, NEW)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('fix6 ok: %d -> %d bytes' % (len(b), len(nb)))
