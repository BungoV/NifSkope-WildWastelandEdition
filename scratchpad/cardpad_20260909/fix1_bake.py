#!/usr/bin/env python3
"""CARDPAD 1/2 -- the bake (src/nifskope_ui.cpp): the number bungo gives is the
GAP between two neighbouring silhouettes, so the margin on each side is half of it."""
import io, sys

P = 'src/nifskope_ui.cpp'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')

OLD_LAW = """						/* THE PADDING, PER AXIS, AND THE MIPS IT BUYS (bungo, 2026-09-09).
						 * His words: "maximizing the size of the geometry on each render, so
						 * that there is still a little bit of padding, 8 pixels on 1024x1024,
						 * 16 on 2k, and so on", and "enough pixel padding so that there's no
						 * mip map bleeding into other rows and columns".
						 *
						 * A reader sampling inside a frame's UV rect reaches half a texel past
						 * the rect AT the rect's own border, and that tap lands in the
						 * NEIGHBOURING frame. Mip CONSTRUCTION never mixes frames (a frame side
						 * stays even all the way down), so the bleed is at SAMPLE time and the
						 * condition is one texel of gutter at the level being sampled:
						 * P / 2^k >= 1. P texels therefore carry k = log2(P) clean levels below
						 * the top, and a sheet may ship M = 1 + log2(P) mips and not one more.
						 *
						 * P = side/16 IS his two numbers: an 8 x 8 grid of 128-texel frames is a
						 * 1024 sheet and gets 8, a grid of 256-texel frames is a 2048 sheet and
						 * gets 16. Applied PER AXIS, so the gutter is the same fraction of a
						 * short side as of a long one and a narrow frame does not spend a
						 * quarter of itself on margin. Floored at 2, so every card ships at
						 * least two clean mips; rounded UP TO EVEN, so --card-half-aux (which
						 * halves both sides) still lands the aux gutter on a whole texel.
						 *
						 * WHAT THIS REPLACES: one gutter G = max(4, tileLong/16) on all four
						 * sides, and a chain that ran until a frame's SHORTER side spanned eight
						 * texels. That pair shipped a BLEEDING level on every 128-texel card --
						 * measured on the 19-tree Sanctuary library: mip 4, gutter half a texel,
						 * 26/255 of a neighbour's alpha across 16 of 28 frame borders. */
						auto padOf = []( int side ) {
							const int p = qMax( 2, side / 16 );
							return p + ( p & 1 );		// even, so a half-aux gutter is a whole texel
						};
"""

NEW_LAW = """						/* THE GAP, PER AXIS, AND THE MIPS IT BUYS (bungo, 2026-09-09, correcting
						 * the reading of the same day).
						 *
						 * His number names the DISTANCE BETWEEN TWO RENDERED OBJECTS, verbatim:
						 * "When I say padding 8 for 1k, it's 8 pixels of distance between two
						 * rendered objects." So on a 1024 sheet of 8 x 8 frames, two neighbouring
						 * silhouettes are 8 texels apart ACROSS the frame border they share --
						 * which is 4 texels of margin on each side of it, not 8. The lane before
						 * this one read the number as the per-side margin and spent twice the
						 * texels (MISTAKES.md, 2026-09-09).
						 *
						 *   gap(side) = max(2, side / 16), rounded UP to even
						 *   pad(side) = gap(side) / 2                 // on EACH side of a frame
						 *
						 * so the inner rect is `side - gap`: 15/16 of the frame wherever the side
						 * is a multiple of 32, which is his 8-on-1024 and 16-on-2k exactly.
						 *
						 * THE MIPS THE GAP BUYS. Mip CONSTRUCTION never mixes frames (a frame side
						 * stays even all the way down); the bleed is at SAMPLE time, where a tap on
						 * a frame's own UV border reads half of that frame's last texel and half of
						 * the neighbour's first. What separates the two silhouettes at level k is
						 * the gap measured at level k, gap / 2^k, and the sheet ships every level
						 * whose gap is still at least one whole texel:
						 *
						 *   mips = 1 + log2( min( gapX, gapY ) )
						 *
						 * A 128-texel frame therefore ships 4 levels (128, 64, 32, 16) -- the same
						 * count the per-side reading gave, for half the padding, because the count
						 * was always the gap's and the gap has not changed.
						 *
						 * THE SHEET'S OUTER BORDER needs only HALF a gap: there is no neighbouring
						 * frame beyond it. Padding every frame by gap/2 gives exactly that, so no
						 * special case is needed -- an interior border carries gap/2 from each of
						 * the two frames that meet on it, an outer border carries gap/2 and faces
						 * the sheet edge. That holds because the sheet is sampled CLAMPED: the card
						 * quad's UV rect is a sub-rect of the sheet, and neither the DDS nor the
						 * .lodm asks for wrapping. Under WRAP the outer border would face the
						 * opposite edge's frames and would need a whole gap.
						 *
						 * Floored at 2 so every card ships at least two levels; rounded UP TO EVEN
						 * so the gap splits into two whole texels of margin. */
						auto gapOf = []( int side ) {
							const int g = qMax( 2, side / 16 );
							return g + ( g & 1 );		// even, so the gap splits into two whole texels
						};
						auto padOf = [gapOf]( int side ) { return gapOf( side ) / 2; };
"""

assert s.count(OLD_LAW) == 1, 'law block: %d' % s.count(OLD_LAW)
s = s.replace(OLD_LAW, NEW_LAW)

OLD_PAD = """						const int padX = padOf( tw ), padY = padOf( th );
						const int iw = tw - 2 * padX, ih = th - 2 * padY;
"""
NEW_PAD = """						const int padX = padOf( tw ), padY = padOf( th );
						// the gap the sidecar records: what a mip cap and a reader are told
						const int gapX = gapOf( tw ), gapY = gapOf( th );
						const int iw = tw - 2 * padX, ih = th - 2 * padY;
"""
assert s.count(OLD_PAD) == 1, 'padX line: %d' % s.count(OLD_PAD)
s = s.replace(OLD_PAD, NEW_PAD)

OLD_META = """						/* THE PADDING, on a line of its own, because the mip cap is DERIVED from
						 * it and a reader must not have to re-guess the law that produced the
						 * sheet. `pad <x> <y>`, in texels, on EACH side of every frame. A bake
						 * from before this line has no `pad`, and lodgenCard falls back to the
						 * old max(4, longSide/16) on both axes -- which is what those sheets were
						 * actually written with. */
						ms << "pad " << padX << " " << padY << "\\n";
"""
NEW_META = """						/* THE GAP, on a line of its own, because the mip cap is DERIVED from it
						 * and a reader must not have to re-guess the law that produced the sheet.
						 * `gap <x> <y>`, in texels, is the distance between two neighbouring
						 * SILHOUETTES across a frame border -- bungo's own quantity -- and the
						 * margin on each side of a frame is half of it.
						 *
						 * Two older sidecars still read: one with a `pad <x> <y>` line (2026-09-09,
						 * lane CARDFIT3) carries a PER-SIDE number written under the law
						 * mips = 1 + log2(pad), and one with neither line falls back to
						 * max(4, longSide/16) per side under that same older law. lodgenCard keeps
						 * both paths so those sheets still convert to exactly the chains they were
						 * built for. */
						ms << "gap " << gapX << " " << gapY << "\\n";
"""
assert s.count(OLD_META) == 1, 'meta line: %d' % s.count(OLD_META)
s = s.replace(OLD_META, NEW_META)

nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0, 'CR moved: %d -> %d' % (cr0, nb.count(b'\r'))
open(P, 'wb').write(nb)
print('fix1 ok: %d -> %d bytes, CR %d' % (len(b), len(nb), cr0))
