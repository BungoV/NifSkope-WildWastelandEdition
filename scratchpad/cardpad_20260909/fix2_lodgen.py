#!/usr/bin/env python3
"""CARDPAD 2/2 -- lodgen.cpp: the card reader, the mip cap and the .lodm fields
move from the per-side padding to the GAP between two neighbouring silhouettes."""

P = 'src/lodgen.cpp'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')

def sub(old, new, what):
    global s
    n = s.count(old)
    assert n == 1, '%s: %d occurrences' % (what, n)
    s = s.replace(old, new)

# ---------------------------------------------------------------- 1. the struct
sub("""	//! The gutter the bake left on EACH side of a frame, per axis, in texels
	//! (the meta's `pad` line). 0 = a bake from before the line; the reader
	//! then falls back to the old max(4, longSide/16) on both axes, which is
	//! what those sheets really carry. The MIP CAP is derived from it:
	//! a sample on a frame's UV border reaches half a texel into the next
	//! frame, so a level is clean only while its gutter is a whole texel,
	//! pad / 2^k >= 1, and the sheet ships 1 + log2(min(padX, padY)) mips.
	int octPadX = 0, octPadY = 0;
""", """	//! The GAP between two neighbouring silhouettes across a frame border, per
	//! axis, in texels -- the meta's `gap` line, and bungo's own quantity
	//! ("8 pixels of distance between two rendered objects", 2026-09-09).
	//! 0 = a sidecar from before the line.
	int octGapX = 0, octGapY = 0;
	//! The gutter on EACH side of a frame, per axis, in texels: half the gap on
	//! a `gap` sidecar, the literal number on an older `pad` one, and
	//! max(4, longSide/16) on a sidecar with neither.
	int octPadX = 0, octPadY = 0;
	//! What the MIP CAP divides, min over the two axes. It is the GAP under the
	//! law of 2026-09-09 -- a tap on a frame's UV border reads half of that
	//! frame's last texel and half of the neighbour's first, so what separates
	//! the two silhouettes at level k is gap / 2^k and the chain stops at the
	//! last level where that is still a whole texel -- and it is the PER-SIDE
	//! padding on a sidecar written under the reading before it, so those sheets
	//! still convert to exactly the chain they were built for.
	//! mips = 1 + log2(octMipUnit).
	int octMipUnit = 0;
""", 'struct fields')

# ---------------------------------------------------------------- 2. the meta reader
sub("""			} else if ( line[0] == QLatin1String( "pad" ) && line.size() >= 3 ) {
				// the gutter, per axis, on each side of a frame -- the mip cap's input
				card.octPadX = line[1].toInt();
				card.octPadY = line[2].toInt();
""", """			} else if ( line[0] == QLatin1String( "gap" ) && line.size() >= 3 ) {
				/* THE GAP between two neighbouring silhouettes across a frame border,
				 * per axis (bungo, 2026-09-09). The margin on each side is half of it,
				 * and the mip cap divides the gap itself. */
				card.octGapX = qMax( 2, line[1].toInt() );
				card.octGapY = qMax( 2, line[2].toInt() );
				card.octPadX = card.octGapX / 2;
				card.octPadY = card.octGapY / 2;
				card.octMipUnit = qMin( card.octGapX, card.octGapY );
			} else if ( line[0] == QLatin1String( "pad" ) && line.size() >= 3 ) {
				/* A sidecar from lane CARDFIT3 (2026-09-09, superseded the same day):
				 * the number is the PER-SIDE margin and its chain was capped at
				 * 1 + log2(pad). Read under its own law so it still converts to the
				 * sheet it was baked for. */
				card.octPadX = line[1].toInt();
				card.octPadY = line[2].toInt();
				card.octGapX = 2 * card.octPadX;
				card.octGapY = 2 * card.octPadY;
				card.octMipUnit = qMin( card.octPadX, card.octPadY );
""", 'meta reader')

# ---------------------------------------------------------------- 3. the mip cap
sub("""				/* THE MIP CAP IS THE PADDING'S (bungo, 2026-09-09: "enough pixel
				 * padding so that there's no mip map bleeding into other rows and
				 * columns"). A frame's mips never mix ACROSS a border -- the box
				 * filter halves an even frame into an even frame -- but a reader
				 * sampling ON a frame's UV border takes half its value from the
				 * next frame, so a level is clean only while its gutter is still a
				 * whole texel: pad / 2^k >= 1. Hence 1 + log2(min(padX, padY))
				 * levels, and never the one after.
				 *
				 * The old cap ran the chain until a frame's SHORTER side spanned
				 * eight texels, which had nothing to do with the gutter and shipped
				 * one bleeding level on every 128-texel card: measured 26/255 of a
				 * neighbour's alpha across 16 of 28 borders at mip 4. */
				const int padFallback = qMax( 4, qMax( card.octTileW, card.octTileH ) / 16 );
				const int padX = card.octPadX > 0 ? card.octPadX : padFallback;
				const int padY = card.octPadY > 0 ? card.octPadY : padFallback;
				int frameMips = 1;
				for ( int g = qMin( padX, padY ); g >= 2; g /= 2 )
					frameMips++;
""", """				/* THE MIP CAP IS THE GAP'S (bungo, 2026-09-09: "enough pixel padding so
				 * that there's no mip map bleeding into other rows and columns", with
				 * the number named the same day as "8 pixels of distance between two
				 * rendered objects"). A frame's mips never mix ACROSS a border -- the
				 * box filter halves an even frame into an even frame -- but a reader
				 * sampling ON a frame's UV border takes half its value from the next
				 * frame, so what has to survive is the SEPARATION between the two
				 * silhouettes: gap / 2^k >= 1. Hence 1 + log2(min(gapX, gapY)) levels,
				 * and never the one after -- 4 on a 128-texel frame, which is the count
				 * the per-side reading also gave, because the count was always the
				 * gap's.
				 *
				 * `octMipUnit` is the gap on a sidecar that names one and the per-side
				 * padding on the two older kinds, each read under its own law. The last
				 * fallback -- neither line -- is the pre-2026-09-09 sheets' own
				 * max(4, longSide/16) per side, capped as those sheets were capped. */
				const int padFallback = qMax( 4, qMax( card.octTileW, card.octTileH ) / 16 );
				const int padX = card.octPadX > 0 ? card.octPadX : padFallback;
				const int padY = card.octPadY > 0 ? card.octPadY : padFallback;
				const int gapX = card.octGapX > 0 ? card.octGapX : 2 * padFallback;
				const int gapY = card.octGapY > 0 ? card.octGapY : 2 * padFallback;
				const int mipUnit = card.octMipUnit > 0 ? card.octMipUnit : padFallback;
				int frameMips = 1;
				for ( int g = mipUnit; g >= 2; g /= 2 )
					frameMips++;
""", 'mip cap')

# ---------------------------------------------------------------- 4. the aux mips
sub("""				/* The aux sheets' gutter came down by auxDiv with everything else,
				 * so their clean depth does too. The bake rounds the padding UP TO
				 * EVEN precisely so this division lands on a whole texel. */
				int auxMips = 1;
				for ( int g = qMin( padX, padY ) / auxDiv; g >= 2; g /= 2 )
					auxMips++;
""", """				/* The aux sheets' gap came down by auxDiv with everything else, so their
				 * clean depth does too. The bake rounds the gap UP TO EVEN, so a halved
				 * frame still splits it into two whole texels down to gap 2; below that
				 * -- the 16- and 32-texel frames at --card-half-aux -- the division
				 * reaches 1 and the aux sheets ship a single level, which is the
				 * fallback naming itself rather than a silent bleed. */
				int auxMips = 1;
				for ( int g = mipUnit / auxDiv; g >= 2; g /= 2 )
					auxMips++;
""", 'aux mips')

# ---------------------------------------------------------------- 5. the card .lodm
sub("""					/* The gutter, per axis, in texels on EACH side of a frame. A
					 * reader needs it to know which part of a frame is picture: the
					 * silhouette occupies the INNER rect, `half` still spans the
					 * whole frame, and `mips` is 1 + log2(min(pad)) by construction. */
					oc.insert( QStringLiteral( "pad" ), QJsonArray{ padX, padY } );
""", """					/* THE PADDING IS THE PER-SIDE NUMBER, and `gap` is the distance
					 * between two neighbouring silhouettes across a frame border --
					 * bungo's own quantity, and exactly twice it. A reader needs `pad`
					 * to know which part of a frame is picture (the silhouette occupies
					 * the INNER rect, `frame - 2*pad`, while `half` still spans the whole
					 * frame) and `gap` to know the law the mip count came from,
					 * 1 + log2(min(gap)). A set that carries `pad` and no `gap` is a
					 * CARDFIT3 set whose count was 1 + log2(min(pad)). */
					oc.insert( QStringLiteral( "pad" ), QJsonArray{ padX, padY } );
					oc.insert( QStringLiteral( "gap" ), QJsonArray{ gapX, gapY } );
""", 'card lodm')

# ---------------------------------------------------------------- 6. the card ARRAY
sub("""		/* The gutter, per axis, as the set's own .lodm records it. An array is
		 * built from the same PNGs and the same dilation as the per-card set,
		 * so it inherits that set's padding and therefore that set's clean mip
		 * depth; a set from before the `pad` key falls back to the law those
		 * sheets were written under. */
		const QJsonArray padA = card.value( QStringLiteral( "pad" ) ).toArray();
		const int padFallback = qMax( 4, qMax( fw, fh ) / 16 );
		const int padX = padA.size() == 2 ? padA[0].toInt() : padFallback;
		const int padY = padA.size() == 2 ? padA[1].toInt() : padFallback;
""", """		/* The gutter and the GAP, per axis, as the set's own .lodm records them. An
		 * array is built from the same PNGs and the same dilation as the per-card set,
		 * so it inherits that set's spacing and therefore that set's clean mip depth.
		 * Three vintages, each read under the law it was written under: `gap` present
		 * (2026-09-09, the gap law, mips = 1 + log2(min(gap))); `pad` alone (lane
		 * CARDFIT3, per-side, mips = 1 + log2(min(pad))); neither (older still,
		 * max(4, longSide/16) per side under that same per-side law). */
		const QJsonArray padA = card.value( QStringLiteral( "pad" ) ).toArray();
		const QJsonArray gapA = card.value( QStringLiteral( "gap" ) ).toArray();
		const int padFallback = qMax( 4, qMax( fw, fh ) / 16 );
		const int padX = padA.size() == 2 ? padA[0].toInt() : padFallback;
		const int padY = padA.size() == 2 ? padA[1].toInt() : padFallback;
		const int gapX = gapA.size() == 2 ? gapA[0].toInt() : 2 * padX;
		const int gapY = gapA.size() == 2 ? gapA[1].toInt() : 2 * padY;
		const int mipUnit = gapA.size() == 2 ? qMin( gapX, gapY ) : qMin( padX, padY );
""", 'array pad read')

sub("""		g.padX = padX;
		g.padY = padY;
		// the clean depth: a whole texel of gutter at the deepest level sampled
		g.mips = 1;
		for ( int g2 = qMin( padX, padY ); g2 >= 2; g2 /= 2 )
			g.mips++;
""", """		g.padX = padX;
		g.padY = padY;
		g.gapX = gapX;
		g.gapY = gapY;
		// the clean depth: a whole texel of GAP between the two silhouettes that meet
		// on a border, at the deepest level sampled
		g.mips = 1;
		for ( int g2 = mipUnit; g2 >= 2; g2 /= 2 )
			g.mips++;
""", 'array mips')

sub("""		g.auxMips = 1;
		for ( int g2 = qMin( padX, padY ) / auxDiv; g2 >= 2; g2 /= 2 )
			g.auxMips++;
""", """		g.auxMips = 1;
		for ( int g2 = mipUnit / auxDiv; g2 >= 2; g2 /= 2 )
			g.auxMips++;
""", 'array aux mips')

sub("""	struct Group { bool pbr = false; int w = 0, h = 0, aw = 0, ah = 0, oct = 0, fw = 0, fh = 0, padX = 0, padY = 0, mips = 1, auxMips = 1; QVector<Layer> layers;""",
    """	struct Group { bool pbr = false; int w = 0, h = 0, aw = 0, ah = 0, oct = 0, fw = 0, fh = 0, padX = 0, padY = 0, gapX = 0, gapY = 0, mips = 1, auxMips = 1; QVector<Layer> layers;""",
    'Group struct')

sub("""		arr.insert( QStringLiteral( "pad" ), QJsonArray{ g.padX, g.padY } );
""", """		arr.insert( QStringLiteral( "pad" ), QJsonArray{ g.padX, g.padY } );
		// the distance between two neighbouring silhouettes, shared: the mip cap's own input
		arr.insert( QStringLiteral( "gap" ), QJsonArray{ g.gapX, g.gapY } );
""", 'array lodm gap')

nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0, 'CR moved: %d -> %d' % (cr0, nb.count(b'\r'))
open(P, 'wb').write(nb)
print('fix2 ok: %d -> %d bytes, CR %d' % (len(b), len(nb), cr0))
