"""DOCFIX1: IMPOSTOR_SPEC frame law -- retire the five-rung aspect ladder and 46.4 %.
Source: src/nifskope_ui.cpp:23251 `for ( int s = 16; s <= tileLong; s += 16 ) {`;
CARD_SHEETS §3.2/§3.3/§3.5 (42.9 %, gate asserts exact bytes)."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/docs/LODGEN_IMPOSTOR_SPEC.md'
reps = [
(b"""**Aspect**, from the silhouette the bake has just measured over every view. Five
rungs for the short side, rounded to a multiple of four, never below the floor
that clears the gutter:

    1    3/4    1/2    3/8    1/4

The three-quarter rung is measured, not symmetric: TreeMapleForest2's silhouette
is 0.75 of its height at 8 x 8, and rounding that to a square frame costs 33%.

Both ladders are coarse ON PURPOSE. A card array holds only sets sharing a grid
AND a frame, so each additional frame shape is another array and another bind.
Nearest-in-log bounds the rounding to about 15%, and the fit GROWS whichever
extent is loose rather than cropping""",
b"""**Aspect**, from the silhouette the bake has just measured over every view: the
short side is the smallest **multiple of 16** texels whose INNER rect (the frame
less its margin on both sides) is not narrower than the silhouette's ratio
(`src/nifskope_ui.cpp`, `for ( int s = 16; s <= tileLong; s += 16 ) {`; the
derivation is `docs/LODGEN_CARD_SHEETS.md` \xc2\xa73.2). Until 2026-09-09 it was a
five-rung ratio ladder (1, 3/4, 1/2, 3/8, 1/4, rounded to a multiple of four);
that ladder is retired, because its floor forced a SQUARE frame on a
needle-shaped tree (TreeBlasted05 filled 4 texels of a 32-texel frame, 12.5%).

Both quantisations are coarse ON PURPOSE. A card array holds only sets sharing a
grid AND a frame, so each additional frame shape is another array and another
bind. The size ladder's nearest-in-log bounds its rounding to about 15%, and
both fits GROW whichever extent is loose rather than cropping""", 1),
(b"""set, the payload falls from 3,584 to 1,664 bytes: 46.4%.""",
b"""set (the card-array gate's, `gap 4`, a 16x32 sheet of 8x16 frames), the payload
falls from 4,480 to 1,920 bytes: 42.9% (`docs/LODGEN_CARD_SHEETS.md` \xc2\xa73.5). It
read 46.4% on 2026-09-06; the difference is the mip law, not the saving.""", 1),
(b"""The divide happens after frame dilation. The gutter is at least four texels, so a
halving mixes nothing across a frame border. Frames are multiples of four, so a
halved frame is still even.""",
b"""The divide happens after frame dilation. The gap is rounded up to an EVEN number
of texels so a halving lands its margins on whole texels, and frames are
multiples of 16, so a halved frame is still even and nothing mixes across a
frame border.""", 1),
]
b = open(P, 'rb').read(); cr0 = b.count(b'\r\n'); bad = False
for old, new, n in reps:
    c = b.count(old)
    if c != n: print('REFUSE', old[:60], c); bad = True; continue
    b = b.replace(old, new)
if bad or b.count(b'\r\n') != cr0: sys.exit(1)
if '--check' in sys.argv: print('check OK'); sys.exit(0)
open(P, 'wb').write(b); print('written', len(b), 'CRLF', b.count(b'\r\n'))
