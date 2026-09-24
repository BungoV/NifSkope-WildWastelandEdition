#!/usr/bin/env python3
"""IDENTPROX -- the two highway cameras, and the seam planes they look at.

Heights are read off the terrain, never typed -- the same rule SUNSIM1's
`cams.py` states.
"""
import numpy as np
import sys

SUNSIM = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/sunsim1_20260919'
if SUNSIM not in sys.path:
    sys.path.insert(0, SUNSIM)
from render import Camera                                   # noqa: E402

# The deck run measured by hwy_why.py: it lies along +Y at x about 20,357,
# deck top z about 2,486, boxes spanning x 18,949..21,765.
DECK_X = 20357.0
DECK_TOP = 2486.0

# Seam planes: the Y at which two highway identities' world AABBs abut.  Taken
# from hwy_why.py's consecutive-pair table, not typed from a picture.
SEAMS = [(-47397.0, 'g100480 EndCapL01 | g100481 EndCapL03'),
         (-44561.0, 'g100481 EndCapL03 | g100263 Str01Damaged01  (GREEN | YELLOW-GREEN)'),
         (-43261.0, 'g100263 Str01Damaged01 | g100293 ChunkLargeTop03'),
         (-42513.0, 'g100263 Str01Damaged01 | g100092 StrExit01  (YELLOW-GREEN | PINK)'),
         (-38417.0, 'g100092 StrExit01 | g100091 Str01Damaged01  (PINK | GREEN-TEAL)'),
         (-36369.0, 'g100091 Str01Damaged01 | g100007 Str01  (GREEN-TEAL | BLUE)'),
         (-34321.0, 'g100007 Str01 | g100006 EndCapR01  (BLUE | TEAL)')]


def build(ter, w=1600, h=900):
    def g(x, y):
        return float(ter.atf(np.array([x]), np.array([y]))[0])

    cams = {}
    # (1) "hwydeck": beside and above the deck at its south end, looking NORTH
    #     along the run, so every seam in the run is in one frame and the sun
    #     at azimuth 180 grazes the deck surface along its length.
    e = (23400.0, -47800.0)
    t = (20357.0, -36800.0)
    cams['hwydeck'] = Camera((e[0], e[1], g(*e) + 4200.0), (t[0], t[1], DECK_TOP - 250.0),
                             52.0, w, h, name='hwydeck')
    # (2) "hwyunder": standing on the ground WEST of the run, looking EAST at
    #     the underside and the support columns.  The eye is one head above the
    #     terrain it stands on and far enough back that the ground between the
    #     lens and the columns -- where the deck's own shadow lands at a
    #     western sun -- is in frame.
    e = (17000.0, -39300.0)
    t = (20357.0, -39000.0)
    cams['hwyunder'] = Camera((e[0], e[1], g(*e) + 180.0), (t[0], t[1], 1750.0),
                              55.0, w, h, name='hwyunder')
    return cams


SUNS = [(120.0, 5.0), (120.0, 15.0), (240.0, 15.0),
        (180.0, 10.0),      # ALONG the run (it lies north-south)
        (90.0, 10.0)]       # ACROSS it
SUN_NOTE = {(120.0, 5.0): 'the lane default, very low',
            (120.0, 15.0): 'the lane default',
            (240.0, 15.0): 'the lane default, western',
            (180.0, 10.0): 'ALONG the run (the deck lies north-south)',
            (90.0, 10.0): 'ACROSS the run'}
