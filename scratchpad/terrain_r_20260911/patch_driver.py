p = 'src/lodgen.cpp'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:80], s.count(a))
    s = s.replace(a, b)


# ---- the estimator's per-tile byte count ----
rep("""static qint64 lodgenVtTileBytes( int stored, int mips, bool coverTile, bool withHeight )
{
	qint64 n = 0;
	for ( int m = 0; m < mips; m++ ) {
		const qint64 s = stored >> m;
		const qint64 blocks = ( s / 4 ) * ( s / 4 );
		n += blocks * 8;                            // colour, BC1
		n += blocks * 8;                            // msn, BC1
		n += blocks * ( coverTile ? 16 : 8 );       // data, BC3 only with cover
		if ( withHeight )
			n += s * s * 2;                         // height, R16 uncompressed
	}
	return n;
}""",
"""static qint64 lodgenVtTileBytes( int stored, int mips, bool coverTile, bool withHeight,
	bool withEmissive = false, bool coverInColor = false )
{
	qint64 n = 0;
	for ( int m = 0; m < mips; m++ ) {
		const qint64 s = stored >> m;
		const qint64 blocks = ( s / 4 ) * ( s / 4 );
		/* EXACTLY ONE sheet carries the ground-cover alpha and it is the only
		 * one that doubles. By default it is the mask (bungo's open question;
		 * `--vt-cover-in-color` is the other arm and makes the COLOUR sheet the
		 * BC3 one instead). */
		n += blocks * ( ( coverTile && coverInColor ) ? 16 : 8 );    // colour
		n += blocks * 8;                                            // msn, BC1
		n += blocks * ( ( coverTile && !coverInColor ) ? 16 : 8 );  // mask
		if ( withHeight )
			n += s * s * 2;                         // height, R16 uncompressed
		if ( withEmissive )
			n += blocks * 8;                        // emissive, BC1, no alpha
	}
	return n;
}""")

rep("""	const qint64 rawNoCover = lodgenVtTileBytes( stored, opts.mips, false, opts.height );
	const qint64 rawCover = lodgenVtTileBytes( stored, opts.mips, true, opts.height );""",
"""	const qint64 rawNoCover = lodgenVtTileBytes( stored, opts.mips, false, opts.height,
		false, opts.coverInColor );
	const qint64 rawCover = lodgenVtTileBytes( stored, opts.mips, true, opts.height,
		false, opts.coverInColor );""")

# ---- the writers' header ----
rep("""		h.mipCount = quint8( mips );
		h.sheetCount = opts.height ? 4 : 3;""",
"""		h.mipCount = quint8( mips );
		/* THE SHEET SET (2.2, version 2): colour, msn, mask, then HEIGHT if it
		 * was asked for, then EMISSIVE if any layer in this bake supplies one.
		 * The order here is the order the payload concatenates them in, so the
		 * two are written from one place and cannot drift. */
		h.sheetCount = quint8( 3 + ( opts.height ? 1 : 0 ) + ( wantEmissive ? 1 : 0 ) );""")

rep("""		h.sheets[0] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC1_UNORM, LODV_ROLE_COLOR, 1 };
		h.sheets[1] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC1_UNORM, LODV_ROLE_MSN, 0 };
		h.sheets[2] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC3_UNORM, LODV_ROLE_DATA, 0 };
		if ( opts.height )
			h.sheets[3] = { LODV_DXGI_R16_UNORM, LODV_DXGI_R16_UNORM, LODV_ROLE_HEIGHT, 0 };""",
"""		const quint16 colorCoverFmt = opts.coverInColor ? LODV_DXGI_BC3_UNORM : LODV_DXGI_BC1_UNORM;
		const quint16 maskCoverFmt = opts.coverInColor ? LODV_DXGI_BC1_UNORM : LODV_DXGI_BC3_UNORM;
		h.sheets[0] = { LODV_DXGI_BC1_UNORM, colorCoverFmt, LODV_ROLE_COLOR, 1 };
		h.sheets[1] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC1_UNORM, LODV_ROLE_MSN, 0 };
		h.sheets[2] = { LODV_DXGI_BC1_UNORM, maskCoverFmt, LODV_ROLE_MASK, 0 };
		int nextSheet = 3;
		if ( opts.height )
			h.sheets[nextSheet++] = { LODV_DXGI_R16_UNORM, LODV_DXGI_R16_UNORM, LODV_ROLE_HEIGHT, 0 };
		if ( wantEmissive )
			h.sheets[nextSheet++] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC1_UNORM, LODV_ROLE_EMISSIVE, 0 };""")

# ---- the emissive pre-pass, before the writers open ----
rep("""	const quint64 vhgtHash = world.vhgtCorpusHash();
	const quint64 paintCorpusHashUNUSED = 0;""", """""", 0)

rep("""	const quint64 vhgtHash = world.vhgtCorpusHash();
	const quint64 paintHash = world.paintCorpusHash();

	std::vector<std::unique_ptr<LodvWriter>> writers;""",
"""	const quint64 vhgtHash = world.vhgtCorpusHash();
	const quint64 paintHash = world.paintCorpusHash();

	/* THE EMISSIVE SHEET IS DECIDED BEFORE ANY CONTAINER OPENS, because its
	 * presence is a HEADER field and a tile's payload size depends on it.
	 *
	 * bungo's ruling, 2026-09-11 09:5x: an EMISSIVE sheet "when any layer
	 * supplies one (absent = none, named in the index)". So every LTEX the
	 * bake's own rectangle paints is resolved once, here, through the same
	 * cache the tiles then reuse -- no layer is read twice and the pass costs
	 * one walk of the region's LAND records. A worldspace whose landscape
	 * names no emissive map writes NO emissive sheet at all, which is the
	 * fallback, and the index says so in words rather than shipping a black
	 * sheet nobody can tell from a missing one. */
	LodgenVtMaskCache maskCache;
	bool wantEmissive = false;
	int layerFormsSeen = 0;
	{
		EsmLand land;
		for ( int cy = levels[0].south; cy <= levels[0].north; cy++ ) {
			for ( int cx = levels[0].west; cx <= levels[0].east; cx++ ) {
				if ( !world.land( cx, cy, land ) )
					continue;
				for ( int q = 0; q < 4; q++ ) {
					if ( land.baseTex[q] ) {
						maskCache.resolve( world, dataRoot, land.baseTex[q] );
						layerFormsSeen++;
					}
					for ( const EsmLandLayer & layer : land.layers[q] ) {
						if ( !layer.ltex )
							continue;
						maskCache.resolve( world, dataRoot, layer.ltex );
						layerFormsSeen++;
					}
				}
			}
		}
		wantEmissive = maskCache.withEmissive > 0;
	}

	std::vector<std::unique_ptr<LodvWriter>> writers;""")

# ---- writeTile / bake / filter call sites ----
rep("""		const QByteArray raw = lodgenVtEncodeTile( st, stored, mips, opts.height );""",
"""		const QByteArray raw = lodgenVtEncodeTile( st, stored, mips, opts.height,
			wantEmissive, opts.coverInColor );""")

rep("""				lodgenVtFilterTile( rings[size_t( lv )], levels[lv].tilesX, levels[lv].tilesY,
					content, border, stored, tx, p, prow[size_t( tx )] );""",
"""				lodgenVtFilterTile( rings[size_t( lv )], levels[lv].tilesX, levels[lv].tilesY,
					content, border, stored, tx, p, wantEmissive, prow[size_t( tx )] );""")

rep("""			if ( !lodgenBakeVtTile( world, dataRoot, bc, opts.cover, landCache,
				cellX0, cellY0, levels[0].dim, content, border, row[size_t( tx )] ) )""",
"""			if ( !lodgenBakeVtTile( world, dataRoot, bc, opts.cover, landCache, maskCache,
				wantEmissive, cellX0, cellY0, levels[0].dim, content, border, row[size_t( tx )] ) )""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
